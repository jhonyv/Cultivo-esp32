#include "sensors.h"
#include <Wire.h>
#include "settings.h"

// Drivers mínimos: SHT3x/SHT4x (temperatura y humedad), SCD4x (CO2, también mide T/H) y BH1750 (luz).
// Cada carpa tiene su bus; en cada uno se prueban estas direcciones y se usa lo que responda.

enum class Model : uint8_t { None, Sht3x, Sht4x, Scd4x, Bh1750 };
enum class Family : uint8_t { Sht, Scd, Bh };

struct Slot {
  Family family;
  uint8_t addr;
};
static const Slot SLOTS[SENSOR_SLOTS] = {
    {Family::Sht, 0x44}, {Family::Sht, 0x45}, {Family::Scd, 0x62}, {Family::Bh, 0x23}, {Family::Bh, 0x5C}};

static const uint8_t MAX_FAILS = 5;             // lecturas fallidas seguidas antes de darlo por desconectado
static const uint32_t DETECT_RETRY_MS = 30000;  // cada cuánto buscar sensores que no están
static const uint32_t SCD_STALE_MS = 30000;     // el SCD4x mide cada 5 s; sin datos en 30 s se invalida

struct Sensor {
  TwoWire* bus;
  Family family;
  uint8_t addr;
  Model model;
  uint8_t fails;
  bool valid;
  float t, h, co2, lux;
  uint32_t lastDetect, lastData;
};

static Sensor sensors[NUM_TENTS][SENSOR_SLOTS];
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

static bool ack(TwoWire& bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

// ---------- SHT3x / SHT4x ----------
static bool shtRaw(TwoWire& bus, uint8_t addr, Model m, uint16_t& rawT, uint16_t& rawH) {
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

static bool shtRead(Sensor& x) {
  uint16_t rawT, rawH;
  if (!shtRaw(*x.bus, x.addr, x.model, rawT, rawH)) return false;
  x.t = -45.0f + 175.0f * rawT / 65535.0f;
  float h = x.model == Model::Sht4x ? -6.0f + 125.0f * rawH / 65535.0f : 100.0f * rawH / 65535.0f;
  x.h = constrain(h, 0.0f, 100.0f);
  return true;
}

// ---------- SCD4x ----------
static bool scdCommand(TwoWire& bus, uint16_t cmd) {
  bus.beginTransmission(0x62);
  bus.write(cmd >> 8);
  bus.write(cmd & 0xFF);
  return bus.endTransmission() == 0;
}

// Lee n palabras de 16 bits (cada una con su CRC) después de un comando.
static bool scdReadWords(TwoWire& bus, uint16_t cmd, uint16_t* words, uint8_t n) {
  if (!scdCommand(bus, cmd)) return false;
  delay(2);
  uint8_t len = n * 3;
  if (bus.requestFrom((uint8_t)0x62, len) != len) return false;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t b[3] = {(uint8_t)bus.read(), (uint8_t)bus.read(), (uint8_t)bus.read()};
    if (crc8(b, 2) != b[2]) return false;
    words[i] = (b[0] << 8) | b[1];
  }
  return true;
}

static bool scdStart(TwoWire& bus) {
  scdCommand(bus, 0x3F86);  // stop_periodic_measurement (por si quedó midiendo de antes)
  delay(500);
  uint16_t serial[3];
  if (!scdReadWords(bus, 0x3682, serial, 3)) return false;  // get_serial_number: confirma que es un SCD4x
  return scdCommand(bus, 0x21B1);                           // start_periodic_measurement (una cada 5 s)
}

// true = sin error de bus (haya o no dato nuevo).
static bool scdRead(Sensor& x) {
  uint16_t ready;
  if (!scdReadWords(*x.bus, 0xE4B8, &ready, 1)) return false;  // get_data_ready_status
  if ((ready & 0x07FF) == 0) return true;
  uint16_t w[3];
  if (!scdReadWords(*x.bus, 0xEC05, w, 3)) return false;  // read_measurement
  x.co2 = w[0];
  x.t = -45.0f + 175.0f * w[1] / 65535.0f;
  x.h = constrain(100.0f * w[2] / 65535.0f, 0.0f, 100.0f);
  x.lastData = millis();
  return true;
}

// ---------- BH1750 ----------
static bool bhCommand(TwoWire& bus, uint8_t addr, uint8_t cmd) {
  bus.beginTransmission(addr);
  bus.write(cmd);
  return bus.endTransmission() == 0;
}

static bool bhStart(TwoWire& bus, uint8_t addr) {
  return bhCommand(bus, addr, 0x01) && bhCommand(bus, addr, 0x10);  // encender + continuo alta resolución
}

static bool bhRead(Sensor& x) {
  if (x.bus->requestFrom(x.addr, (uint8_t)2) != 2) return false;
  uint16_t raw = (x.bus->read() << 8) | x.bus->read();
  x.lux = raw / 1.2f;
  x.lastData = millis();
  return true;
}

// ---------- detección y lectura ----------
static const char* modelName(Model m) {
  switch (m) {
    case Model::Sht4x: return "SHT4x";
    case Model::Sht3x: return "SHT3x";
    case Model::Scd4x: return "SCD4x";
    case Model::Bh1750: return "BH1750";
    default: return "";
  }
}

static void detect(Sensor& x) {
  // Primero se pregunta si responde la dirección, para no perder tiempo con lo que no está.
  x.lastDetect = millis();
  x.model = Model::None;
  if (!ack(*x.bus, x.addr)) return;
  switch (x.family) {
    case Family::Sht: {
      uint16_t t, h;
      if (shtRaw(*x.bus, x.addr, Model::Sht4x, t, h)) x.model = Model::Sht4x;
      else if (shtRaw(*x.bus, x.addr, Model::Sht3x, t, h)) x.model = Model::Sht3x;
      break;
    }
    case Family::Scd:
      if (scdStart(*x.bus)) x.model = Model::Scd4x;
      break;
    case Family::Bh:
      if (bhStart(*x.bus, x.addr)) x.model = Model::Bh1750;
      break;
  }
  x.fails = 0;
  x.lastData = 0;
  if (x.model != Model::None) Serial.printf("[sensor] %s en 0x%02X (bus %d)\n", modelName(x.model), x.addr, x.bus == &Wire ? 1 : 2);
}

static void readSensor(Sensor& x) {
  if (x.model == Model::None) {
    x.valid = false;
    if (x.lastDetect && millis() - x.lastDetect < DETECT_RETRY_MS) return;
    detect(x);
    if (x.model == Model::None) return;
  }
  bool ok;
  switch (x.model) {
    case Model::Scd4x: ok = scdRead(x); break;
    case Model::Bh1750: ok = bhRead(x); break;
    default: ok = shtRead(x); if (ok) x.lastData = millis(); break;
  }
  if (!ok) {
    if (++x.fails >= MAX_FAILS) {
      Serial.printf("[sensor] %s en 0x%02X dejó de responder\n", modelName(x.model), x.addr);
      x.valid = false;
      x.model = Model::None;  // se vuelve a buscar (desenchufado o reconectado)
      x.fails = 0;
    }
    return;
  }
  x.fails = 0;
  x.valid = x.lastData && millis() - x.lastData < SCD_STALE_MS;
}

void sensorsBegin() {
  Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL, I2C_FREQ);
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    for (uint8_t k = 0; k < SENSOR_SLOTS; k++) {
      Sensor& x = sensors[t][k];
      x = {};
      x.bus = t == 0 ? &Wire : &Wire1;
      x.family = SLOTS[k].family;
      x.addr = SLOTS[k].addr;
    }
  }
  sensorsLoop();
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

