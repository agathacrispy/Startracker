#include <AccelStepper.h>
#include <EEPROM.h>

const int ALT_STEP_PIN = 2;
const int ALT_DIR_PIN  = 3;
const int AZ_STEP_PIN  = 5;
const int AZ_DIR_PIN   = 6;

static const int32_t MOTOR_STEPS_PER_REV = 200;
static const int32_t MICROSTEPS          = 8;
static const int32_t GEAR_AZ             = 60;
static const int32_t GEAR_ALT            = 2;

static const int32_t AZ_STEPS_PER_REV  = MOTOR_STEPS_PER_REV * MICROSTEPS * GEAR_AZ;   // 96000
static const int32_t ALT_STEPS_PER_REV = MOTOR_STEPS_PER_REV * MICROSTEPS * GEAR_ALT;  // 3200

static const int32_t DEG_SCALE = 1000000;
static const int64_t FULL_TURN_UDEG = (int64_t)360 * DEG_SCALE;
static const int64_t HALF_TURN_UDEG = (int64_t)180 * DEG_SCALE;

const float MAX_SPEED_AZ  = 6000;
const float MAX_SPEED_ALT = 3000;
const float ACCEL_AZ      = 4000;
const float ACCEL_ALT     = 2500;

AccelStepper az(AccelStepper::DRIVER, AZ_STEP_PIN, AZ_DIR_PIN);
AccelStepper alt(AccelStepper::DRIVER, ALT_STEP_PIN, ALT_DIR_PIN);

struct Persist {
  uint32_t magic;
  int32_t  homeAzSteps;
  int32_t  homeAltSteps;
  uint8_t  homeValid;
};

static const uint32_t MAGIC = 0x415A4154;
static const int EEPROM_ADDR = 0;

Persist cfg;

static int64_t mod_pos(int64_t a, int64_t m) {
  int64_t r = a % m;
  return (r < 0) ? (r + m) : r;
}

static int64_t normAzUdeg(int64_t udeg) {
  int64_t r = udeg % FULL_TURN_UDEG;
  return (r < 0) ? (r + FULL_TURN_UDEG) : r;
}

static int32_t udegToSteps(int64_t udeg, int32_t stepsPerRev) {
  int64_t num = udeg * (int64_t)stepsPerRev;
  int64_t den = FULL_TURN_UDEG;
  if (num >= 0) return (int32_t)((num + den/2) / den);
  else          return (int32_t)((num - den/2) / den);
}

static int64_t stepsToUdeg(int64_t steps, int32_t stepsPerRev) {
  int64_t num = steps * FULL_TURN_UDEG;
  int64_t den = stepsPerRev;
  if (num >= 0) return (num + den/2) / den;
  else          return (num - den/2) / den;
}

static int64_t shortestDeltaAzUdeg(int64_t curUdeg, int64_t tgtUdeg) {
  int64_t curN = normAzUdeg(curUdeg);
  int64_t tgtN = normAzUdeg(tgtUdeg);
  int64_t d = tgtN - curN;
  if (d >  HALF_TURN_UDEG) d -= FULL_TURN_UDEG;
  if (d < -HALF_TURN_UDEG) d += FULL_TURN_UDEG;
  return d;
}

static bool parseDegToUdeg(const char* s, int64_t &outUdeg) {
  while (*s == ' ' || *s == '\t') s++;
  bool neg = false;
  if (*s == '-') { neg = true; s++; }
  else if (*s == '+') { s++; }
  if (*s < '0' || *s > '9') return false;

  int64_t intPart = 0;
  while (*s >= '0' && *s <= '9') { intPart = intPart * 10 + (*s - '0'); s++; }

  int64_t fracPart = 0;
  int32_t fracDigits = 0;
  if (*s == '.') {
    s++;
    while (*s >= '0' && *s <= '9' && fracDigits < 6) {
      fracPart = fracPart * 10 + (*s - '0');
      fracDigits++; s++;
    }
    while (*s >= '0' && *s <= '9') s++;
  }

  while (*s == ' ' || *s == '\t') s++;
  if (*s != '\0') return false;

  while (fracDigits < 6) { fracPart *= 10; fracDigits++; }
  int64_t udeg = intPart * (int64_t)DEG_SCALE + fracPart;
  outUdeg = neg ? -udeg : udeg;
  return true;
}

