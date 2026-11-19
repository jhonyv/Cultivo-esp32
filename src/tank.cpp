#include "tank.h"
#include <utility>
#include "settings.h"

static const uint32_t READ_EVERY_MS = 2000;  // una medición por vez, alternando carpas
static const uint8_t SAMPLES = 5;            // mediana de las últimas lecturas: el agua se mueve y hay ecos
static const uint32_t ECHO_TIMEOUT_US = 30000;  // ~5 m
static const uint8_t MAX_FAILS = 5;

static TankReading data[NUM_TENTS];
static float samples[NUM_TENTS][SAMPLES];
static uint8_t sampleCount[NUM_TENTS], sampleHead[NUM_TENTS], fails[NUM_TENTS];
static uint32_t lastRead = 0;
static uint8_t next = 0;
static bool pinsReady[NUM_TENTS];

static float median(const float* v, uint8_t n) {
  float s[SAMPLES];
  memcpy(s, v, n * sizeof(float));
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && s[j - 1] > s[j]; j--) std::swap(s[j], s[j - 1]);
  return s[n / 2];
}

static float measureCm(uint8_t t) {
  digitalWrite(TANK_TRIG_PINS[t], LOW);
  delayMicroseconds(4);
  digitalWrite(TANK_TRIG_PINS[t], HIGH);
  delayMicroseconds(20);  // el JSN-SR04T pide un pulso más largo que el HC-SR04
  digitalWrite(TANK_TRIG_PINS[t], LOW);
  uint32_t us = pulseIn(TANK_ECHO_PINS[t], HIGH, ECHO_TIMEOUT_US);
  if (!us) return -1;
  return us / 58.0f;
}

static void readTent(uint8_t t) {
  const TankConfig& c = settings.tents[t].tank;
  if (!pinsReady[t]) {
    pinMode(TANK_TRIG_PINS[t], OUTPUT);
    digitalWrite(TANK_TRIG_PINS[t], LOW);
    pinMode(TANK_ECHO_PINS[t], INPUT);
    pinsReady[t] = true;
  }
  float cm = measureCm(t);
  TankReading& r = data[t];
  if (cm < 2 || cm > 500) {
    if (++fails[t] >= MAX_FAILS) r.valid = false, sampleCount[t] = 0;
    return;
  }
  fails[t] = 0;
  samples[t][sampleHead[t]] = cm;
  sampleHead[t] = (sampleHead[t] + 1) % SAMPLES;
  if (sampleCount[t] < SAMPLES) sampleCount[t]++;
  r.cm = median(samples[t], sampleCount[t]);
  float pct = (c.emptyCm - r.cm) * 100.0f / (c.emptyCm - c.fullCm);
  r.percent = constrain(pct, 0.0f, 100.0f);
  r.liters = r.percent * c.liters / 100.0f;
  r.valid = true;
}

void tankBegin() {}

void tankLoop() {
  if (lastRead && millis() - lastRead < READ_EVERY_MS) return;
  lastRead = millis();
  for (uint8_t k = 0; k < NUM_TENTS; k++) {
    uint8_t t = (next + k) % NUM_TENTS;
    const TentConfig& tc = settings.tents[t];
    if (!tc.enabled || !tc.tank.enabled) {
      data[t] = {};
      sampleCount[t] = 0;
      continue;
    }
    readTent(t);
    next = (t + 1) % NUM_TENTS;
    return;
  }
}

const TankReading& tank(uint8_t tent) { return data[tent < NUM_TENTS ? tent : 0]; }

bool tankLow(uint8_t tent) {
  const TankConfig& c = settings.tents[tent].tank;
  const TankReading& r = tank(tent);
  return c.enabled && c.cutPct && r.valid && r.percent < c.cutPct;
}
