#include "history.h"
#include <LittleFS.h>
#include "clock.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"

struct Sample {
  int16_t t;  // °C x10
  int16_t h;  // % x10
  int16_t v;  // kPa x100
};

struct __attribute__((packed)) HourRecord {
  uint32_t hour;  // epoch / 3600 (0 = vacío)
  int16_t tAvg, tMin, tMax;  // °C x10
  int16_t hAvg, hMin, hMax;  // % x10
  int16_t vAvg;              // kPa x100
  uint16_t lightMin, fanMin;
  uint16_t irrigSec;
};

static const char* HOURLY_FILE = "/hourly.bin";

static Sample samples[HISTORY_SIZE];
static uint16_t head = 0, count = 0;
static uint32_t lastSample = 0;

static bool fsOk = false;
static uint32_t lastTick = 0;

// Acumulador de la hora en curso.
static struct {
  uint32_t hour;
  float tSum, hSum, vSum, tMin, tMax, hMin, hMax;
  uint32_t n, lightSec, fanSec, irrigSec;
} acc = {};

static void accReset(uint32_t hour) {
  acc = {};
  acc.hour = hour;
  acc.tMin = acc.hMin = 1e9;
  acc.tMax = acc.hMax = -1e9;
}

static void flushHour() {
  if (!fsOk || !acc.hour || (!acc.n && !acc.lightSec && !acc.fanSec && !acc.irrigSec)) return;
  HourRecord r = {};
  r.hour = acc.hour;
  if (acc.n) {
    r.tAvg = lroundf(acc.tSum / acc.n * 10);
    r.tMin = lroundf(acc.tMin * 10);
    r.tMax = lroundf(acc.tMax * 10);
    r.hAvg = lroundf(acc.hSum / acc.n * 10);
    r.hMin = lroundf(acc.hMin * 10);
    r.hMax = lroundf(acc.hMax * 10);
    r.vAvg = lroundf(acc.vSum / acc.n * 100);
  } else {
    r.tAvg = r.tMin = r.tMax = r.hAvg = r.hMin = r.hMax = r.vAvg = INT16_MIN;  // sin sensor esa hora
  }
  r.lightMin = acc.lightSec / 60;
  r.fanMin = acc.fanSec / 60;
  r.irrigSec = min<uint32_t>(acc.irrigSec, 65535);

  File f = LittleFS.open(HOURLY_FILE, "r+");
  if (!f) return;
  f.seek((acc.hour % HISTORY_HOURS) * sizeof(HourRecord));
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
}

void historyBegin() {
  fsOk = LittleFS.begin(true);
  if (!fsOk) {
    Serial.println("[hist] no se pudo montar LittleFS, sin historial semanal/mensual");
    return;
  }
  File f = LittleFS.open(HOURLY_FILE, "r");
  size_t size = f ? f.size() : 0;
  if (f) f.close();
  if (size != HISTORY_HOURS * sizeof(HourRecord)) {
    f = LittleFS.open(HOURLY_FILE, "w");
    HourRecord empty = {};
    for (uint32_t i = 0; i < HISTORY_HOURS; i++) f.write((const uint8_t*)&empty, sizeof(empty));
    f.close();
    Serial.println("[hist] archivo de historial creado");
  }
}

static void sampleRam(const Climate& c) {
  if (count && millis() - lastSample < HISTORY_INTERVAL_S * 1000UL) return;
  lastSample = millis();
  samples[head] = {(int16_t)lroundf(c.temperature * 10), (int16_t)lroundf(c.humidity * 10), (int16_t)lroundf(c.vpd * 100)};
  head = (head + 1) % HISTORY_SIZE;
  if (count < HISTORY_SIZE) count++;
}

static void tickHourly(const Climate& c) {
  if (!clockValid()) return;
  uint32_t hour = time(nullptr) / 3600;
  if (hour != acc.hour) {
    flushHour();
    accReset(hour);
  }
  if (c.valid) {
    acc.tSum += c.temperature;
    acc.hSum += c.humidity;
    acc.vSum += c.vpd;
    acc.tMin = min(acc.tMin, c.temperature);
    acc.tMax = max(acc.tMax, c.temperature);
    acc.hMin = min(acc.hMin, c.humidity);
    acc.hMax = max(acc.hMax, c.humidity);
    acc.n++;
  }
  bool light = false, fan = false, irrig = false;
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    if (!relayState(i)) continue;
    switch (settings.relays[i].type) {
      case RelayType::Light: light = true; break;
      case RelayType::FanIn:
      case RelayType::FanOut: fan = true; break;
      case RelayType::Irrigation: irrig = true; break;
      default: break;
    }
  }
  acc.lightSec += light;
  acc.fanSec += fan;
  acc.irrigSec += irrig;
}

void historyLoop() {
  if (lastTick && millis() - lastTick < 1000) return;
  lastTick = millis();
  const Climate& c = climate();
  if (c.valid) sampleRam(c);
  tickHourly(c);
}

