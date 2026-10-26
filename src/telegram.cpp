#include "telegram.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "automation.h"
#include "clock.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"

struct Command {
  uint8_t relay;
  int8_t mode;     // RelayMode, -1 = no cambia
  uint16_t pulse;  // segundos, 0 = no
};

static QueueHandle_t queue;
static WiFiClientSecure client;
static int64_t lastUpdate = 0;

static const uint32_t ALERT_DELAY_MS = 10 * 60 * 1000;  // condición sostenida antes de avisar

struct Alert {
  const char* on;
  const char* off;
  uint32_t since;
  bool sent;
};

static const uint8_t NUM_ALERTS = 5;
static const Alert ALERT_TEXTS[NUM_ALERTS] = {
    {"🔥 Temperatura alta", "✅ Temperatura normal otra vez", 0, false},
    {"🥶 Temperatura baja", "✅ Temperatura normal otra vez", 0, false},
    {"💦 Humedad muy alta", "✅ Humedad normal otra vez", 0, false},
    {"🏜️ Humedad muy baja", "✅ Humedad normal otra vez", 0, false},
    {"⚠️ Los sensores de temperatura/humedad no responden", "✅ Los sensores volvieron a responder", 0, false},
};
static Alert alerts[NUM_TENTS][NUM_ALERTS];

static String apiUrl(const char* method) { return String("https://api.telegram.org/bot") + settings.telegramToken + "/" + method; }

static bool send(const String& chat, const String& text) {
  HTTPClient http;
  http.begin(client, apiUrl("sendMessage"));
  http.addHeader("Content-Type", "application/json");
  JsonDocument doc;
  doc["chat_id"] = chat;
  doc["text"] = text;
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  return code == 200;
}

static String statusText() {
  String s;
  if (clockValid()) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[8];
    strftime(buf, sizeof(buf), "%H:%M", &tm);
    s += String("🕒 ") + buf + "\n";
  }
  for (uint8_t t = 0; t < NUM_TENTS; t++) {
    if (!settings.tents[t].enabled) continue;
    const Climate& c = climate(t);
    s += String("\n🌱 ") + settings.tents[t].name + (automationIsDay(t) ? " · ☀️ Día\n" : " · 🌙 Noche\n");
    if (c.valid) {
      s += "🌡 " + String(c.temperature, 1) + " °C   💧 " + String(c.humidity, 0) + " %   VPD " + String(c.vpd, 2) + " kPa\n";
    } else {
      s += "⚠️ Sin lectura de los sensores\n";
    }
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
      const RelayConfig& r = settings.relays[i];
      if (r.tent != t) continue;
      s += String(i + 1) + ". " + (relayState(i) ? "🟢 " : "⚫ ") + r.name + " (" + relayModeName(r.mode) + ")\n";
    }
  }
  return s;
}

static const char* HELP =
    "Comandos:\n"
    "/estado – lecturas y relés\n"
    "/luces on|off|auto – todas las luces\n"
    "/rele N on|off|auto – un relé (1 a 16)\n"
    "/regar N SEG – prende el relé N esos segundos\n"
    "/ayuda – esta ayuda";

static int8_t parseMode(String m) {
  m.toLowerCase();
  if (m == "on" || m == "prender") return (int8_t)RelayMode::On;
  if (m == "off" || m == "apagar") return (int8_t)RelayMode::Off;
  if (m == "auto") return (int8_t)RelayMode::Auto;
  return -1;
}

static void handleCommand(const String& chat, String text) {
  text.trim();
  int at = text.indexOf('@');  // "/estado@MiBot" en grupos
  int sp = text.indexOf(' ');
  String cmd = text.substring(0, at > 0 && (sp < 0 || at < sp) ? at : (sp < 0 ? text.length() : sp));
  String args = sp < 0 ? "" : text.substring(sp + 1);
  args.trim();
  cmd.toLowerCase();

  if (cmd == "/estado") return (void)send(chat, statusText());

  if (cmd == "/luces") {
    int8_t mode = parseMode(args);
    if (mode < 0) return (void)send(chat, "Uso: /luces on|off|auto");
    uint8_t n = 0;
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
      if (settings.relays[i].type != RelayType::Light) continue;
      Command c = {i, mode, 0};
      xQueueSend(queue, &c, 0);
      n++;
    }
    return (void)send(chat, n ? String("Listo: luces en ") + args : String("No hay relés asignados a luz"));
  }

  if (cmd == "/rele" || cmd == "/regar") {
    int sp2 = args.indexOf(' ');
    int n = args.substring(0, sp2).toInt();
    String rest = sp2 < 0 ? "" : args.substring(sp2 + 1);
    rest.trim();
    if (n < 1 || n > NUM_RELAYS) return (void)send(chat, cmd == "/rele" ? "Uso: /rele N on|off|auto" : "Uso: /regar N SEG");
    Command c = {(uint8_t)(n - 1), -1, 0};
    if (cmd == "/rele") {
      c.mode = parseMode(rest);
      if (c.mode < 0) return (void)send(chat, "Uso: /rele N on|off|auto");
    } else {
      int sec = rest.toInt();
      if (sec < 1 || sec > 3600) return (void)send(chat, "Uso: /regar N SEG (1 a 3600)");
      c.pulse = sec;
    }
    xQueueSend(queue, &c, 0);
    return (void)send(chat, String("Listo: ") + settings.relays[n - 1].name + (c.pulse ? String(" prendido ") + rest + " s" : String(" en ") + rest));
  }

  send(chat, String("🌱 Cultivo\n\n") + HELP);
}

