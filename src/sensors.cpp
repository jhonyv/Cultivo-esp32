#include "sensors.h"
#include <Wire.h>
#include "settings.h"

// Driver mínimo para SHT3x y SHT4x (mismo formato de respuesta), en 0x44 o 0x45.

static const uint8_t ADDRESSES[SENSORS_PER_TENT] = {0x44, 0x45};
static const uint8_t MAX_FAILS = 5;            // lecturas fallidas seguidas antes de invalidar el sensor
static const uint32_t DETECT_RETRY_MS = 30000;  // cada cuánto buscar sensores que no están

enum class Model : uint8_t { None, Sht3x, Sht4x };

struct Sensor {
  TwoWire* bus;
  uint8_t addr;
  Model model;
  uint8_t fails;
  bool valid;
  float t, h;
  uint32_t lastDetect;
};

static Sensor sensors[NUM_TENTS][SENSORS_PER_TENT];
static Climate data[NUM_TENTS] = {};
static uint32_t lastRead = 0;

static uint8_t crc8(const uint8_t* buf, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
  }
  return crc;
}

static bool readRaw(TwoWire& bus, uint8_t addr, Model m, uint16_t& rawT, uint16_t& rawH) {
  bus.beginTransmission(addr);
  if (m == Model::Sht4x) {
    bus.write(0xFD);  // medición alta precisión
  } else {
    bus.write(0x24);  // single shot, alta repetibilidad, sin clock stretching
    bus.write(0x00);
  }
  if (bus.endTransmission() != 0) return false;
  delay(m == Model::Sht4x ? 10 : 16);

  uint8_t buf[6];
  if (bus.requestFrom(addr, (uint8_t)6) != 6) return false;
  for (uint8_t i = 0; i < 6; i++) buf[i] = bus.read();
  if (crc8(buf, 2) != buf[2] || crc8(buf + 3, 2) != buf[5]) return false;
  rawT = (buf[0] << 8) | buf[1];
  rawH = (buf[3] << 8) | buf[4];
  return true;
}

static float svp(float t) { return 0.61078f * expf(17.27f * t / (t + 237.3f)); }

float calcVpd(float airTemp, float rh, float leafOffset) {
  float v = svp(airTemp + leafOffset) - svp(airTemp) * rh / 100.0f;
  return v < 0 ? 0 : v;
}

static float calcDewPoint(float t, float rh) {
  float g = logf(rh / 100.0f) + 17.27f * t / (t + 237.3f);
  return 237.3f * g / (17.27f - g);
}

static const char* modelName(Model m) {
  switch (m) {
    case Model::Sht4x: return "SHT4x";
    case Model::Sht3x: return "SHT3x";
    default: return "ninguno";
  }
}

static void detect(Sensor& x) {
  // Primero se pregunta por si responde la dirección, para no esperar mediciones de un sensor que no está.
  x.lastDetect = millis();
  x.bus->beginTransmission(x.addr);
  if (x.bus->endTransmission() != 0) {
    x.model = Model::None;
    return;
  }
  uint16_t t, h;
  if (readRaw(*x.bus, x.addr, Model::Sht4x, t, h)) x.model = Model::Sht4x;
  else if (readRaw(*x.bus, x.addr, Model::Sht3x, t, h)) x.model = Model::Sht3x;
  else x.model = Model::None;
  if (x.model != Model::None) Serial.printf("[sensor] %s en 0x%02X (bus %d)\n", modelName(x.model), x.addr, x.bus == &Wire ? 1 : 2);
}

static void readSensor(Sensor& x) {
  if (x.model == Model::None) {
    x.valid = false;
    if (x.lastDetect && millis() - x.lastDetect < DETECT_RETRY_MS) return;
    detect(x);
    if (x.model == Model::None) return;
  }
  uint16_t rawT, rawH;
  if (!readRaw(*x.bus, x.addr, x.model, rawT, rawH)) {
    if (++x.fails >= MAX_FAILS) {
      x.valid = false;
      x.model = Model::None;  // reintentar detección (sensor desconectado/reconectado)
      x.fails = 0;
      Serial.printf("[sensor] 0x%02X sin lecturas válidas\n", x.addr);
    }
    return;
  }
  x.fails = 0;
  x.t = -45.0f + 175.0f * rawT / 65535.0f;
  float h = x.model == Model::Sht4x ? -6.0f + 125.0f * rawH / 65535.0f : 100.0f * rawH / 65535.0f;
  x.h = constrain(h, 0.0f, 100.0f);
  x.valid = true;
}

void sensorsBegin() {
  Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL, I2C_FREQ);
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    for (uint8_t k = 0; k < SENSORS_PER_TENT; k++) {
      sensors[t][k] = {t == 0 ? &Wire : &Wire1, ADDRESSES[k], Model::None, 0, false, 0, 0, 0};
    }
  }
  sensorsLoop();
}

void sensorsLoop() {
  if (lastRead && millis() - lastRead < SENSOR_INTERVAL_MS) return;
  lastRead = millis();

  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    float sumT = 0, sumH = 0;
    uint8_t n = 0;
    for (uint8_t k = 0; k < SENSORS_PER_TENT; k++) {
      Sensor& x = sensors[t][k];
      readSensor(x);
      if (!x.valid) continue;
      sumT += x.t;
      sumH += x.h;
      n++;
    }
    Climate& c = data[t];
    if (!n) {
      c.valid = false;
      continue;
    }
    c.temperature = sumT / n;
    c.humidity = sumH / n;
    c.vpd = calcVpd(c.temperature, c.humidity, settings.leafOffset);
    c.dewPoint = c.humidity > 0 ? calcDewPoint(c.temperature, c.humidity) : NAN;
    c.updatedMs = millis();
    c.valid = true;
  }
}

const Climate& climate(uint8_t tent) { return data[tent < NUM_TENTS ? tent : 0]; }

SensorReading sensorReading(uint8_t tent, uint8_t index) {
  const Sensor& x = sensors[tent][index];
  return {modelName(x.model), x.addr, x.valid, x.t, x.h};
}
