#include "automation.h"
#include "clock.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"

static const char* reasons[NUM_RELAYS];
static uint32_t nextLightChange[NUM_RELAYS];
static bool isDay = true;
static uint32_t lastRun = 0;

// Estados con histéresis, para no prender y apagar a cada rato cerca del límite.
static bool hot = false, cold = false, humHigh = false, humLow = false;

static void hysteresis(bool& state, bool turnOn, bool turnOff) {
  if (turnOn) state = true;
  else if (turnOff) state = false;
}

static bool lightOn(const LightSchedule& l, time_t now, uint32_t& untilChange) {
  int64_t cycle = (int64_t)(l.onMin + l.offMin) * 60;
  int64_t pos = ((int64_t)now - (int64_t)l.anchor) % cycle;
  if (pos < 0) pos += cycle;
  bool on = pos < (int64_t)l.onMin * 60;
  untilChange = on ? l.onMin * 60 - pos : cycle - pos;
  return on;
}

static bool irrigationOn(const RelayConfig& r, time_t now) {
  struct tm tm;
  localtime_r(&now, &tm);
  int32_t sec = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
  for (uint8_t e = 0; e < r.eventCount; e++) {
    const IrrigationEvent& ev = r.events[e];
    if (!ev.enabled) continue;
    int32_t since = (sec - ev.startMin * 60 + 86400) % 86400;
    if (since < ev.durSec) return true;
  }
  return false;
}

static void updateLights(time_t now, bool clockOk) {
  bool anyLight = false, anyOn = false;
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    const RelayConfig& r = settings.relays[i];
    nextLightChange[i] = 0;
    if (r.type != RelayType::Light) continue;
    anyLight = true;
    bool on = false;
    if (!clockOk) {
      reasons[i] = "sin hora";
    } else {
      on = lightOn(r.light, now, nextLightChange[i]);
      reasons[i] = on ? "horario: encendida" : "horario: apagada";
    }
    relaySetAuto(i, on);
    if (r.mode != RelayMode::Off && relayState(i)) anyOn = true;
  }
  // Sin luces controladas por el equipo se usan siempre los objetivos de día.
  isDay = !anyLight || anyOn;
}

static void updateClimate(time_t now, bool clockOk) {
  const Climate& c = climate();
  const ClimateConfig& cfg = settings.climate;
  const ClimateTargets& t = isDay ? cfg.day : cfg.night;

  if (c.valid) {
    hysteresis(hot, c.temperature > t.tempMax, c.temperature < t.tempMax - cfg.tempHyst);
    hysteresis(cold, c.temperature < t.tempMin, c.temperature > t.tempMin + cfg.tempHyst);
    hysteresis(humHigh, c.humidity > t.humMax, c.humidity < t.humMax - cfg.humHyst);
    hysteresis(humLow, c.humidity < t.humMin, c.humidity > t.humMin + cfg.humHyst);
  }

  bool vent = false;
  if (cfg.ventOnMin && cfg.ventEveryMin) {
    uint32_t minutes = clockOk ? now / 60 : millis() / 60000;
    vent = minutes % cfg.ventEveryMin < cfg.ventOnMin;
  }

  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    bool on = false;
    const char* why = nullptr;
    switch (settings.relays[i].type) {
      case RelayType::FanOut:
      case RelayType::FanIn:
        if (!c.valid) on = true, why = "sin sensor: ventilando";
        else if (hot) on = true, why = "temperatura alta";
        else if (humHigh) on = true, why = "humedad alta";
        else if (vent) on = true, why = "renovación de aire";
        else why = "clima en rango";
        break;
      case RelayType::Humidifier:
        on = c.valid && humLow;
        why = !c.valid ? "sin sensor" : on ? "humedad baja" : "humedad en rango";
        break;
      case RelayType::Dehumidifier:
        on = c.valid && humHigh;
        why = !c.valid ? "sin sensor" : on ? "humedad alta" : "humedad en rango";
        break;
      case RelayType::Heater:
        on = c.valid && cold;
        why = !c.valid ? "sin sensor" : on ? "temperatura baja" : "temperatura en rango";
        break;
      default:
        continue;
    }
    relaySetAuto(i, on);
    reasons[i] = why;
  }
}

static void updateIrrigation(time_t now, bool clockOk) {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    const RelayConfig& r = settings.relays[i];
    if (r.type != RelayType::Irrigation) continue;
    bool on = clockOk && irrigationOn(r, now);
    relaySetAuto(i, on);
    reasons[i] = !clockOk ? "sin hora" : on ? "regando" : "esperando próximo riego";
  }
}

void automationLoop() {
  if (lastRun && millis() - lastRun < 1000) return;
  lastRun = millis();

  time_t now = time(nullptr);
  bool clockOk = clockValid();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    if (settings.relays[i].type == RelayType::Generic) {
      relaySetAuto(i, false);
      reasons[i] = "sin función asignada";
    }
  }
  updateLights(now, clockOk);
  updateClimate(now, clockOk);
  updateIrrigation(now, clockOk);
}

bool automationIsDay() { return isDay; }
const char* automationReason(uint8_t i) { return i < NUM_RELAYS && reasons[i] ? reasons[i] : ""; }
uint32_t automationNextLightChange(uint8_t i) { return i < NUM_RELAYS ? nextLightChange[i] : 0; }
