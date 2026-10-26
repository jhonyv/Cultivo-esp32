#include "settings.h"
#include <Preferences.h>

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

static void setRelay(uint8_t i, const char* name, RelayType type) {
  strlcpy(settings.relays[i].name, name, sizeof(settings.relays[i].name));
  settings.relays[i].type = type;
  settings.relays[i].mode = RelayMode::Off;
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
  settings.relayActiveLow = true;
  settings.leafOffset = -2.0f;
  strlcpy(settings.apPassword, AP_PASSWORD_DEFAULT, sizeof(settings.apPassword));
  strlcpy(settings.tz, TZ_DEFAULT, sizeof(settings.tz));
}

void settingsToJson(JsonObject obj) {
  obj["relayActiveLow"] = settings.relayActiveLow;
  obj["leafOffset"] = settings.leafOffset;
  obj["apPassword"] = settings.apPassword;
  obj["tz"] = settings.tz;
  JsonArray arr = obj["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    JsonObject r = arr.add<JsonObject>();
    r["name"] = settings.relays[i].name;
    r["type"] = relayTypeName(settings.relays[i].type);
    r["mode"] = relayModeName(settings.relays[i].mode);
  }
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
  }

  settings = s;
  return true;
}

void settingsBegin() {
  settingsDefaults();
  Preferences prefs;
  prefs.begin("cfg", true);
  String json = prefs.getString("json", "");
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
  settingsToJson(doc.to<JsonObject>());
  String json;
  serializeJson(doc, json);
  Preferences prefs;
  prefs.begin("cfg", false);
  prefs.putString("json", json);
  prefs.end();
}
