#include "sensors.h"
#include <Wire.h>
#include "config.h"
#include "settings.h"

// Driver mínimo para SHT3x y SHT4x (ambos en 0x44, mismo formato de respuesta).

static const uint8_t SHT_ADDR = 0x44;
static const uint8_t MAX_FAILS = 5;  // lecturas fallidas seguidas antes de invalidar los datos

enum class Model : uint8_t { None, Sht3x, Sht4x };
static Model model = Model::None;
static Climate data = {};
static uint32_t lastRead = 0;
static uint8_t fails = 0;

static uint8_t crc8(const uint8_t* buf, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
  }
  return crc;
}

static bool readRaw(Model m, uint16_t& rawT, uint16_t& rawH) {
  Wire.beginTransmission(SHT_ADDR);
  if (m == Model::Sht4x) {
    Wire.write(0xFD);  // medición alta precisión
  } else {
    Wire.write(0x24);  // single shot, alta repetibilidad, sin clock stretching
    Wire.write(0x00);
  }
  if (Wire.endTransmission() != 0) return false;
  delay(m == Model::Sht4x ? 10 : 16);

  uint8_t buf[6];
  if (Wire.requestFrom(SHT_ADDR, (uint8_t)6) != 6) return false;
  for (uint8_t i = 0; i < 6; i++) buf[i] = Wire.read();
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

static void detect() {
  uint16_t t, h;
  if (readRaw(Model::Sht4x, t, h)) model = Model::Sht4x;
  else if (readRaw(Model::Sht3x, t, h)) model = Model::Sht3x;
  else model = Model::None;
  Serial.printf("[sensor] detectado: %s\n", sensorModel());
}

void sensorsBegin() {
  detect();
  sensorsLoop();
}

void sensorsLoop() {
  if (lastRead && millis() - lastRead < SENSOR_INTERVAL_MS) return;
  lastRead = millis();

  if (model == Model::None) {
    detect();
    if (model == Model::None) {
      data.valid = false;
      return;
    }
  }

  uint16_t rawT, rawH;
  if (!readRaw(model, rawT, rawH)) {
    if (++fails >= MAX_FAILS) {
      data.valid = false;
      model = Model::None;  // reintentar detección (sensor desconectado/reconectado)
      fails = 0;
      Serial.println("[sensor] sin lecturas válidas");
    }
    return;
  }
  fails = 0;

  float t = -45.0f + 175.0f * rawT / 65535.0f;
  float h = model == Model::Sht4x ? -6.0f + 125.0f * rawH / 65535.0f : 100.0f * rawH / 65535.0f;
  h = constrain(h, 0.0f, 100.0f);

  data.temperature = t;
  data.humidity = h;
  data.vpd = calcVpd(t, h, settings.leafOffset);
  data.dewPoint = h > 0 ? calcDewPoint(t, h) : NAN;
  data.updatedMs = millis();
  data.valid = true;
}

const Climate& climate() { return data; }

const char* sensorModel() {
  switch (model) {
    case Model::Sht4x: return "SHT4x";
    case Model::Sht3x: return "SHT3x";
    default: return "ninguno";
  }
}
