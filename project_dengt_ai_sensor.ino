// Combined AI + HC-SR04 traffic light controller.
//
// Physical light wiring:
// Phase 1 light controls Road 1 + Road 3:
//   D2 -> R, D3 -> Y, D4 -> G
// Phase 2 light controls Road 2 + Road 4:
//   D5 -> R, D6 -> Y, D7 -> G
//
// HC-SR04 sensors:
// Road 1: TRIG A0, ECHO D8
// Road 2: TRIG A1, ECHO D9
// Road 3: TRIG A2, ECHO D10
// Road 4: TRIG A3, ECHO D11
//
// AI serial protocol:
//   LEVELS,r1,r2,r3,r4
//   BLOCKED,0/1/2/3
//
// Behavior:
//   AI online  -> AI controls the light, sensors cross-check it.
//   AI offline -> sensors control the light.
//   Camera sees but sensor does not -> all red + ALERT,CAM_THAY_CAM_BIEN_KHONG,...
//   Sensor sees but camera does not -> ALERT,CAM_KHONG_THAY_CAM_BIEN_THAY,...

const byte ROAD_COUNT = 4;
const byte PHASE_COUNT = 2;

const byte PHASE1 = 0;
const byte PHASE2 = 1;
const byte NO_PHASE = 255;

const byte R_PINS[PHASE_COUNT] = {2, 5};
const byte Y_PINS[PHASE_COUNT] = {3, 6};
const byte G_PINS[PHASE_COUNT] = {4, 7};
const byte ROAD_TO_PHASE[ROAD_COUNT] = {PHASE1, PHASE2, PHASE1, PHASE2};

const byte ULTRASONIC_TRIG_PINS[ROAD_COUNT] = {A0, A1, A2, A3};
const byte ULTRASONIC_ECHO_PINS[ROAD_COUNT] = {8, 9, 10, 11};

const bool CAMERA_ONLY_FORCES_ALL_RED = true;

const unsigned long LOW_GREEN_MS = 5000;
const unsigned long MEDIUM_GREEN_MS = 10000;
const unsigned long HIGH_GREEN_MS = 15000;
const unsigned long MIN_GREEN_MS = 2000;
const unsigned long CLEAR_GRACE_MS = 1000;
const unsigned long YELLOW_MS = 2000;
const unsigned long IDLE_POLL_MS = 100;
const unsigned long AI_TIMEOUT_MS = 15000;
const unsigned long BLOCKED_TIMEOUT_MS = 3000;
const unsigned long SENSOR_REFRESH_MS = 150;
const unsigned long ALERT_REPEAT_MS = 3000;
const unsigned int ULTRASONIC_TIMEOUT_US = 6000;
const int ULTRASONIC_MIN_CM = 3;
const int ULTRASONIC_DETECT_CM = 15;

int aiRoadLevels[ROAD_COUNT] = {0, 0, 0, 0};
int sensorLevels[ROAD_COUNT] = {0, 0, 0, 0};
int sensorDistancesCm[ROAD_COUNT] = {999, 999, 999, 999};

unsigned long lastAiUpdateMs = 0;
unsigned long lastBlockedUpdateMs = 0;
unsigned long lastSensorReadMs = 0;
unsigned long lastAlertSentMs = 0;

byte blockedOwnerPhase = 0;
byte lastAlertType = 0;
byte lastAlertCameraOnlyMask = 0;
byte lastAlertSensorOnlyMask = 0;
byte lastServedPhase = PHASE2;

char serialBuffer[64];
byte serialIndex = 0;

void setup() {
  Serial.begin(115200);

  for (byte phase = 0; phase < PHASE_COUNT; phase++) {
    pinMode(R_PINS[phase], OUTPUT);
    pinMode(Y_PINS[phase], OUTPUT);
    pinMode(G_PINS[phase], OUTPUT);
  }

  for (byte road = 0; road < ROAD_COUNT; road++) {
    pinMode(ULTRASONIC_TRIG_PINS[road], OUTPUT);
    pinMode(ULTRASONIC_ECHO_PINS[road], INPUT);
    digitalWrite(ULTRASONIC_TRIG_PINS[road], LOW);
  }

  allRed();
  Serial.println("READY AI_SENSOR");
  Serial.println("Lights: phase1=D2,D3,D4 phase2=D5,D6,D7");
  Serial.println("Road mapping: R1,R3 -> phase1; R2,R4 -> phase2");
  Serial.println("Ultrasonic TRIG: A0,A1,A2,A3 ECHO: D8,D9,D10,D11");
  Serial.println("Send: LEVELS,0,1,2,3");
}

