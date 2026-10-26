#include "web.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include "automation.h"
#include "clock.h"
#include "history.h"
#include "net.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"
#include "web_page.h"

static WebServer server(80);

static void sendJson(JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void sendError(int code, const String& msg) {
  JsonDocument doc;
  doc["error"] = msg;
  sendJson(doc, code);
}

static bool parseBody(JsonDocument& doc) {
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    sendError(400, "JSON inválido");
    return false;
  }
  return true;
}

static void handleStatus() {
  JsonDocument doc;
  const Climate& c = climate();
  JsonObject cl = doc["climate"].to<JsonObject>();
  cl["valid"] = c.valid;
  cl["sensor"] = sensorModel();
  if (c.valid) {
    cl["temperature"] = serialized(String(c.temperature, 1));
    cl["humidity"] = serialized(String(c.humidity, 1));
    cl["vpd"] = serialized(String(c.vpd, 2));
    cl["dewPoint"] = serialized(String(c.dewPoint, 1));
    cl["age"] = (millis() - c.updatedMs) / 1000;
  }

  JsonObject t = doc["time"].to<JsonObject>();
  t["valid"] = clockValid();
  t["epoch"] = (uint32_t)time(nullptr);
  t["rtc"] = clockHasRtc();
  t["ntp"] = clockNtpSynced();
  t["day"] = automationIsDay();
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

  JsonArray rel = doc["relays"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    JsonObject r = rel.add<JsonObject>();
    r["name"] = settings.relays[i].name;
    r["type"] = relayTypeName(settings.relays[i].type);
    r["mode"] = relayModeName(settings.relays[i].mode);
    r["on"] = relayState(i);
    r["reason"] = automationReason(i);
    if (uint32_t p = relayPulseRemaining(i)) r["pulse"] = p;
    if (uint32_t n = automationNextLightChange(i)) r["next"] = n;
  }
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  sendJson(doc);
}

// POST /api/relay {"id":0..7,"mode":"on"|"off"|"auto"} o {"id":0..7,"pulse":segundos}
static void handleRelay() {
  JsonDocument doc;
  if (!parseBody(doc)) return;
  int id = doc["id"] | -1;
  if (id < 0 || id >= NUM_RELAYS) return sendError(400, "id inválido");
  if (doc["pulse"].is<int>()) {
    int sec = doc["pulse"];
    if (sec < 0 || sec > 3600) return sendError(400, "pulso inválido (0-3600 s)");
    relayPulse(id, sec);
  } else {
    RelayMode mode;
    if (!doc["mode"].is<const char*>() || !relayModeFromName(doc["mode"], mode)) return sendError(400, "modo inválido");
    settings.relays[id].mode = mode;
    settingsSave();
  }
  relaysUpdate();
  handleStatus();
}

static void handleHistory() {
  JsonDocument doc;
  historyToJson(doc.to<JsonObject>());
  sendJson(doc);
}

static void handleGetConfig() {
  JsonDocument doc;
  settingsToJson(doc.to<JsonObject>());
  sendJson(doc);
}

static void handlePostConfig() {
  JsonDocument doc;
  if (!parseBody(doc)) return;
  String error;
  if (!settingsFromJson(doc.as<JsonObjectConst>(), error)) return sendError(400, error);
  settingsSave();
  clockSetTimezone(settings.tz);
  relaysUpdate();
  handleGetConfig();
}

// GET /api/wifi/scan: la primera llamada arranca el escaneo, las siguientes devuelven el resultado.
static void handleScan() {
  JsonDocument doc;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    doc["scanning"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    doc["scanning"] = true;
  } else {
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) continue;
      bool dup = false;
      for (JsonObject o : arr) {
        if (s == o["ssid"].as<const char*>()) {
          dup = true;
          break;
        }
      }
      if (dup) continue;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = s;
      o["rssi"] = WiFi.RSSI(i);
      o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  }
  sendJson(doc);
}

// POST /api/wifi {"ssid":"...","pass":"..."}
static void handleWifi() {
  JsonDocument doc;
  if (!parseBody(doc)) return;
  String ssid = doc["ssid"] | "";
  String pass = doc["pass"] | "";
  if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63) return sendError(400, "SSID o clave inválidos");
  server.send(200, "application/json", "{\"ok\":true}");
  delay(100);
  netSaveCredentials(ssid, pass);
}

static void handleForget() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  netForget();
}

// POST /api/time {"epoch":<UTC>} para fijar la hora sin internet (la manda el navegador).
static void handleTime() {
  JsonDocument doc;
  if (!parseBody(doc)) return;
  uint32_t epoch = doc["epoch"] | 0;
  if (epoch < 1704067200) return sendError(400, "epoch inválido");
  clockSet(epoch);
  handleStatus();
}

static void handleRestart() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

static void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char*)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}

static void handleNotFound() {
  // Portal cautivo: cualquier URL desconocida (las que usan Android/iOS para detectar
  // portales) redirige a la página principal mientras el AP está activo.
  if (netApActive() && !server.uri().startsWith("/api/")) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  sendError(404, "no encontrado");
}

void webBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_POST, handleRelay);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/wifi/forget", HTTP_POST, handleForget);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() { server.handleClient(); }