static void poll() {
  HTTPClient http;
  http.setTimeout(30000);
  String url = apiUrl("getUpdates") + "?timeout=20&limit=5&offset=" + String((long long)(lastUpdate + 1));
  http.begin(client, url);
  int code = http.GET();
  if (code != 200) {
    http.end();
    if (code == 401 || code == 404) Serial.println("[telegram] token inválido");
    vTaskDelay(pdMS_TO_TICKS(code == 401 || code == 404 ? 60000 : 5000));
    return;
  }
  String body = http.getString();
  http.end();

  JsonDocument filter;
  filter["result"][0]["update_id"] = true;
  filter["result"][0]["message"]["chat"]["id"] = true;
  filter["result"][0]["message"]["text"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  for (JsonObject u : doc["result"].as<JsonArray>()) {
    lastUpdate = u["update_id"].as<int64_t>();
    const char* text = u["message"]["text"];
    if (!text) continue;
    String chat = String((long long)u["message"]["chat"]["id"].as<int64_t>());
    if (chat != settings.telegramChat) {
      send(chat, "Este equipo todavía no te reconoce. Tu chat id es " + chat +
                     ": cargalo en Ajustes > Telegram de la página del equipo.");
      continue;
    }
    handleCommand(chat, text);
  }
}

static void checkAlerts() {
  if (!settings.telegramAlerts || !settings.telegramChat[0]) return;
  for (uint8_t tent = 0; tent < NUM_TENTS; tent++) {
    if (!settings.tents[tent].enabled) continue;
    const Climate& c = climate(tent);
    const ClimateConfig& cfg = settings.tents[tent].climate;
    const ClimateTargets& t = automationIsDay(tent) ? cfg.day : cfg.night;
    bool active[NUM_ALERTS] = {
        c.valid && c.temperature > t.tempMax + 2,
        c.valid && c.temperature < t.tempMin - 2,
        c.valid && c.humidity > t.humMax + 10,
        c.valid && c.humidity < t.humMin - 10,
        !c.valid,
    };
    String prefix = String(settings.tents[tent].name) + ": ";
    for (uint8_t i = 0; i < NUM_ALERTS; i++) {
      Alert& a = alerts[tent][i];
      if (!active[i]) {
        if (a.sent) send(settings.telegramChat, prefix + ALERT_TEXTS[i].off + "\n" + statusText());
        a.since = 0;
        a.sent = false;
        continue;
      }
      if (!a.since) a.since = millis();
      if (!a.sent && millis() - a.since > ALERT_DELAY_MS) {
        a.sent = send(settings.telegramChat, prefix + ALERT_TEXTS[i].on + "\n" + statusText());
      }
    }
  }
}

static void task(void*) {
  client.setInsecure();  // sin validar el certificado de Telegram, para no depender de un CA que puede cambiar
  bool bootSent = false;
  for (;;) {
    if (!settings.telegramToken[0] || WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    if (!bootSent && settings.telegramChat[0]) {
      bootSent = send(settings.telegramChat, "🌱 El equipo se encendió (¿hubo un corte de luz?)\n\n" + statusText());
    }
    checkAlerts();
    poll();
  }
}

void telegramBegin() {
  queue = xQueueCreate(16, sizeof(Command));
  xTaskCreatePinnedToCore(task, "telegram", 12288, nullptr, 1, nullptr, 0);
}

void telegramLoop() {
  Command c;
  bool save = false;
  while (xQueueReceive(queue, &c, 0) == pdTRUE) {
    if (c.mode >= 0) {
      settings.relays[c.relay].mode = (RelayMode)c.mode;
      save = true;
    }
    if (c.pulse) relayPulse(c.relay, c.pulse);
  }
  if (save) settingsSave();
}