void loop() {
  readSerialCommands();
  refreshSensors();

  if (aiLevelsFresh() && crossCheckNeedsAllRed()) {
    allRed();
    waitWithInputs(IDLE_POLL_MS);
    return;
  }

  int phase1Level = getEffectivePhaseLevel(PHASE1);
  int phase2Level = getEffectivePhaseLevel(PHASE2);

  if (phase1Level == 0 && phase2Level == 0) {
    allRed();
    waitWithInputs(IDLE_POLL_MS);
    return;
  }

  byte selectedPhase = choosePhaseToRun(phase1Level, phase2Level);
  if (selectedPhase == PHASE1) {
    if (runPhase(PHASE1, phase1Level, 1)) {
      lastServedPhase = PHASE1;
    }
  } else if (selectedPhase == PHASE2) {
    if (runPhase(PHASE2, phase2Level, 2)) {
      lastServedPhase = PHASE2;
    }
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char incoming = (char)Serial.read();

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      if (serialIndex > 0) {
        serialBuffer[serialIndex] = '\0';
        handleSerialCommand(serialBuffer);
        serialIndex = 0;
      }
      continue;
    }

    if (serialIndex < sizeof(serialBuffer) - 1) {
      serialBuffer[serialIndex++] = incoming;
    }
  }
}

