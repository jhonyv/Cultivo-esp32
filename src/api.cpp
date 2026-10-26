#include "api.h"
#include <WiFi.h>
#include "automation.h"
#include "clock.h"
#include "cloud.h"
#include "history.h"
#include "net.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"

static uint32_t restartAt = 0;

static int error(JsonDocument& out, int code, const String& msg) {
  out.clear();
  out["error"] = msg;
  return code;
}

// Valor de un parámetro en "a=1&b=2".
static String param(const String& query, const char* name) {
  String key = String(name) + "=";
  int start = 0;
  while (start < (int)query.length()) {
    int end = query.indexOf('&', start);
    if (end < 0) end = query.length();
    if (query.substring(start, start + key.length()) == key) return query.substring(start + key.length(), end);
    start = end + 1;
  }
  return "";
}

static void statusJson(JsonDocument& doc) {
  JsonArray tents = doc["tents"].to<JsonArray>();
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    JsonObject o = tents.add<JsonObject>();
    o["name"] = settings.tents[t].name;
    o["enabled"] = settings.tents[t].enabled;
    o["day"] = automationIsDay(t);
    const Climate& c = climate(t);
    JsonObject cl = o["climate"].to<JsonObject>();
    cl["valid"] = c.valid;
    if (c.valid) {
      cl["temperature"] = serialized(String(c.temperature, 1));
      cl["humidity"] = serialized(String(c.humidity, 1));
      cl["vpd"] = serialized(String(c.vpd, 2));
      cl["dewPoint"] = serialized(String(c.dewPoint, 1));
      cl["age"] = (millis() - c.updatedMs) / 1000;
    }
    JsonArray sensors = o["sensors"].to<JsonArray>();
    for (uint8_t k = 0; k < SENSORS_PER_TENT; k++) {
      SensorReading r = sensorReading(t, k);
      JsonObject so = sensors.add<JsonObject>();
      so["model"] = r.model;
      so["addr"] = r.address;
      so["valid"] = r.valid;
      if (r.valid) {
        so["temperature"] = serialized(String(r.temperature, 1));
        so["humidity"] = serialized(String(r.humidity, 1));
      }
    }
  }

  JsonObject t = doc["time"].to<JsonObject>();
  t["valid"] = clockValid();
  t["epoch"] = (uint32_t)time(nullptr);
  t["rtc"] = clockHasRtc();
  t["ntp"] = clockNtpSynced();
  if (clockValid()) {
    char buf[20];
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    t["local"] = buf;
  }

  JsonObject w = doc["wifi"].to<JsonObject>();
  w["connected"] = netConnected();
  w["ssid"] = netSavedSsid();
  if (netConnected()) {
    w["ip"] = WiFi.localIP().toString();
    w["rssi"] = WiFi.RSSI();
  }
  w["ap"] = netApActive();
  w["apSsid"] = netApSsid();
  w["cloud"] = cloudConnected();
  doc["device"] = cloudDeviceId();

  JsonArray rel = doc["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    JsonObject r = rel.add<JsonObject>();
    r["name"] = settings.relays[i].name;
    r["tent"] = settings.relays[i].tent;
    r["present"] = relayPresent(i);
    r["type"] = relayTypeName(settings.relays[i].type);
    r["mode"] = relayModeName(settings.relays[i].mode);
    r["on"] = relayState(i);
    r["reason"] = automationReason(i);
    if (uint32_t p = relayPulseRemaining(i)) r["pulse"] = p;
    if (uint32_t n = automationNextLightChange(i)) r["next"] = n;
  }
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
}

// POST /api/relay {"id":0..15,"mode":"on"|"off"|"auto"} o {"id":0..7,"pulse":segundos}
static int relayPost(JsonDocument& in, JsonDocument& out) {
  int id = in["id"] | -1;
  if (id < 0 || id >= NUM_RELAYS) return error(out, 400, "id inválido");
  if (in["pulse"].is<int>()) {
    int sec = in["pulse"];
    if (sec < 0 || sec > 3600) return error(out, 400, "pulso inválido (0-3600 s)");
    relayPulse(id, sec);
  } else {
    RelayMode mode;
    if (!in["mode"].is<const char*>() || !relayModeFromName(in["mode"], mode)) return error(out, 400, "modo inválido");
    settings.relays[id].mode = mode;
    settingsSave();
  }
  relaysUpdate();
  statusJson(out);
  return 200;
}

// GET /api/history?range=day|week|month&tent=0|1
static int historyGet(const String& query, JsonDocument& out) {
  String range = param(query, "range");
  uint8_t tent = constrain(param(query, "tent").toInt(), 0, NUM_TENTS - 1);
  if (range == "week") historyHourlyToJson(out.to<JsonObject>(), tent, 7 * 24);
  else if (range == "month") historyDailyToJson(out.to<JsonObject>(), tent, 31);
  else {
    historyToJson(out.to<JsonObject>(), tent);
    historyDailyToJson(out["today"].to<JsonObject>(), tent, 1);
  }
  return 200;
}

static int configPost(JsonDocument& in, JsonDocument& out, bool remote) {
  // Desde la nube no se cambia la conexión a la nube: un error dejaría el equipo inaccesible.
  if (remote && (in["mqttHost"].is<const char*>() || in["mqttUser"].is<const char*>() || in["mqttPass"].is<const char*>()))
    return error(out, 403, "La conexión a la nube solo se cambia desde la red local");
  String err;
  if (!settingsFromJson(in.as<JsonObjectConst>(), err)) return error(out, 400, err);
  settingsSave();
  clockSetTimezone(settings.tz);
  relaysUpdate();
  settingsToJson(out.to<JsonObject>());
  return 200;
}

// POST /api/time {"epoch":<UTC>} para fijar la hora sin internet (la manda el navegador).
static int timePost(JsonDocument& in, JsonDocument& out) {
  uint32_t epoch = in["epoch"] | 0;
  if (epoch < 1704067200) return error(out, 400, "epoch inválido");
  clockSet(epoch);
  statusJson(out);
  return 200;
}

int apiHandle(const String& method, const String& path, const String& query, const String& body, JsonDocument& out,
              bool remote) {
  bool post = method == "POST";
  JsonDocument in;
  if (post && (deserializeJson(in, body) || !in.is<JsonObject>())) return error(out, 400, "JSON inválido");

  if (!post && path == "/api/status") return statusJson(out), 200;
  if (!post && path == "/api/history") return historyGet(query, out);
  if (!post && path == "/api/config") return settingsToJson(out.to<JsonObject>()), 200;
  if (post && path == "/api/relay") return relayPost(in, out);
  if (post && path == "/api/config") return configPost(in, out, remote);
  if (post && path == "/api/time") return timePost(in, out);
  if (post && path == "/api/restart") {
    restartAt = millis() + 1000;
    out["ok"] = true;
    return 200;
  }
  if (remote && path.startsWith("/api/wifi")) return error(out, 403, "El WiFi solo se cambia desde la red local");
  return error(out, 404, "no encontrado");
}

void apiLoop() {
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
}
