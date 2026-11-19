#include "soil.h"
#include <Wire.h>
#include "settings.h"

// Conversión de a un canal sin bloquear: se dispara la medición y se lee en la vuelta siguiente.
static const uint32_t CYCLE_MS = 5000;       // cada cuánto se leen todos los canales
static const uint32_t CONVERSION_MS = 12;    // 128 muestras/s = 8 ms
static const uint16_t RAW_MIN = 1600;        // < 0,2 V: canal sin sensor (o en corto)
static const uint16_t RAW_MAX = 28000;       // > 3,5 V: canal suelto/flotando
static const float SMOOTHING = 0.3f;         // filtro exponencial: el capacitivo es ruidoso

static SoilReading data[NUM_TENTS];
static float filtered[NUM_TENTS][SOIL_SENSORS_MAX];
static bool present[NUM_TENTS];
static uint8_t curTent = 0, curCh = 0;
static bool pending = false;
static uint32_t pendingSince = 0, cycleStart = 0;
static bool cycleRunning = false;

static TwoWire& busFor(uint8_t tent) { return tent == 0 ? Wire : Wire1; }

static bool startConversion(uint8_t tent, uint8_t ch) {
  // OS=1, MUX=1xx (AINx contra GND), PGA ±4,096 V, single-shot, 128 SPS, comparador apagado.
  uint16_t cfg = 0x8000 | ((4 + ch) << 12) | 0x0200 | 0x0100 | 0x0080 | 0x0003;
  TwoWire& bus = busFor(tent);
  bus.beginTransmission(ADS1115_ADDR);
  bus.write(0x01);
  bus.write(cfg >> 8);
  bus.write(cfg & 0xFF);
  return bus.endTransmission() == 0;
}

static bool readConversion(uint8_t tent, int16_t& value) {
  TwoWire& bus = busFor(tent);
  bus.beginTransmission(ADS1115_ADDR);
  bus.write(0x00);
  if (bus.endTransmission() != 0) return false;
  if (bus.requestFrom((uint8_t)ADS1115_ADDR, (uint8_t)2) != 2) return false;
  value = (int16_t)((bus.read() << 8) | bus.read());
  return true;
}

static float toPercent(const SoilConfig& c, float raw) {
  if (c.dry == c.wet) return 0;
  float p = (c.dry - raw) * 100.0f / (c.dry - c.wet);  // más agua = menos tensión
  return constrain(p, 0.0f, 100.0f);
}

static void finishTent(uint8_t t) {
  const SoilConfig& c = settings.tents[t].soil;
  SoilReading& r = data[t];
  float sumRaw = 0, sumPct = 0;
  uint8_t n = 0;
  for (uint8_t k = 0; k < SOIL_SENSORS_MAX; k++) {
    if (k >= c.count || !r.chValid[k]) {
      r.chValid[k] = false;
      continue;
    }
    sumRaw += r.chRaw[k];
    sumPct += r.chPercent[k] = toPercent(c, r.chRaw[k]);
    n++;
  }
  r.valid = n > 0;
  r.raw = n ? lroundf(sumRaw / n) : 0;
  r.percent = n ? sumPct / n : 0;
}

// Próximo canal a leer; false cuando se terminó la vuelta.
static bool advance() {
  while (curTent < NUM_TENTS) {
    const SoilConfig& c = settings.tents[curTent].soil;
    bool tentOn = c.enabled && settings.tents[curTent].enabled;
    if (tentOn && curCh < c.count) return true;
    if (tentOn) finishTent(curTent);
    curTent++;
    curCh = 0;
  }
  return false;
}

void soilBegin() {
  for (uint8_t t = 0; t < NUM_TENTS; t++)
    for (uint8_t k = 0; k < SOIL_SENSORS_MAX; k++) filtered[t][k] = -1;
}

void soilLoop() {
  if (pending) {
    if (millis() - pendingSince < CONVERSION_MS) return;
    pending = false;
    int16_t v;
    SoilReading& r = data[curTent];
    if (readConversion(curTent, v) && v >= RAW_MIN && v <= RAW_MAX) {
      float& f = filtered[curTent][curCh];
      f = f < 0 ? v : f + SMOOTHING * (v - f);
      r.chRaw[curCh] = lroundf(f);
      r.chValid[curCh] = true;
    } else {
      r.chValid[curCh] = false;
      filtered[curTent][curCh] = -1;
    }
    curCh++;
  }

  if (!cycleRunning) {
    if (cycleStart && millis() - cycleStart < CYCLE_MS) return;
    cycleStart = millis();
    cycleRunning = true;
    curTent = 0;
    curCh = 0;
  }

  while (advance()) {
    if (startConversion(curTent, curCh)) {
      if (!present[curTent]) Serial.printf("[suelo] ADS1115 de la carpa %u encontrado\n", curTent + 1);
      present[curTent] = true;
      pending = true;
      pendingSince = millis();
      return;
    }
    // El ADS1115 de esta carpa no responde: se invalida y se pasa a la siguiente.
    if (present[curTent]) Serial.printf("[suelo] ADS1115 de la carpa %u no responde\n", curTent + 1);
    present[curTent] = false;
    for (uint8_t k = 0; k < SOIL_SENSORS_MAX; k++) data[curTent].chValid[k] = false;
    curCh = SOIL_SENSORS_MAX;
  }
  cycleRunning = false;
  // Carpas con el suelo desactivado quedan sin lectura.
  for (uint8_t t = 0; t < NUM_TENTS; t++)
    if (!settings.tents[t].soil.enabled || !settings.tents[t].enabled) data[t] = {};
}

const SoilReading& soil(uint8_t tent) { return data[tent < NUM_TENTS ? tent : 0]; }
bool soilPresent(uint8_t tent) { return tent < NUM_TENTS && present[tent]; }
