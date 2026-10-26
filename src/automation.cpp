#include "automation.h"
#include "clock.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"

static const char* reasons[NUM_RELAYS];
static uint32_t nextLightChange[NUM_RELAYS];
static bool isDay[NUM_TENTS] = {true, true};
static uint32_t lastRun = 0;

// Estados con histéresis por carpa, para no prender y apagar a cada rato cerca del límite.
static bool hot[NUM_TENTS], cold[NUM_TENTS], humHigh[NUM_TENTS], humLow[NUM_TENTS];

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
  bool anyLight[NUM_TENTS] = {}, anyOn[NUM_TENTS] = {};
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    const RelayConfig& r = settings.relays[i];
    nextLightChange[i] = 0;
    if (r.type != RelayType::Light) continue;
    anyLight[r.tent] = true;
    bool on = false;
    if (!clockOk) {
      reasons[i] = "sin hora";
    } else {
      on = lightOn(r.light, now, nextLightChange[i]);
      reasons[i] = on ? "horario: encendida" : "horario: apagada";
    }
    relaySetAuto(i, on);
    if (r.mode != RelayMode::Off && relayState(i)) anyOn[r.tent] = true;
  }
  // Sin luces controladas por el equipo se usan siempre los objetivos de día.
  for (uint8_t t = 0; t < NUM_TENTS; t++) isDay[t] = !anyLight[t] || anyOn[t];
}

static void updateClimate(time_t now, bool clockOk) {
  bool vent[NUM_TENTS] = {};
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    const Climate& c = climate(t);
    const ClimateConfig& cfg = settings.tents[t].climate;
    const ClimateTargets& tg = isDay[t] ? cfg.day : cfg.night;
    if (c.valid) {
      hysteresis(hot[t], c.temperature > tg.tempMax, c.temperature < tg.tempMax - cfg.tempHyst);
      hysteresis(cold[t], c.temperature < tg.tempMin, c.temperature > tg.tempMin + cfg.tempHyst);
      hysteresis(humHigh[t], c.humidity > tg.humMax, c.humidity < tg.humMax - cfg.humHyst);
      hysteresis(humLow[t], c.humidity < tg.humMin, c.humidity > tg.humMin + cfg.humHyst);
    }
    if (cfg.ventOnMin && cfg.ventEveryMin) {
      uint32_t minutes = clockOk ? now / 60 : millis() / 60000;
      vent[t] = minutes % cfg.ventEveryMin < cfg.ventOnMin;
    }
  }

  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    bool on = false;
    const char* why = nullptr;
    uint8_t t = settings.relays[i].tent;
    const Climate& c = climate(t);
    switch (settings.relays[i].type) {
      case RelayType::FanOut:
      case RelayType::FanIn:
        if (!c.valid) on = true, why = "sin sensor: ventilando";
        else if (hot[t]) on = true, why = "temperatura alta";
        else if (humHigh[t]) on = true, why = "humedad alta";
        else if (vent[t]) on = true, why = "renovación de aire";
        else why = "clima en rango";
        break;
      case RelayType::Humidifier:
        on = c.valid && humLow[t];
        why = !c.valid ? "sin sensor" : on ? "humedad baja" : "humedad en rango";
        break;
      case RelayType::Dehumidifier:
        on = c.valid && humHigh[t];
        why = !c.valid ? "sin sensor" : on ? "humedad alta" : "humedad en rango";
        break;
      case RelayType::Heater:
        on = c.valid && cold[t];
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

bool automationIsDay(uint8_t tent) { return isDay[tent < NUM_TENTS ? tent : 0]; }
const char* automationReason(uint8_t i) { return i < NUM_RELAYS && reasons[i] ? reasons[i] : ""; }
uint32_t automationNextLightChange(uint8_t i) { return i < NUM_RELAYS ? nextLightChange[i] : 0; }