void historyToJson(JsonObject obj) {
  obj["interval"] = HISTORY_INTERVAL_S;
  obj["age"] = count ? (millis() - lastSample) / 1000 : 0;  // segundos desde la última muestra
  JsonArray t = obj["t"].to<JsonArray>();
  JsonArray h = obj["h"].to<JsonArray>();
  JsonArray v = obj["v"].to<JsonArray>();
  for (uint16_t k = 0; k < count; k++) {
    const Sample& s = samples[(head + HISTORY_SIZE - count + k) % HISTORY_SIZE];
    t.add(s.t / 10.0f);
    h.add(s.h / 10.0f);
    v.add(s.v / 100.0f);
  }
}

// Recorre las horas guardadas desde hace `hours` horas hasta la actual (incluida la que está en curso).
template <typename F>
static void forEachHour(uint16_t hours, F fn) {
  if (!fsOk || !clockValid()) return;
  flushHour();  // para incluir la hora en curso
  uint32_t now = time(nullptr) / 3600;
  File f = LittleFS.open(HOURLY_FILE, "r");
  if (!f) return;
  for (uint32_t hr = now - hours + 1; hr <= now; hr++) {
    HourRecord r;
    if (!f.seek((hr % HISTORY_HOURS) * sizeof(r)) || f.read((uint8_t*)&r, sizeof(r)) != sizeof(r)) break;
    if (r.hour == hr) fn(r);
  }
  f.close();
}

static void addClimate(JsonArray arr, int16_t v, float scale) {
  if (v == INT16_MIN) arr.add(nullptr);
  else arr.add(v / scale);
}

void historyHourlyToJson(JsonObject obj, uint16_t hours) {
  JsonArray ts = obj["ts"].to<JsonArray>();
  JsonArray t = obj["t"].to<JsonArray>(), tn = obj["tn"].to<JsonArray>(), tx = obj["tx"].to<JsonArray>();
  JsonArray h = obj["h"].to<JsonArray>(), hn = obj["hn"].to<JsonArray>(), hx = obj["hx"].to<JsonArray>();
  JsonArray v = obj["v"].to<JsonArray>();
  JsonArray light = obj["light"].to<JsonArray>(), fan = obj["fan"].to<JsonArray>(), irr = obj["irr"].to<JsonArray>();
  forEachHour(hours, [&](const HourRecord& r) {
    ts.add(r.hour * 3600);
    addClimate(t, r.tAvg, 10);
    addClimate(tn, r.tMin, 10);
    addClimate(tx, r.tMax, 10);
    addClimate(h, r.hAvg, 10);
    addClimate(hn, r.hMin, 10);
    addClimate(hx, r.hMax, 10);
    addClimate(v, r.vAvg, 100);
    light.add(r.lightMin);
    fan.add(r.fanMin);
    irr.add(r.irrigSec);
  });
}

void historyDailyToJson(JsonObject obj, uint8_t days) {
  struct Day {
    int32_t key;  // año*1000 + día del año (hora local)
    uint32_t ts;
    float tSum, hSum, vSum, tMin, tMax, hMin, hMax;
    uint16_t n;
    uint32_t light, fan, irr;
  };
  static Day d[36];
  uint8_t nd = 0;
  forEachHour((uint16_t)days * 24 + 23, [&](const HourRecord& r) {
    time_t when = (time_t)r.hour * 3600;
    struct tm tm;
    localtime_r(&when, &tm);
    int32_t key = tm.tm_year * 1000 + tm.tm_yday;
    if (!nd || d[nd - 1].key != key) {
      if (nd == 36) return;
      tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
      d[nd] = {key, (uint32_t)mktime(&tm), 0, 0, 0, 1e9, -1e9, 1e9, -1e9, 0, 0, 0, 0};
      nd++;
    }
    Day& x = d[nd - 1];
    if (r.tAvg != INT16_MIN) {
      x.tSum += r.tAvg / 10.0f;
      x.hSum += r.hAvg / 10.0f;
      x.vSum += r.vAvg / 100.0f;
      x.tMin = min(x.tMin, r.tMin / 10.0f);
      x.tMax = max(x.tMax, r.tMax / 10.0f);
      x.hMin = min(x.hMin, r.hMin / 10.0f);
      x.hMax = max(x.hMax, r.hMax / 10.0f);
      x.n++;
    }
    x.light += r.lightMin;
    x.fan += r.fanMin;
    x.irr += r.irrigSec;
  });

  uint8_t first = nd > days ? nd - days : 0;
  JsonArray arr = obj["days"].to<JsonArray>();
  for (uint8_t k = first; k < nd; k++) {
    const Day& x = d[k];
    JsonObject o = arr.add<JsonObject>();
    o["ts"] = x.ts;
    if (x.n) {
      o["t"] = serialized(String(x.tSum / x.n, 1));
      o["tn"] = serialized(String(x.tMin, 1));
      o["tx"] = serialized(String(x.tMax, 1));
      o["h"] = serialized(String(x.hSum / x.n, 1));
      o["hn"] = serialized(String(x.hMin, 1));
      o["hx"] = serialized(String(x.hMax, 1));
      o["v"] = serialized(String(x.vSum / x.n, 2));
    }
    o["light"] = x.light;  // minutos
    o["fan"] = x.fan;      // minutos
    o["irr"] = x.irr;      // segundos
  }
}
