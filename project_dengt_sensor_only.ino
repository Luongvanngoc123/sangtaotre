// Sensor-only traffic light controller for 2 physical traffic-light modules and 4 roads.
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
// This sketch does not need AI or Python. It reads HC-SR04 sensors directly.

const byte ROAD_COUNT = 4;
const byte PHASE_COUNT = 2;

const byte PHASE1 = 0;
const byte PHASE2 = 1;
const byte NO_PHASE = 255;

const byte R_PINS[PHASE_COUNT] = {2, 5};
const byte Y_PINS[PHASE_COUNT] = {3, 6};
const byte G_PINS[PHASE_COUNT] = {4, 7};

const byte ULTRASONIC_TRIG_PINS[ROAD_COUNT] = {A0, A1, A2, A3};
const byte ULTRASONIC_ECHO_PINS[ROAD_COUNT] = {8, 9, 10, 11};

const unsigned long ONE_ROAD_GREEN_MS = 5000;
const unsigned long TWO_ROADS_GREEN_MS = 10000;
const unsigned long MIN_GREEN_MS = 2000;
const unsigned long YELLOW_MS = 2000;
const unsigned long IDLE_POLL_MS = 100;
const unsigned long SENSOR_REFRESH_MS = 150;
const unsigned int ULTRASONIC_TIMEOUT_US = 6000;
const int ULTRASONIC_MIN_CM = 3;
const int ULTRASONIC_DETECT_CM = 15;

int sensorLevels[ROAD_COUNT] = {0, 0, 0, 0};
int sensorDistancesCm[ROAD_COUNT] = {999, 999, 999, 999};
unsigned long lastSensorReadMs = 0;
byte lastServedPhase = PHASE2;

char serialBuffer[32];
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
  Serial.println("READY SENSOR_ONLY");
  Serial.println("Lights: phase1=D2,D3,D4 phase2=D5,D6,D7");
  Serial.println("Ultrasonic TRIG: A0,A1,A2,A3 ECHO: D8,D9,D10,D11");
}

void loop() {
  readSerialCommands();
  refreshSensors();

  int phase1Level = getPhaseLevel(PHASE1);
  int phase2Level = getPhaseLevel(PHASE2);

  if (phase1Level == 0 && phase2Level == 0) {
    allRed();
    waitWithSensors(IDLE_POLL_MS);
    return;
  }

  byte selectedPhase = choosePhaseToRun(phase1Level, phase2Level);
  if (selectedPhase == PHASE1) {
    if (runPhase(PHASE1, phase1Level)) {
      lastServedPhase = PHASE1;
    }
  } else if (selectedPhase == PHASE2) {
    if (runPhase(PHASE2, phase2Level)) {
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
  if (equalsIgnoreCase(command, "PING")) {
    Serial.println("PONG");
    return;
  }

  if (equalsIgnoreCase(command, "ALLRED")) {
    allRed();
    Serial.println("OK ALLRED");
    return;
  }

  if (equalsIgnoreCase(command, "SENSORS") || equalsIgnoreCase(command, "HEALTH")) {
    forceRefreshSensors();
    printSensorHealth();
    return;
  }

  Serial.println("ERR expected PING, ALLRED, SENSORS, or HEALTH");
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

int getPhaseLevel(byte phase) {
  if (phase == PHASE1) {
    return sensorLevels[0] + sensorLevels[2];
  }
  return sensorLevels[1] + sensorLevels[3];
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

unsigned long getGreenTimeMs(int phaseLevel) {
  if (phaseLevel >= 2) {
    return TWO_ROADS_GREEN_MS;
  }
  return ONE_ROAD_GREEN_MS;
}

bool runPhase(byte phase, int initialLevel) {
  if (initialLevel <= 0) {
    return false;
  }

  setPhaseGreen(phase);

  unsigned long greenMs = getGreenTimeMs(initialLevel);
  unsigned long startedAt = millis();
  while (millis() - startedAt < greenMs) {
    readSerialCommands();
    refreshSensors();

    int currentLevel = getPhaseLevel(phase);
    if (currentLevel <= 0 && millis() - startedAt >= MIN_GREEN_MS) {
      break;
    }

    delay(10);
  }

  setPhaseYellow(phase);
  waitWithSensors(YELLOW_MS);
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

void waitWithSensors(unsigned long durationMs) {
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
