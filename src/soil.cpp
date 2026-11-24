#include "soil.h"
#include "settings.h"

static const uint32_t CYCLE_MS = 2000;
static const uint8_t SAMPLES = 16;        // promedio por lectura: el cable largo mete ruido
static const uint16_t MV_MIN = 300;       // por debajo: no hay sensor (pull-down a GND)
static const uint16_t MV_MAX = 3000;      // por encima: pin suelto/flotando o en corto a 3,3 V
static const float SMOOTHING = 0.3f;      // filtro exponencial entre lecturas

static SoilReading data[NUM_TENTS];
static float filtered[NUM_TENTS][SOIL_SENSORS_MAX];
static uint32_t lastCycle = 0;

static float toPercent(const SoilConfig& c, float mv) {
  if (c.dry == c.wet) return 0;
  float p = (c.dry - mv) * 100.0f / (c.dry - c.wet);  // más agua = menos tensión
  return constrain(p, 0.0f, 100.0f);
}

void soilBegin() {
  for (uint8_t t = 0; t < NUM_TENTS; t++)
    for (uint8_t k = 0; k < SOIL_SENSORS_MAX; k++) {
      pinMode(SOIL_PINS[t][k], INPUT);
      analogSetPinAttenuation(SOIL_PINS[t][k], ADC_11db);  // rango hasta ~3,1 V
      filtered[t][k] = -1;
    }
}

void soilLoop() {
  if (lastCycle && millis() - lastCycle < CYCLE_MS) return;
  lastCycle = millis();
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    SoilReading& r = data[t];
    const SoilConfig& c = settings.tents[t].soil;
    float sumRaw = 0, sumPct = 0;
    uint8_t n = 0;
    for (uint8_t k = 0; k < SOIL_SENSORS_MAX; k++) {
      uint32_t acc = 0;
      for (uint8_t i = 0; i < SAMPLES; i++) acc += analogReadMilliVolts(SOIL_PINS[t][k]);
      float mv = acc / (float)SAMPLES;
      bool ok = settings.tents[t].enabled && mv >= MV_MIN && mv <= MV_MAX;
      if (ok != r.chValid[k]) Serial.printf("[suelo] carpa %u sensor %u %s\n", t + 1, k + 1, ok ? "conectado" : "desconectado");
      r.chValid[k] = ok;
      float& f = filtered[t][k];
      if (!ok) {
        f = -1;
        continue;
      }
      f = f < 0 ? mv : f + SMOOTHING * (mv - f);
      r.chRaw[k] = lroundf(f);
      sumRaw += f;
      sumPct += r.chPercent[k] = toPercent(c, f);
      n++;
    }
    r.valid = n > 0;
    r.raw = n ? lroundf(sumRaw / n) : 0;
    r.percent = n ? sumPct / n : 0;
  }
}

const SoilReading& soil(uint8_t tent) { return data[tent < NUM_TENTS ? tent : 0]; }
bool soilPresent(uint8_t tent) { return tent < NUM_TENTS && data[tent].valid; }
