#include "settings.h"
#include <Preferences.h>
#include <memory>
#include "sensors.h"

Settings settings;

static const char* const TYPE_NAMES[] = {
    "generic", "light", "fan_in", "fan_out", "irrigation", "humidifier", "dehumidifier", "heater", "ac",
};
static const char* const MODE_NAMES[] = {"off", "on", "auto"};
static const char* const FAN_MODE_NAMES[] = {"climate", "schedule", "always"};

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
  r.tent = i / RELAYS_PER_TENT;
  r.type = type;
  r.mode = RelayMode::Off;
  r.light = {18 * 60, 6 * 60, anchorFromStart(6 * 60)};
  r.eventCount = 0;
  r.soil = {0, 0, 60, 3};
  r.plan = {0, 4, 11};
  // Lo común: extractor 24/7 y el intractor solo cuando hace falta (calor o humedad).
  r.fanMode = type == RelayType::FanOut ? FanMode::Always : FanMode::Climate;
  r.fanWindowCount = 0;
}

static void settingsDefaults() {
  // Cada carpa arranca con la misma distribución. Todo se puede cambiar después en Ajustes > Relés.
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    uint8_t b = t * RELAYS_PER_TENT;
#if RELAYS_PER_TENT >= 8
    // 2 luces, extractor, intractor, riego (una bomba) y 3 asignables.
    setRelay(b + 0, "Luz 1", RelayType::Light);
    setRelay(b + 1, "Luz 2", RelayType::Light);
    setRelay(b + 2, "Extractor", RelayType::FanOut);
    setRelay(b + 3, "Intractor", RelayType::FanIn);
    setRelay(b + 4, "Riego", RelayType::Irrigation);
    for (uint8_t k = 5; k < RELAYS_PER_TENT; k++) {
      char name[16];
      snprintf(name, sizeof(name), "Asignable %u", k - 4);
      setRelay(b + k, name, RelayType::Generic);
    }
#else
    // Luz, extractor, intractor y riego.
    setRelay(b + 0, "Luz", RelayType::Light);
    setRelay(b + 1, "Extractor", RelayType::FanOut);
    setRelay(b + 2, "Intractor", RelayType::FanIn);
    setRelay(b + 3, "Riego", RelayType::Irrigation);
#endif

    TentConfig& tc = settings.tents[t];
    snprintf(tc.name, sizeof(tc.name), "Carpa %u", t + 1);
    tc.enabled = t == 0;
    tc.climate.day = {22, 28, 0.8f, 1.2f};
    tc.climate.night = {18, 24, 0.6f, 1.0f};
    tc.climate.tempHyst = 0.5f;
    tc.climate.ventOnMin = 5;
    tc.climate.ventEveryMin = 30;
    tc.soil = {false, 2600, 1100, 20};
    tc.tank = {false, 60, 25, 50, 20, 5};  // valores típicos de un capacitivo v1.2 a 3,3 V
  }
  settings.relayActiveLow = true;
  settings.leafOffset = -2.0f;
  strlcpy(settings.apPassword, AP_PASSWORD_DEFAULT, sizeof(settings.apPassword));
  settings.apAlways = false;
  strlcpy(settings.tz, TZ_DEFAULT, sizeof(settings.tz));
  settings.webPassword[0] = 0;
  settings.telegramToken[0] = 0;
  settings.telegramChat[0] = 0;
  settings.telegramAlerts = true;
  settings.mqttHost[0] = 0;
  settings.mqttPort = 8883;
  settings.mqttUser[0] = 0;
  settings.mqttPass[0] = 0;
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
  o["vMin"] = t.vpdMin;
  o["vMax"] = t.vpdMax;
}