static bool readLine(char* buf, size_t buflen) {
  static size_t n = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { buf[n] = '\0'; n = 0; return true; }
    if (n < buflen - 1) buf[n++] = c;
  }
  return false;
}

static void loadCfg() {
  EEPROM.get(EEPROM_ADDR, cfg);
  if (cfg.magic != MAGIC) {
    cfg.magic = MAGIC;
    cfg.homeValid = 0;
    cfg.homeAzSteps = 0;
    cfg.homeAltSteps = 0;
    EEPROM.put(EEPROM_ADDR, cfg);
  }
}

static void saveCfg() {
  EEPROM.put(EEPROM_ADDR, cfg);
}

static void printUdeg(int64_t udeg) {
  bool neg = (udeg < 0);
  if (neg) udeg = -udeg;
  long whole = (long)(udeg / DEG_SCALE);
  long frac  = (long)(udeg % DEG_SCALE);
  if (neg) Serial.print('-');
  Serial.print(whole);
  Serial.print('.');
  for (long p = 100000; p >= 1; p /= 10) Serial.print((frac / p) % 10);
}

static void printWhere() {
  int64_t azSteps = az.currentPosition();
  int64_t azWrapped = mod_pos(azSteps, AZ_STEPS_PER_REV);
  int64_t azUdeg = normAzUdeg(stepsToUdeg(azWrapped, AZ_STEPS_PER_REV));

  int64_t altSteps = alt.currentPosition();
  int64_t altUdeg = stepsToUdeg(altSteps, ALT_STEPS_PER_REV);

  Serial.print("AZ=");
  printUdeg(azUdeg);
  Serial.print(" ALT=");
  printUdeg(altUdeg);
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  az.setMaxSpeed(MAX_SPEED_AZ);
  az.setAcceleration(ACCEL_AZ);

  alt.setMaxSpeed(MAX_SPEED_ALT);
  alt.setAcceleration(ACCEL_ALT);

  loadCfg();

  az.setCurrentPosition(0);
  alt.setCurrentPosition(0);

  Serial.println("READY");
}

static void handleGoto(const char* saz, const char* salt) {
  int64_t azUdeg = 0, altUdeg = 0;
  if (!parseDegToUdeg(saz, azUdeg) || !parseDegToUdeg(salt, altUdeg)) {
    Serial.println("ERR");
    return;
  }

  int64_t curAzSteps = az.currentPosition();
  int64_t curAzWrapped = mod_pos(curAzSteps, AZ_STEPS_PER_REV);
  int64_t curAzUdeg = stepsToUdeg(curAzWrapped, AZ_STEPS_PER_REV);

  int64_t dAzUdeg = shortestDeltaAzUdeg(curAzUdeg, azUdeg);
  int32_t dAzSteps = udegToSteps(dAzUdeg, AZ_STEPS_PER_REV);
  az.moveTo((long)(curAzSteps + dAzSteps));

  int32_t altTargetSteps = udegToSteps(altUdeg, ALT_STEPS_PER_REV);
  alt.moveTo((long)altTargetSteps);

  Serial.println("OK");
}

void loop() {
  az.run();
  alt.run();

  char line[96];
  if (!readLine(line, sizeof(line))) return;

  if (strcmp(line, "SYNC") == 0) {
    az.setCurrentPosition(0);
    alt.setCurrentPosition(0);
    Serial.println("OK");
    return;
  }

  if (strcmp(line, "WHERE") == 0)   { printWhere(); return; }
  if (strcmp(line, "SETHOME") == 0) {
    cfg.homeAzSteps  = (int32_t)az.currentPosition();
    cfg.homeAltSteps = (int32_t)alt.currentPosition();
    cfg.homeValid = 1;
    saveCfg();
    Serial.println("OK");
    return;
  }

  if (strcmp(line, "GOHOME") == 0) {
    if (cfg.homeValid) {
      az.moveTo(cfg.homeAzSteps);
      alt.moveTo(cfg.homeAltSteps);
      Serial.println("OK");
    } else {
      Serial.println("NOHOME");
    }
    return;
  }

  if (line[0] == 'G' && line[1] == ',') {
    char* p = line + 2;
    char* comma = strchr(p, ',');
    if (!comma) { Serial.println("ERR"); return; }
    *comma = '\0';
    handleGoto(p, comma + 1);
    return;
  }

  Serial.println("ERR");
}
