#include "net.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include "config.h"
#include "settings.h"

static DNSServer dns;
static String ssid, pass, apSsid;
static bool apActive = false;
static bool connected = false;
static uint32_t connectStart = 0;  // 0 = no hay intento en curso
static uint32_t lastAttempt = 0;
static uint32_t connectedSince = 0;
static uint32_t buttonDown = 0;

static void loadCredentials() {
  Preferences prefs;
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
}

static void startAp() {
  if (apActive) return;
  WiFi.mode(WIFI_AP_STA);
  const char* pw = strlen(settings.apPassword) >= 8 ? settings.apPassword : nullptr;
  WiFi.softAP(apSsid.c_str(), pw);
  dns.start(53, "*", WiFi.softAPIP());
  apActive = true;
  Serial.printf("[wifi] AP \"%s\" activo en %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

static void stopAp() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  Serial.println("[wifi] AP apagado");
}

static void startConnect() {
  if (ssid.isEmpty()) return;
  Serial.printf("[wifi] conectando a \"%s\"...\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  connectStart = millis();
  lastAttempt = millis();
}

void netBegin() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", (uint16_t)(mac >> 32));
  apSsid = String(AP_SSID_PREFIX) + suffix;

  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  loadCredentials();

  if (ssid.isEmpty()) {
    startAp();
  } else {
    WiFi.mode(WIFI_STA);
    startConnect();
  }
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

static void handleButton() {
  if (digitalRead(PIN_BUTTON) == LOW) {
    if (!buttonDown) buttonDown = millis();
    if (millis() - buttonDown > 5000) {
      Serial.println("[wifi] botón mantenido: borrando red guardada");
      netForget();
    }
  } else {
    buttonDown = 0;
  }
}

static void updateLed() {
  if (connected) digitalWrite(PIN_LED, HIGH);
  else if (apActive) digitalWrite(PIN_LED, (millis() / 500) % 2);
  else digitalWrite(PIN_LED, LOW);
}

void netLoop() {
  if (apActive) dns.processNextRequest();
  handleButton();
  updateLed();

  bool nowConnected = WiFi.status() == WL_CONNECTED;
  if (nowConnected && !connected) {
    connectStart = 0;
    connectedSince = millis();
    Serial.printf("[wifi] conectado, IP %s (http://%s.local)\n", WiFi.localIP().toString().c_str(), HOSTNAME);
  } else if (!nowConnected && connected) {
    Serial.println("[wifi] conexión perdida");
    lastAttempt = millis();
  }
  connected = nowConnected;

  if (connected) {
    // Dejar el AP un rato para que el portal muestre la IP nueva y después apagarlo.
    if (apActive && millis() - connectedSince > AP_OFF_DELAY_MS) stopAp();
    return;
  }

  if (connectStart && millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("[wifi] no se pudo conectar");
    connectStart = 0;
    startAp();
  }
  // Si estamos sin red (y no hay intento en curso), reintentar periódicamente.
  if (!connectStart && !ssid.isEmpty() && millis() - lastAttempt > WIFI_RETRY_MS) {
    if (!apActive) startAp();
    startConnect();
  }
}

bool netConnected() { return connected; }
bool netApActive() { return apActive; }
String netApSsid() { return apSsid; }
String netSavedSsid() { return ssid; }

void netSaveCredentials(const String& newSsid, const String& newPass) {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", newSsid);
  prefs.putString("pass", newPass);
  prefs.end();
  ssid = newSsid;
  pass = newPass;
  WiFi.disconnect();
  startConnect();
}

void netForget() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  WiFi.disconnect(true, true);
  delay(200);
  ESP.restart();
}
