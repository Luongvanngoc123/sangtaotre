// AI traffic light controller for 2 physical traffic-light modules and 4 roads.
//
// Physical light wiring:
// Phase 1 light controls Road 1 + Road 3:
//   D2 -> R, D3 -> Y, D4 -> G
// Phase 2 light controls Road 2 + Road 4:
//   D5 -> R, D6 -> Y, D7 -> G
//
// D12-D13 are unused by this sketch.
//
// HC-SR04 sensors:
// Road 1: TRIG A0, ECHO D8
// Road 2: TRIG A1, ECHO D9
// Road 3: TRIG A2, ECHO D10
// Road 4: TRIG A3, ECHO D11
//
// Cross-check logic while AI is online:
//   Camera sees car, sensor does not -> all red and ALERT,CAM_THAY_CAM_BIEN_KHONG,...
//   Camera does not see car, sensor does -> ALERT,CAM_KHONG_THAY_CAM_BIEN_THAY,...
//   Camera and sensor both see car on the same road -> normal
//
// Serial protocol from AI:
//   LEVELS,r1,r2,r3,r4
//   BLOCKED,0  -> intersection clear
//   BLOCKED,1  -> vehicle in intersection belongs to roads 1&3
//   BLOCKED,2  -> vehicle in intersection belongs to roads 2&4
//   BLOCKED,3  -> unknown or mixed conflict
//
// Levels:
//   0 = no car
//   1 = low density    -> green 5s
//   2 = medium density -> green 10s
//   3 = high density   -> green 15s

const byte ROAD_COUNT = 4;
const byte PHASE_COUNT = 2;

const byte PHASE1 = 0;
const byte PHASE2 = 1;

const byte R_PINS[PHASE_COUNT] = {2, 5};
const byte Y_PINS[PHASE_COUNT] = {3, 6};
const byte G_PINS[PHASE_COUNT] = {4, 7};
const byte ULTRASONIC_TRIG_PINS[ROAD_COUNT] = {A0, A1, A2, A3};
const byte ULTRASONIC_ECHO_PINS[ROAD_COUNT] = {8, 9, 10, 11};

const unsigned long LOW_GREEN_MS = 5000;
const unsigned long MEDIUM_GREEN_MS = 10000;
const unsigned long HIGH_GREEN_MS = 15000;
const unsigned long YELLOW_MS = 2000;
const unsigned long IDLE_POLL_MS = 100;
const unsigned long AI_TIMEOUT_MS = 15000;
const unsigned long BLOCKED_TIMEOUT_MS = 3000;
const unsigned long ULTRASONIC_REFRESH_MS = 250;
const unsigned long ALERT_REPEAT_MS = 3000;
const unsigned int ULTRASONIC_TIMEOUT_US = 6000;
const int ULTRASONIC_MIN_CM = 3;
const int ULTRASONIC_DETECT_CM = 30;
const int ULTRASONIC_PRESENT_LEVEL = 1;

int roadLevels[ROAD_COUNT] = {0, 0, 0, 0};
int sensorLevels[ROAD_COUNT] = {0, 0, 0, 0};
int sensorDistancesCm[ROAD_COUNT] = {999, 999, 999, 999};
unsigned long lastAiUpdateMs = 0;
byte blockedOwnerPhase = 0;
unsigned long lastBlockedUpdateMs = 0;
unsigned long lastUltrasonicReadMs = 0;
byte lastAlertType = 0;
byte lastAlertCameraOnlyMask = 0;
byte lastAlertSensorOnlyMask = 0;
unsigned long lastAlertSentMs = 0;

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
  Serial.println("READY");
  Serial.println("Lights: phase1=D2,D3,D4 phase2=D5,D6,D7");
  Serial.println("Send: LEVELS,0,1,2,3");
  Serial.println("Ultrasonic TRIG: A0,A1,A2,A3 ECHO: D8,D9,D10,D11");
}

void loop() {
  readSerialCommands();
  refreshUltrasonicSensors();

  if (aiLevelsFresh()) {
    if (crossCheckNeedsAllRed()) {
      allRed();
      waitWithSerial(IDLE_POLL_MS);
      return;
    }
  } else {
    copySensorLevelsToRoadLevels();
  }

  int phase1Level = max(roadLevels[0], roadLevels[2]);
  int phase2Level = max(roadLevels[1], roadLevels[3]);

  if (phase1Level == 0 && phase2Level == 0) {
    allRed();
    waitWithSerial(IDLE_POLL_MS);
    return;
  }

  runPhaseIfNeeded(PHASE1, phase1Level, 1);
  runPhaseIfNeeded(PHASE2, phase2Level, 2);
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
    forceRefreshUltrasonicSensors();
    printSensorHealth();
    return;
  }

  if (equalsIgnoreCase(token, "ALLRED")) {
    clearRoadLevels();
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
    roadLevels[road] = newLevels[road];
  }
  lastAiUpdateMs = millis();

  Serial.print("OK LEVELS ");
  for (byte road = 0; road < ROAD_COUNT; road++) {
    Serial.print(roadLevels[road]);
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

void clearRoadLevels() {
  for (byte road = 0; road < ROAD_COUNT; road++) {
    roadLevels[road] = 0;
  }
}

bool aiLevelsFresh() {
  return lastAiUpdateMs > 0 && millis() - lastAiUpdateMs <= AI_TIMEOUT_MS;
}

void refreshUltrasonicSensors() {
  unsigned long now = millis();
  if (lastUltrasonicReadMs > 0 && now - lastUltrasonicReadMs < ULTRASONIC_REFRESH_MS) {
    return;
  }

  lastUltrasonicReadMs = now;
  for (byte road = 0; road < ROAD_COUNT; road++) {
    int distanceCm = readUltrasonicCm(ULTRASONIC_TRIG_PINS[road], ULTRASONIC_ECHO_PINS[road]);
    sensorDistancesCm[road] = distanceCm;
    bool vehiclePresent = distanceCm >= ULTRASONIC_MIN_CM && distanceCm <= ULTRASONIC_DETECT_CM;
    sensorLevels[road] = vehiclePresent ? ULTRASONIC_PRESENT_LEVEL : 0;
  }
}

void forceRefreshUltrasonicSensors() {
  lastUltrasonicReadMs = 0;
  refreshUltrasonicSensors();
}

void copySensorLevelsToRoadLevels() {
  for (byte road = 0; road < ROAD_COUNT; road++) {
    roadLevels[road] = sensorLevels[road];
  }
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
  byte cameraMask = presentMaskFromLevels(roadLevels);
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
  return cameraOnlyMask > 0;
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

unsigned long getGreenTimeMs(int level) {
  switch (level) {
    case 1:
      return LOW_GREEN_MS;
    case 2:
      return MEDIUM_GREEN_MS;
    case 3:
      return HIGH_GREEN_MS;
    default:
      return 0;
  }
}

void runPhaseIfNeeded(byte phase, int level, byte phaseCode) {
  if (level <= 0) {
    return;
  }

  waitUntilPhaseAllowed(phaseCode);

  setPhaseGreen(phase);
  if (!waitWithSerial(getGreenTimeMs(level))) {
    return;
  }

  setPhaseYellow(phase);
  if (!waitWithSerial(YELLOW_MS)) {
    return;
  }

  setPhaseRed(phase);
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
    waitWithSerial(IDLE_POLL_MS);
  }
}

bool waitWithSerial(unsigned long durationMs) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < durationMs) {
    readSerialCommands();
    refreshUltrasonicSensors();
    if (aiLevelsFresh() && crossCheckNeedsAllRed()) {
      allRed();
      return false;
    }
    delay(10);
  }
  return true;
}
