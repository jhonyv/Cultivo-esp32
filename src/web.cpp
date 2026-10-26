#include "web.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include "api.h"
#include "net.h"
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

static void handleApi() {
  JsonDocument out;
  int code = apiHandle(server.method() == HTTP_POST ? "POST" : "GET", server.uri(), server.arg("range"), server.arg("plain"),
                       out, false);
  sendJson(out, code);
}

// Si hay contraseña configurada, toda la página la pide (usuario "cultivo").
static void route(const char* uri, HTTPMethod method, void (*fn)()) {
  server.on(uri, method, [fn]() {
    if (settings.webPassword[0] && !server.authenticate("cultivo", settings.webPassword)) {
      return server.requestAuthentication(BASIC_AUTH, "Cultivo");
    }
    fn();
  });
}

void webBegin() {
  route("/", HTTP_GET, handleRoot);
  route("/api/status", HTTP_GET, handleApi);
  route("/api/relay", HTTP_POST, handleApi);
  route("/api/history", HTTP_GET, handleApi);
  route("/api/config", HTTP_GET, handleApi);
  route("/api/config", HTTP_POST, handleApi);
  route("/api/time", HTTP_POST, handleApi);
  route("/api/restart", HTTP_POST, handleApi);
  route("/api/wifi/scan", HTTP_GET, handleScan);
  route("/api/wifi", HTTP_POST, handleWifi);
  route("/api/wifi/forget", HTTP_POST, handleForget);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() { server.handleClient(); }