void sensorsLoop() {
  if (lastRead && millis() - lastRead < SENSOR_INTERVAL_MS) return;
  lastRead = millis();

  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    float sumT = 0, sumH = 0, scdT = 0, scdH = 0, lux = 0;
    uint8_t n = 0, nLux = 0;
    Climate& c = data[t];
    c.hasCo2 = false;
    for (uint8_t k = 0; k < SENSOR_SLOTS; k++) {
      Sensor& x = sensors[t][k];
      readSensor(x);
      if (!x.valid) continue;
      if (x.model == Model::Scd4x) {
        c.hasCo2 = true;
        c.co2 = x.co2;
        scdT = x.t;
        scdH = x.h;
      } else if (x.model == Model::Bh1750) {
        lux += x.lux;
        nLux++;
      } else {
        sumT += x.t;
        sumH += x.h;
        n++;
      }
    }
    c.hasLux = nLux > 0;
    c.lux = nLux ? lux / nLux : 0;
    // El SCD4x también mide temperatura y humedad, pero se calienta un poco: se usa solo si no hay SHT.
    if (!n && c.hasCo2) {
      sumT = scdT;
      sumH = scdH;
      n = 1;
    }
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

SensorReading sensorReading(uint8_t tent, uint8_t slot) {
  const Sensor& x = sensors[tent][slot];
  SensorKind kind = x.family == Family::Scd ? SensorKind::Co2 : x.family == Family::Bh ? SensorKind::Light : SensorKind::TempHum;
  return {x.model != Model::None, kind, modelName(x.model), x.addr, x.valid, x.t, x.h, x.co2, x.lux};
}
