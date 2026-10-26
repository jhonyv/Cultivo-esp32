#include "settings.h"
#include <Preferences.h>
#include <memory>

Settings settings;

static const char* const TYPE_NAMES[] = {
    "generic", "light", "fan_in", "fan_out", "irrigation", "humidifier", "dehumidifier", "heater",
};
static const char* const MODE_NAMES[] = {"off", "on", "auto"};

const char* relayTypeName(RelayType t) { return TYPE_NAMES[(uint8_t)t]; }
const char* relayModeName(RelayMode m) { return MODE_NAMES[(uint8_t)m]; }

static bool relayTypeFromName(const char* s, RelayType& out) {
  for (uint8_t i = 0; i < sizeof(TYPE_NAMES) / sizeof(TYPE_NAMES[0]); i++) {
    if (strcmp(s, TYPE_NAMES[i]) == 0) {
      out = (RelayType)i;
      return true;
    }
  }
  return false;
}

bool relayModeFromName(const char* s, RelayMode& out) {
  for (uint8_t i = 0; i < 3; i++) {
    if (strcmp(s, MODE_NAMES[i]) == 0) {
      out = (RelayMode)i;
      return true;
    }
  }
  return false;
}

static int parseHHMM(const char* s) {
  int h, m;
  if (!s || sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

// Epoch del encendido: hoy a esa hora local (o 2024-01-01 si todavía no hay hora).
static uint32_t anchorFromStart(int startMin) {
  time_t now = time(nullptr);
  struct tm tm;
  if (now > 1704067200) {
    localtime_r(&now, &tm);
  } else {
    tm = {};
    tm.tm_year = 124;
    tm.tm_mday = 1;
  }
  tm.tm_hour = startMin / 60;
  tm.tm_min = startMin % 60;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

static void formatHHMM(char* buf, size_t len, int minutes) { snprintf(buf, len, "%02d:%02d", minutes / 60, minutes % 60); }

static void setRelay(uint8_t i, const char* name, RelayType type) {
  RelayConfig& r = settings.relays[i];
  strlcpy(r.name, name, sizeof(r.name));
  r.type = type;
  r.mode = RelayMode::Off;
  r.light = {18 * 60, 6 * 60, anchorFromStart(6 * 60)};
  r.eventCount = 0;
}

static void settingsDefaults() {
  setRelay(0, "Luz 1", RelayType::Light);
  setRelay(1, "Luz 2", RelayType::Light);
  setRelay(2, "Extractor", RelayType::FanOut);
  setRelay(3, "Intractor", RelayType::FanIn);
  setRelay(4, "Riego 1", RelayType::Irrigation);
  setRelay(5, "Riego 2", RelayType::Irrigation);
  setRelay(6, "Asignable 1", RelayType::Generic);
  setRelay(7, "Asignable 2", RelayType::Generic);
  settings.climate.day = {22, 28, 55, 70};
  settings.climate.night = {18, 24, 50, 70};
  settings.climate.tempHyst = 0.5f;
  settings.climate.humHyst = 3.0f;
  settings.climate.ventOnMin = 5;
  settings.climate.ventEveryMin = 30;
  settings.relayActiveLow = true;
  settings.leafOffset = -2.0f;
  strlcpy(settings.apPassword, AP_PASSWORD_DEFAULT, sizeof(settings.apPassword));
  strlcpy(settings.tz, TZ_DEFAULT, sizeof(settings.tz));
  settings.webPassword[0] = 0;
  settings.telegramToken[0] = 0;
  settings.telegramChat[0] = 0;
  settings.telegramAlerts = true;
}

static void lightToJson(const LightSchedule& l, JsonObject o) {
  o["on"] = l.onMin;
  o["off"] = l.offMin;
  o["anchor"] = l.anchor;
  // Hora de encendido local, para mostrar en la página.
  time_t a = l.anchor;
  struct tm tm;
  localtime_r(&a, &tm);
  char buf[6];
  formatHHMM(buf, sizeof(buf), tm.tm_hour * 60 + tm.tm_min);
  o["start"] = buf;
}

static void targetsToJson(const ClimateTargets& t, JsonObject o) {
  o["tMin"] = t.tempMin;
  o["tMax"] = t.tempMax;
  o["hMin"] = t.humMin;
  o["hMax"] = t.humMax;
}

static bool targetsFromJson(JsonObjectConst o, ClimateTargets& t, String& error) {
  if (o.isNull()) return true;
  ClimateTargets n = t;
  if (o["tMin"].is<float>()) n.tempMin = o["tMin"];
  if (o["tMax"].is<float>()) n.tempMax = o["tMax"];
  if (o["hMin"].is<float>()) n.humMin = o["hMin"];
  if (o["hMax"].is<float>()) n.humMax = o["hMax"];
  if (n.tempMin < 0 || n.tempMax > 45 || n.tempMin >= n.tempMax || n.humMin < 0 || n.humMax > 100 ||
      n.humMin >= n.humMax) {
    error = "Rangos de temperatura/humedad inválidos";
    return false;
  }
  t = n;
  return true;
}

static bool lightFromJson(JsonObjectConst o, LightSchedule& l, String& error) {
  if (o.isNull()) return true;
  LightSchedule n = l;
  if (o["on"].is<int>()) n.onMin = o["on"];
  if (o["off"].is<int>()) n.offMin = o["off"];
  if (o["anchor"].is<uint32_t>()) {
    n.anchor = o["anchor"];
  } else if (o["start"].is<const char*>()) {
    int start = parseHHMM(o["start"]);
    if (start < 0) {
      error = "Hora de encendido inválida";
      return false;
    }
    n.anchor = anchorFromStart(start);
  }
  if (n.onMin + n.offMin < 60 || n.onMin + n.offMin > 72 * 60) {
    error = "El ciclo de luz tiene que durar entre 1 y 72 horas";
    return false;
  }
  l = n;
  return true;
}

static bool eventsFromJson(JsonArrayConst arr, RelayConfig& r, String& error) {
  if (arr.isNull()) return true;
  if (arr.size() > MAX_IRRIGATION_EVENTS) {
    error = "Máximo " + String(MAX_IRRIGATION_EVENTS) + " riegos por salida";
    return false;
  }
  uint8_t n = 0;
  for (JsonObjectConst o : arr) {
    int start = parseHHMM(o["start"]);
    int dur = o["dur"] | 0;
    if (start < 0 || dur < 1 || dur > 3600) {
      error = "Riego inválido (hora HH:MM, duración 1-3600 s)";
      return false;
    }
    r.events[n++] = {(uint16_t)start, (uint16_t)dur, o["en"] | true};
  }
  r.eventCount = n;
  return true;
}

void settingsToJson(JsonObject obj, bool secrets) {
  obj["relayActiveLow"] = settings.relayActiveLow;
  obj["leafOffset"] = settings.leafOffset;
  obj["apPassword"] = settings.apPassword;
  obj["tz"] = settings.tz;
  obj["telegramChat"] = settings.telegramChat;
  obj["telegramAlerts"] = settings.telegramAlerts;
  if (secrets) {
    obj["webPassword"] = settings.webPassword;
    obj["telegramToken"] = settings.telegramToken;
  } else {
    obj["webPasswordSet"] = settings.webPassword[0] != 0;
    obj["telegramTokenSet"] = settings.telegramToken[0] != 0;
  }
  JsonArray arr = obj["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    JsonObject r = arr.add<JsonObject>();
    r["name"] = settings.relays[i].name;
    r["type"] = relayTypeName(settings.relays[i].type);
    r["mode"] = relayModeName(settings.relays[i].mode);
    lightToJson(settings.relays[i].light, r["light"].to<JsonObject>());
    JsonArray ev = r["events"].to<JsonArray>();
    for (uint8_t e = 0; e < settings.relays[i].eventCount; e++) {
      const IrrigationEvent& x = settings.relays[i].events[e];
      JsonObject o = ev.add<JsonObject>();
      char buf[6];
      formatHHMM(buf, sizeof(buf), x.startMin);
      o["start"] = buf;
      o["dur"] = x.durSec;
      o["en"] = x.enabled;
    }
  }
  JsonObject c = obj["climate"].to<JsonObject>();
  targetsToJson(settings.climate.day, c["day"].to<JsonObject>());
  targetsToJson(settings.climate.night, c["night"].to<JsonObject>());
  c["tempHyst"] = settings.climate.tempHyst;
  c["humHyst"] = settings.climate.humHyst;
  c["ventOn"] = settings.climate.ventOnMin;
  c["ventEvery"] = settings.climate.ventEveryMin;
}

bool settingsFromJson(JsonObjectConst obj, String& error) {
  // Se valida sobre una copia para no dejar la config a medio aplicar.
  Settings s = settings;

  if (obj["relayActiveLow"].is<bool>()) s.relayActiveLow = obj["relayActiveLow"];
  if (obj["leafOffset"].is<float>()) {
    float v = obj["leafOffset"];
    if (v < -5 || v > 5) {
      error = "leafOffset fuera de rango (-5..5)";
      return false;
    }
    s.leafOffset = v;
  }
  if (obj["apPassword"].is<const char*>()) {
    const char* p = obj["apPassword"];
    size_t len = strlen(p);
    if ((len > 0 && len < 8) || len > 32) {
      error = "La clave del AP debe tener entre 8 y 32 caracteres (o vacía)";
      return false;
    }
    strlcpy(s.apPassword, p, sizeof(s.apPassword));
  }
  if (obj["tz"].is<const char*>()) strlcpy(s.tz, obj["tz"], sizeof(s.tz));
  if (obj["webPassword"].is<const char*>()) strlcpy(s.webPassword, obj["webPassword"], sizeof(s.webPassword));
  if (obj["telegramToken"].is<const char*>()) strlcpy(s.telegramToken, obj["telegramToken"], sizeof(s.telegramToken));
  if (obj["telegramChat"].is<const char*>()) strlcpy(s.telegramChat, obj["telegramChat"], sizeof(s.telegramChat));
  if (obj["telegramAlerts"].is<bool>()) s.telegramAlerts = obj["telegramAlerts"];

  JsonArrayConst arr = obj["relays"];
  for (uint8_t i = 0; i < arr.size() && i < NUM_RELAYS; i++) {
    JsonObjectConst r = arr[i];
    if (r["name"].is<const char*>()) strlcpy(s.relays[i].name, r["name"], sizeof(s.relays[i].name));
    if (r["type"].is<const char*>() && !relayTypeFromName(r["type"], s.relays[i].type)) {
      error = String("Tipo inválido en relé ") + (i + 1);
      return false;
    }
    if (r["mode"].is<const char*>() && !relayModeFromName(r["mode"], s.relays[i].mode)) {
      error = String("Modo inválido en relé ") + (i + 1);
      return false;
    }
    if (!lightFromJson(r["light"], s.relays[i].light, error) || !eventsFromJson(r["events"], s.relays[i], error)) {
      error = String("Relé ") + (i + 1) + ": " + error;
      return false;
    }
  }

  JsonObjectConst c = obj["climate"];
  if (!c.isNull()) {
    if (!targetsFromJson(c["day"], s.climate.day, error) || !targetsFromJson(c["night"], s.climate.night, error))
      return false;
    if (c["tempHyst"].is<float>()) s.climate.tempHyst = constrain(c["tempHyst"].as<float>(), 0.1f, 5.0f);
    if (c["humHyst"].is<float>()) s.climate.humHyst = constrain(c["humHyst"].as<float>(), 0.5f, 20.0f);
    if (c["ventOn"].is<int>()) s.climate.ventOnMin = constrain(c["ventOn"].as<int>(), 0, 1440);
    if (c["ventEvery"].is<int>()) s.climate.ventEveryMin = constrain(c["ventEvery"].as<int>(), 0, 1440);
  }

  settings = s;
  return true;
}

void settingsBegin() {
  // La zona horaria hace falta para calcular los horarios por defecto.
  setenv("TZ", TZ_DEFAULT, 1);
  tzset();
  settingsDefaults();
  Preferences prefs;
  prefs.begin("cfg", true);
  size_t len = prefs.getBytesLength("json");
  String json;
  if (len) {
    std::unique_ptr<char[]> buf(new char[len + 1]);
    prefs.getBytes("json", buf.get(), len);
    buf[len] = 0;
    json = buf.get();
  }
  prefs.end();
  if (json.isEmpty()) return;

  JsonDocument doc;
  String error;
  if (deserializeJson(doc, json) || !settingsFromJson(doc.as<JsonObjectConst>(), error)) {
    Serial.println("[cfg] config guardada inválida, uso valores por defecto");
    settingsDefaults();
  }
}

void settingsSave() {
  JsonDocument doc;
  settingsToJson(doc.to<JsonObject>(), true);
  String json;
  serializeJson(doc, json);
  Preferences prefs;
  prefs.begin("cfg", false);
  // Como blob: NVS limita los strings a ~4 KB y la config completa puede superarlo.
  prefs.putBytes("json", json.c_str(), json.length());
  prefs.end();
}