void handleSerialCommand(char *command) {
  char *token = strtok(command, ", ");
  if (token == NULL) {
    return;
  }

  if (equalsIgnoreCase(token, "PING")) {
    Serial.println("PONG");
    return;
  }

  if (equalsIgnoreCase(token, "SENSORS") || equalsIgnoreCase(token, "HEALTH")) {
    forceRefreshSensors();
    printSensorHealth();
    return;
  }

  if (equalsIgnoreCase(token, "ALLRED")) {
    clearAiLevels();
    allRed();
    Serial.println("OK ALLRED");
    return;
  }

  if (equalsIgnoreCase(token, "BLOCKED")) {
    token = strtok(NULL, ", ");
    if (token == NULL) {
      Serial.println("ERR expected BLOCKED,0_to_3");
      return;
    }
    blockedOwnerPhase = clampBlockedOwner(atoi(token));
    lastBlockedUpdateMs = millis();
    Serial.print("OK BLOCKED ");
    Serial.println(blockedOwnerPhase);
    return;
  }

  if (!equalsIgnoreCase(token, "LEVELS")) {
    Serial.println("ERR expected LEVELS,r1,r2,r3,r4");
    return;
  }

  int newLevels[ROAD_COUNT] = {0, 0, 0, 0};
  for (byte road = 0; road < ROAD_COUNT; road++) {
    token = strtok(NULL, ", ");
    if (token == NULL) {
      Serial.println("ERR expected LEVELS,r1,r2,r3,r4");
      return;
    }
    newLevels[road] = clampLevel(atoi(token));
  }

  for (byte road = 0; road < ROAD_COUNT; road++) {
    aiRoadLevels[road] = newLevels[road];
  }
  lastAiUpdateMs = millis();

  Serial.print("OK LEVELS ");
  for (byte road = 0; road < ROAD_COUNT; road++) {
    Serial.print(aiRoadLevels[road]);
    if (road < ROAD_COUNT - 1) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

bool equalsIgnoreCase(const char *left, const char *right) {
  while (*left && *right) {
    char leftChar = *left;
    char rightChar = *right;

    if (leftChar >= 'a' && leftChar <= 'z') {
      leftChar -= 32;
    }
    if (rightChar >= 'a' && rightChar <= 'z') {
      rightChar -= 32;
    }
    if (leftChar != rightChar) {
      return false;
    }

    left++;
    right++;
  }

  return *left == '\0' && *right == '\0';
}

int clampLevel(int level) {
  if (level < 0) {
    return 0;
  }
  if (level > 3) {
    return 3;
  }
  return level;
}

byte clampBlockedOwner(int owner) {
  if (owner < 0) {
    return 0;
  }
  if (owner > 3) {
    return 3;
  }
  return (byte)owner;
}

void clearAiLevels() {
  for (byte road = 0; road < ROAD_COUNT; road++) {
    aiRoadLevels[road] = 0;
  }
}

bool aiLevelsFresh() {
  return lastAiUpdateMs > 0 && millis() - lastAiUpdateMs <= AI_TIMEOUT_MS;
}

void refreshSensors() {
  unsigned long now = millis();
  if (lastSensorReadMs > 0 && now - lastSensorReadMs < SENSOR_REFRESH_MS) {
    return;
  }

  lastSensorReadMs = now;
  for (byte road = 0; road < ROAD_COUNT; road++) {
    int distanceCm = readUltrasonicCm(ULTRASONIC_TRIG_PINS[road], ULTRASONIC_ECHO_PINS[road]);
    sensorDistancesCm[road] = distanceCm;
    sensorLevels[road] = distanceCm >= ULTRASONIC_MIN_CM && distanceCm <= ULTRASONIC_DETECT_CM ? 1 : 0;
  }
}

void forceRefreshSensors() {
  lastSensorReadMs = 0;
  refreshSensors();
}

int readUltrasonicCm(byte trigPin, byte echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long durationUs = pulseIn(echoPin, HIGH, ULTRASONIC_TIMEOUT_US);
  if (durationUs == 0) {
    return 999;
  }

  return (int)(durationUs / 58);
}

int getEffectiveRoadLevel(byte road) {
  if (aiLevelsFresh()) {
    return aiRoadLevels[road];
  }
  return sensorLevels[road];
}

int getEffectivePhaseLevel(byte phase) {
  int level = 0;
  for (byte road = 0; road < ROAD_COUNT; road++) {
    if (ROAD_TO_PHASE[road] == phase) {
      level += getEffectiveRoadLevel(road);
    }
  }
  if (level > 3) {
    return 3;
  }
  return level;
}

byte presentMaskFromLevels(const int levels[]) {
  byte mask = 0;
  for (byte road = 0; road < ROAD_COUNT; road++) {
    if (levels[road] > 0) {
      mask |= (1 << road);
    }
  }
  return mask;
}

bool crossCheckNeedsAllRed() {
  byte cameraMask = presentMaskFromLevels(aiRoadLevels);
  byte sensorMask = presentMaskFromLevels(sensorLevels);
  byte cameraOnlyMask = cameraMask & ~sensorMask;
  byte sensorOnlyMask = sensorMask & ~cameraMask;

  if (cameraOnlyMask == 0 && sensorOnlyMask == 0) {
    lastAlertType = 0;
    return false;
  }

  byte alertType = 0;
  if (cameraOnlyMask > 0 && sensorOnlyMask > 0) {
    alertType = 3;
  } else if (cameraOnlyMask > 0) {
    alertType = 1;
  } else {
    alertType = 2;
  }

  sendCrossCheckAlert(alertType, cameraOnlyMask, sensorOnlyMask, cameraMask, sensorMask);
  return CAMERA_ONLY_FORCES_ALL_RED && cameraOnlyMask > 0;
}

void sendCrossCheckAlert(byte alertType, byte cameraOnlyMask, byte sensorOnlyMask, byte cameraMask, byte sensorMask) {
  unsigned long now = millis();
  bool changed = alertType != lastAlertType
    || cameraOnlyMask != lastAlertCameraOnlyMask
    || sensorOnlyMask != lastAlertSensorOnlyMask;

  if (!changed && now - lastAlertSentMs < ALERT_REPEAT_MS) {
    return;
  }

  lastAlertType = alertType;
  lastAlertCameraOnlyMask = cameraOnlyMask;
  lastAlertSensorOnlyMask = sensorOnlyMask;
  lastAlertSentMs = now;

  Serial.print("ALERT,");
  if (alertType == 1) {
    Serial.print("CAM_THAY_CAM_BIEN_KHONG");
  } else if (alertType == 2) {
    Serial.print("CAM_KHONG_THAY_CAM_BIEN_THAY");
  } else {
    Serial.print("DU_LIEU_KHONG_KHOP");
  }
  Serial.print(',');
  Serial.print(cameraOnlyMask);
  Serial.print(',');
  Serial.print(sensorOnlyMask);
  Serial.print(',');
  Serial.print(cameraMask);
  Serial.print(',');
  Serial.println(sensorMask);
}

byte choosePhaseToRun(int phase1Level, int phase2Level) {
  if (phase1Level <= 0 && phase2Level <= 0) {
    return NO_PHASE;
  }
  if (phase1Level > 0 && phase2Level <= 0) {
    return PHASE1;
  }
  if (phase2Level > 0 && phase1Level <= 0) {
    return PHASE2;
  }
  if (phase1Level > phase2Level) {
    return PHASE1;
  }
  if (phase2Level > phase1Level) {
    return PHASE2;
  }
  return lastServedPhase == PHASE1 ? PHASE2 : PHASE1;
}

unsigned long getGreenTimeMs(int level) {
  if (level >= 3) {
    return HIGH_GREEN_MS;
  }
  if (level == 2) {
    return MEDIUM_GREEN_MS;
  }
  return LOW_GREEN_MS;
}

bool runPhase(byte phase, int initialLevel, byte phaseCode) {
  if (initialLevel <= 0) {
    return false;
  }

  waitUntilPhaseAllowed(phaseCode);
  setPhaseGreen(phase);

  byte oppositePhase = phase == PHASE1 ? PHASE2 : PHASE1;
  unsigned long segmentMs = getGreenTimeMs(initialLevel);
  unsigned long segmentStartedAt = millis();
  unsigned long clearStartedAt = 0;

  while (true) {
    readSerialCommands();
    refreshSensors();

    if (aiLevelsFresh() && crossCheckNeedsAllRed()) {
      allRed();
      return false;
    }

    int currentLevel = getEffectivePhaseLevel(phase);
    int oppositeLevel = getEffectivePhaseLevel(oppositePhase);
    unsigned long now = millis();
    unsigned long elapsed = now - segmentStartedAt;

    if (currentLevel > 0) {
      clearStartedAt = 0;
    } else if (elapsed >= MIN_GREEN_MS) {
      if (clearStartedAt == 0) {
        clearStartedAt = now;
      }
      if (now - clearStartedAt >= CLEAR_GRACE_MS) {
        break;
      }
    }

    if (elapsed >= segmentMs) {
      if (currentLevel > 0 && oppositeLevel <= 0) {
        segmentStartedAt = now;
        segmentMs = getGreenTimeMs(currentLevel);
        clearStartedAt = 0;
        continue;
      }
      break;
    }

    delay(10);
  }

  setPhaseYellow(phase);
  waitWithInputs(YELLOW_MS);
  setPhaseRed(phase);
  return true;
}

void allRed() {
  for (byte phase = 0; phase < PHASE_COUNT; phase++) {
    setPhaseRed(phase);
  }
}

void setPhaseRed(byte phase) {
  digitalWrite(R_PINS[phase], HIGH);
  digitalWrite(Y_PINS[phase], LOW);
  digitalWrite(G_PINS[phase], LOW);
}

void setPhaseGreen(byte phase) {
  allRed();
  digitalWrite(R_PINS[phase], LOW);
  digitalWrite(G_PINS[phase], HIGH);
}

void setPhaseYellow(byte phase) {
  digitalWrite(G_PINS[phase], LOW);
  digitalWrite(Y_PINS[phase], HIGH);
}

bool blockInfoFresh() {
  return lastBlockedUpdateMs > 0 && millis() - lastBlockedUpdateMs <= BLOCKED_TIMEOUT_MS;
}

bool zoneBlocksPhase(byte phaseCode) {
  if (!blockInfoFresh() || blockedOwnerPhase == 0) {
    return false;
  }
  if (blockedOwnerPhase == 3) {
    return true;
  }
  return blockedOwnerPhase != phaseCode;
}

void waitUntilPhaseAllowed(byte phaseCode) {
  while (zoneBlocksPhase(phaseCode)) {
    allRed();
    waitWithInputs(IDLE_POLL_MS);
  }
}

void waitWithInputs(unsigned long durationMs) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < durationMs) {
    readSerialCommands();
    refreshSensors();
    delay(10);
  }
}

void printSensorHealth() {
  Serial.print("SENSORS");
  for (byte road = 0; road < ROAD_COUNT; road++) {
    Serial.print(",R");
    Serial.print(road + 1);
    Serial.print('=');

    if (sensorDistancesCm[road] >= 999) {
      Serial.print("NO_ECHO");
    } else {
      Serial.print(sensorDistancesCm[road]);
      Serial.print("cm");
    }

    Serial.print(':');
    if (sensorLevels[road] > 0) {
      Serial.print("CO_XE");
    } else if (sensorDistancesCm[road] >= 999) {
      Serial.print("KHONG_DOC_DUOC");
    } else {
      Serial.print("OK_KHONG_XE");
    }
  }
  Serial.println();
}
