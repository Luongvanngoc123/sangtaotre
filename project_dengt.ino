// AI traffic light controller for 4 traffic-light modules.
//
// Wiring:
// Road 1: D2  -> R, D3  -> Y, D4  -> G
// Road 2: D5  -> R, D6  -> Y, D7  -> G
// Road 3: D8  -> R, D9  -> Y, D10 -> G
// Road 4: D11 -> R, D12 -> Y, D13 -> G
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

const byte R_PINS[ROAD_COUNT] = {2, 5, 8, 11};
const byte Y_PINS[ROAD_COUNT] = {3, 6, 9, 12};
const byte G_PINS[ROAD_COUNT] = {4, 7, 10, 13};

const unsigned long LOW_GREEN_MS = 5000;
const unsigned long MEDIUM_GREEN_MS = 10000;
const unsigned long HIGH_GREEN_MS = 15000;
const unsigned long YELLOW_MS = 2000;
const unsigned long IDLE_POLL_MS = 100;
const unsigned long AI_TIMEOUT_MS = 15000;
const unsigned long BLOCKED_TIMEOUT_MS = 3000;

int roadLevels[ROAD_COUNT] = {0, 0, 0, 0};
unsigned long lastAiUpdateMs = 0;
byte blockedOwnerPhase = 0;
unsigned long lastBlockedUpdateMs = 0;

char serialBuffer[64];
byte serialIndex = 0;

void setup() {
  Serial.begin(115200);

  for (byte road = 0; road < ROAD_COUNT; road++) {
    pinMode(R_PINS[road], OUTPUT);
    pinMode(Y_PINS[road], OUTPUT);
    pinMode(G_PINS[road], OUTPUT);
  }

  allRed();
  Serial.println("READY");
  Serial.println("Send: LEVELS,0,1,2,3");
}

void loop() {
  readSerialCommands();

  if (lastAiUpdateMs > 0 && millis() - lastAiUpdateMs > AI_TIMEOUT_MS) {
    clearRoadLevels();
    allRed();
  }

  int pair13 = max(roadLevels[0], roadLevels[2]);
  int pair24 = max(roadLevels[1], roadLevels[3]);

  if (pair13 == 0 && pair24 == 0) {
    allRed();
    waitWithSerial(IDLE_POLL_MS);
    return;
  }

  runPairIfNeeded(0, 2, pair13, 1);
  runPairIfNeeded(1, 3, pair24, 2);
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

void runPairIfNeeded(byte roadA, byte roadB, int level, byte phase) {
  if (level <= 0) {
    return;
  }

  waitUntilPhaseAllowed(phase);

  setPairGreen(roadA, roadB);
  waitWithSerial(getGreenTimeMs(level));

  setPairYellow(roadA, roadB);
  waitWithSerial(YELLOW_MS);

  setRoadRed(roadA);
  setRoadRed(roadB);
}

void allRed() {
  for (byte road = 0; road < ROAD_COUNT; road++) {
    setRoadRed(road);
  }
}

void setRoadRed(byte road) {
  digitalWrite(R_PINS[road], HIGH);
  digitalWrite(Y_PINS[road], LOW);
  digitalWrite(G_PINS[road], LOW);
}

void setPairGreen(byte roadA, byte roadB) {
  allRed();
  digitalWrite(R_PINS[roadA], LOW);
  digitalWrite(G_PINS[roadA], HIGH);
  digitalWrite(R_PINS[roadB], LOW);
  digitalWrite(G_PINS[roadB], HIGH);
}

void setPairYellow(byte roadA, byte roadB) {
  digitalWrite(G_PINS[roadA], LOW);
  digitalWrite(G_PINS[roadB], LOW);
  digitalWrite(Y_PINS[roadA], HIGH);
  digitalWrite(Y_PINS[roadB], HIGH);
}

bool blockInfoFresh() {
  return lastBlockedUpdateMs > 0 && millis() - lastBlockedUpdateMs <= BLOCKED_TIMEOUT_MS;
}

bool zoneBlocksPhase(byte phase) {
  if (!blockInfoFresh() || blockedOwnerPhase == 0) {
    return false;
  }
  if (blockedOwnerPhase == 3) {
    return true;
  }
  return blockedOwnerPhase != phase;
}

void waitUntilPhaseAllowed(byte phase) {
  while (zoneBlocksPhase(phase)) {
    allRed();
    waitWithSerial(IDLE_POLL_MS);
  }
}

void waitWithSerial(unsigned long durationMs) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < durationMs) {
    readSerialCommands();
    delay(10);
  }
}