static bool targetsFromJson(JsonObjectConst o, ClimateTargets& t, String& error) {
  if (o.isNull()) return true;
  ClimateTargets n = t;
  if (o["tMin"].is<float>()) n.tempMin = o["tMin"];
  if (o["tMax"].is<float>()) n.tempMax = o["tMax"];
  if (o["vMin"].is<float>()) n.vpdMin = o["vMin"];
  if (o["vMax"].is<float>()) n.vpdMax = o["vMax"];
  if (!o["vMin"].is<float>() && o["hMin"].is<float>() && o["hMax"].is<float>()) {
    // Config de antes del control por VPD: se convierte el rango de humedad al VPD equivalente.
    n.vpdMin = calcVpd(n.tempMin, o["hMax"].as<float>(), settings.leafOffset);
    n.vpdMax = calcVpd(n.tempMax, o["hMin"].as<float>(), settings.leafOffset);
    n.vpdMin = roundf(constrain(n.vpdMin, 0.2f, 2.5f) * 20) / 20;
    n.vpdMax = roundf(constrain(n.vpdMax, n.vpdMin + 0.2f, 3.0f) * 20) / 20;
  }
  if (n.tempMin < 0 || n.tempMax > 45 || n.tempMin >= n.tempMax || n.vpdMin < 0.1f || n.vpdMax > 3 ||
      n.vpdMin >= n.vpdMax) {
    error = "Rangos de temperatura/VPD inválidos";
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

static void fanToJson(const RelayConfig& r, JsonObject o) {
  o["mode"] = FAN_MODE_NAMES[(uint8_t)r.fanMode];
  JsonArray arr = o["windows"].to<JsonArray>();
  for (uint8_t k = 0; k < r.fanWindowCount; k++) {
    char a[6], b[6];
    formatHHMM(a, sizeof(a), r.fanWindows[k].startMin);
    formatHHMM(b, sizeof(b), r.fanWindows[k].endMin);
    JsonObject w = arr.add<JsonObject>();
    w["start"] = a;
    w["end"] = b;
    w["en"] = r.fanWindows[k].enabled;
  }
}

static bool fanFromJson(JsonObjectConst o, RelayConfig& r, String& error) {
  if (o.isNull()) return true;
  if (o["mode"].is<const char*>()) {
    bool found = false;
    for (uint8_t i = 0; i < 3; i++) {
      if (strcmp(o["mode"].as<const char*>(), FAN_MODE_NAMES[i]) == 0) r.fanMode = (FanMode)i, found = true;
    }
    if (!found) {
      error = "Modo de ventilación inválido";
      return false;
    }
  }
  JsonArrayConst arr = o["windows"];
  if (arr.isNull()) return true;
  if (arr.size() > MAX_FAN_WINDOWS) {
    error = "Máximo " + String(MAX_FAN_WINDOWS) + " franjas por salida";
    return false;
  }
  uint8_t n = 0;
  for (JsonObjectConst w : arr) {
    int a = parseHHMM(w["start"]), b = parseHHMM(w["end"]);
    if (a < 0 || b < 0 || a == b) {
      error = "Franja inválida (desde y hasta HH:MM, distintas)";
      return false;
    }
    r.fanWindows[n++] = {(uint16_t)a, (uint16_t)b, w["en"] | true};
  }
  r.fanWindowCount = n;
  return true;
}

static void tankToJson(const TankConfig& c, JsonObject o) {
  o["enabled"] = c.enabled;
  o["emptyCm"] = c.emptyCm;
  o["fullCm"] = c.fullCm;
  o["liters"] = c.liters;
  o["alert"] = c.alertPct;
  o["cut"] = c.cutPct;
}

static bool tankFromJson(JsonObjectConst o, TankConfig& c, String& error) {
  if (o.isNull()) return true;
  if (o["enabled"].is<bool>()) c.enabled = o["enabled"];
  if (o["emptyCm"].is<int>()) c.emptyCm = constrain(o["emptyCm"].as<int>(), 1, 500);
  if (o["fullCm"].is<int>()) c.fullCm = constrain(o["fullCm"].as<int>(), 1, 500);
  if (o["liters"].is<int>()) c.liters = constrain(o["liters"].as<int>(), 1, 5000);
  if (o["alert"].is<int>()) c.alertPct = constrain(o["alert"].as<int>(), 0, 100);
  if (o["cut"].is<int>()) c.cutPct = constrain(o["cut"].as<int>(), 0, 100);
  if (c.emptyCm <= c.fullCm) {
    error = "Tanque: la distancia con el tanque vacío tiene que ser mayor que lleno";
    return false;
  }
  return true;
}

static void soilToJson(const SoilConfig& c, JsonObject o) {
  o["enabled"] = c.enabled;
  o["dry"] = c.dry;
  o["alert"] = c.alertBelow;
  o["wet"] = c.wet;
}

static bool soilFromJson(JsonObjectConst o, SoilConfig& c, String& error) {
  if (o.isNull()) return true;
  if (o["enabled"].is<bool>()) c.enabled = o["enabled"];
  if (o["alert"].is<int>()) c.alertBelow = constrain(o["alert"].as<int>(), 0, 90);
  if (o["dry"].is<int>()) c.dry = constrain(o["dry"].as<int>(), 0, 32767);
  if (o["wet"].is<int>()) c.wet = constrain(o["wet"].as<int>(), 0, 32767);
  // Calibración vieja del ADS1115 (otra escala): se vuelve a los valores típicos en mV.
  if (c.dry > 3300) c.dry = 2600, c.wet = 1100;
  if (c.dry <= c.wet) {
    error = "Suelo: la lectura en seco tiene que ser mayor que la mojada";
    return false;
  }
  return true;
}

static void irrigationSoilToJson(const IrrigationSoil& s, JsonObject o) {
  o["skip"] = s.skipAbove;
  o["below"] = s.waterBelow;
  o["dur"] = s.durSec;
  o["max"] = s.maxPerDay;
}

static void planToJson(const IrrigationPlan& p, JsonObject o) {
  o["flow"] = p.flowMlMin;
  o["pots"] = p.pots;
  o["potL"] = p.potLiters;
}

static void planFromJson(JsonObjectConst o, IrrigationPlan& p) {
  if (o.isNull()) return;
  if (o["flow"].is<int>()) p.flowMlMin = constrain(o["flow"].as<int>(), 0, 60000);
  if (o["pots"].is<int>()) p.pots = constrain(o["pots"].as<int>(), 1, 50);
  if (o["potL"].is<int>()) p.potLiters = constrain(o["potL"].as<int>(), 1, 200);
}

static void irrigationSoilFromJson(JsonObjectConst o, IrrigationSoil& s) {
  if (o.isNull()) return;
  if (o["skip"].is<int>()) s.skipAbove = constrain(o["skip"].as<int>(), 0, 100);
  if (o["below"].is<int>()) s.waterBelow = constrain(o["below"].as<int>(), 0, 100);
  if (o["dur"].is<int>()) s.durSec = constrain(o["dur"].as<int>(), 1, 3600);
  if (o["max"].is<int>()) s.maxPerDay = constrain(o["max"].as<int>(), 1, 12);
}

static void climateToJson(const ClimateConfig& cfg, JsonObject c) {
  targetsToJson(cfg.day, c["day"].to<JsonObject>());
  targetsToJson(cfg.night, c["night"].to<JsonObject>());
  c["tempHyst"] = cfg.tempHyst;
  c["ventOn"] = cfg.ventOnMin;
  c["ventEvery"] = cfg.ventEveryMin;
}

static bool climateFromJson(JsonObjectConst c, ClimateConfig& cfg, String& error) {
  if (c.isNull()) return true;
  if (!targetsFromJson(c["day"], cfg.day, error) || !targetsFromJson(c["night"], cfg.night, error)) return false;
  if (c["tempHyst"].is<float>()) cfg.tempHyst = constrain(c["tempHyst"].as<float>(), 0.1f, 5.0f);
  if (c["ventOn"].is<int>()) cfg.ventOnMin = constrain(c["ventOn"].as<int>(), 0, 1440);
  if (c["ventEvery"].is<int>()) cfg.ventEveryMin = constrain(c["ventEvery"].as<int>(), 0, 1440);
  return true;
}

void settingsToJson(JsonObject obj, bool secrets) {
  obj["relayActiveLow"] = settings.relayActiveLow;
  obj["leafOffset"] = settings.leafOffset;
  obj["apPassword"] = settings.apPassword;
  obj["apAlways"] = settings.apAlways;
  obj["tz"] = settings.tz;
  obj["telegramChat"] = settings.telegramChat;
  obj["telegramAlerts"] = settings.telegramAlerts;
  obj["mqttHost"] = settings.mqttHost;
  obj["mqttPort"] = settings.mqttPort;
  obj["mqttUser"] = settings.mqttUser;
  if (secrets) {
    obj["webPassword"] = settings.webPassword;
    obj["telegramToken"] = settings.telegramToken;
    obj["mqttPass"] = settings.mqttPass;
  } else {
    obj["webPasswordSet"] = settings.webPassword[0] != 0;
    obj["telegramTokenSet"] = settings.telegramToken[0] != 0;
    obj["mqttPassSet"] = settings.mqttPass[0] != 0;
  }
  JsonArray arr = obj["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    JsonObject r = arr.add<JsonObject>();
    r["name"] = settings.relays[i].name;
    r["tent"] = settings.relays[i].tent;
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
    irrigationSoilToJson(settings.relays[i].soil, r["soil"].to<JsonObject>());
    planToJson(settings.relays[i].plan, r["plan"].to<JsonObject>());
    fanToJson(settings.relays[i], r["fan"].to<JsonObject>());
  }
  JsonArray tents = obj["tents"].to<JsonArray>();
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    JsonObject o = tents.add<JsonObject>();
    o["name"] = settings.tents[t].name;
    o["enabled"] = settings.tents[t].enabled;
    climateToJson(settings.tents[t].climate, o["climate"].to<JsonObject>());
    soilToJson(settings.tents[t].soil, o["soil"].to<JsonObject>());
    tankToJson(settings.tents[t].tank, o["tank"].to<JsonObject>());
  }
}

bool settingsFromJson(JsonObjectConst obj, String& error) {
  // Se valida sobre una copia para no dejar la config a medio aplicar.
  static Settings s;  // estática: es grande para la pila
  s = settings;

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
  if (obj["apAlways"].is<bool>()) s.apAlways = obj["apAlways"];
  if (obj["tz"].is<const char*>()) strlcpy(s.tz, obj["tz"], sizeof(s.tz));
  if (obj["webPassword"].is<const char*>()) strlcpy(s.webPassword, obj["webPassword"], sizeof(s.webPassword));
  if (obj["telegramToken"].is<const char*>()) strlcpy(s.telegramToken, obj["telegramToken"], sizeof(s.telegramToken));
  if (obj["telegramChat"].is<const char*>()) strlcpy(s.telegramChat, obj["telegramChat"], sizeof(s.telegramChat));
  if (obj["telegramAlerts"].is<bool>()) s.telegramAlerts = obj["telegramAlerts"];
  if (obj["mqttHost"].is<const char*>()) strlcpy(s.mqttHost, obj["mqttHost"], sizeof(s.mqttHost));
  if (obj["mqttPort"].is<int>()) s.mqttPort = constrain(obj["mqttPort"].as<int>(), 1, 65535);
  if (obj["mqttUser"].is<const char*>()) strlcpy(s.mqttUser, obj["mqttUser"], sizeof(s.mqttUser));
  if (obj["mqttPass"].is<const char*>()) strlcpy(s.mqttPass, obj["mqttPass"], sizeof(s.mqttPass));

  JsonArrayConst arr = obj["relays"];
  for (uint8_t i = 0; i < arr.size() && i < NUM_RELAYS; i++) {
    JsonObjectConst r = arr[i];
    if (r["name"].is<const char*>()) strlcpy(s.relays[i].name, r["name"], sizeof(s.relays[i].name));
    if (r["tent"].is<int>()) {
      int t = r["tent"];
      if (t < 0 || t >= NUM_TENTS) {
        error = String("Carpa inválida en relé ") + (i + 1);
        return false;
      }
      s.relays[i].tent = t;
    }
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
    irrigationSoilFromJson(r["soil"], s.relays[i].soil);
    planFromJson(r["plan"], s.relays[i].plan);
    if (!fanFromJson(r["fan"], s.relays[i], error)) {
      error = String("Relé ") + (i + 1) + ": " + error;
      return false;
    }
  }

  // Config de antes de las 2 carpas: "climate" suelto es el de la carpa 1.
  if (!climateFromJson(obj["climate"], s.tents[0].climate, error)) return false;
  JsonArrayConst tents = obj["tents"];
  for (uint8_t t = 0; t < tents.size() && t < NUM_TENTS; t++) {
    JsonObjectConst o = tents[t];
    if (o["name"].is<const char*>()) strlcpy(s.tents[t].name, o["name"], sizeof(s.tents[t].name));
    if (o["enabled"].is<bool>()) s.tents[t].enabled = o["enabled"];
    if (!climateFromJson(o["climate"], s.tents[t].climate, error) || !soilFromJson(o["soil"], s.tents[t].soil, error) ||
        !tankFromJson(o["tank"], s.tents[t].tank, error)) {
      error = String(s.tents[t].name) + ": " + error;
      return false;
    }
  }
  s.tents[0].enabled = true;  // la carpa 1 siempre existe

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
  bool bad = deserializeJson(doc, json);
  // Config guardada por la otra versión (4 u 8 relés por carpa): se conserva todo menos los relés,
  // que arrancan con la distribución por defecto de esta versión.
  if (!bad && doc["relays"].is<JsonArray>() && doc["relays"].size() != NUM_RELAYS) {
    Serial.printf("[cfg] la config guardada tiene %u relés y esta versión %u: relés por defecto\n", (unsigned)doc["relays"].size(),
                  (unsigned)NUM_RELAYS);
    doc.remove("relays");
  }
  if (bad || !settingsFromJson(doc.as<JsonObjectConst>(), error)) {
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
