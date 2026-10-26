#include "history.h"
#include "sensors.h"

struct Sample {
  int16_t t;  // °C x10
  int16_t h;  // % x10
  int16_t v;  // kPa x100
};

static Sample samples[HISTORY_SIZE];
static uint16_t head = 0, count = 0;
static uint32_t lastSample = 0;

void historyLoop() {
  const Climate& c = climate();
  if (!c.valid) return;
  if (count && millis() - lastSample < HISTORY_INTERVAL_S * 1000UL) return;
  lastSample = millis();
  samples[head] = {(int16_t)lroundf(c.temperature * 10), (int16_t)lroundf(c.humidity * 10), (int16_t)lroundf(c.vpd * 100)};
  head = (head + 1) % HISTORY_SIZE;
  if (count < HISTORY_SIZE) count++;
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
