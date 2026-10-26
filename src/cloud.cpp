#include "cloud.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "api.h"
#include "net.h"
#include "settings.h"

static const uint32_t RETRY_MS = 20000;

static WiFiClientSecure tls;
static PubSubClient mqtt(tls);
static String base;          // cultivo/<id>
static String activeConfig;  // host/puerto/usuario/clave con los que se conectó
static uint32_t lastAttempt = 0;

String cloudDeviceId() {
  String id = netApSsid();
  id.replace(AP_SSID_PREFIX, "");
  id.toLowerCase();
  return id;
}

bool cloudConnected() { return mqtt.connected(); }

// Pedido: {"id":"x","m":"GET|POST","p":"/api/status","q":"week","b":{...}}
// Respuesta en cultivo/<id>/res: {"id":"x","c":200,"d":{...}}
static void onMessage(char*, byte* payload, unsigned int len) {
  JsonDocument req;
  if (deserializeJson(req, (const char*)payload, len)) return;
  String body;
  if (!req["b"].isNull()) serializeJson(req["b"], body);

  JsonDocument out;
  int code = apiHandle(req["m"] | "GET", req["p"] | "", req["q"] | "", body, out, true);

  JsonDocument res;
  res["id"] = req["id"];
  res["c"] = code;
  res["d"] = out.as<JsonVariantConst>();
  String topic = base + "/res";
  // Publicación en streaming: las respuestas grandes (historial) no entran en el buffer de PubSubClient.
  mqtt.beginPublish(topic.c_str(), measureJson(res), false);
  serializeJson(res, mqtt);
  mqtt.endPublish();
}

void cloudBegin() {
  base = "cultivo/" + cloudDeviceId();
  tls.setInsecure();  // sin validar el certificado del broker, para no depender de un CA fijo
  mqtt.setBufferSize(4096);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(5);
  mqtt.setCallback(onMessage);
}

void cloudLoop() {
  String config = String(settings.mqttHost) + ":" + settings.mqttPort + "@" + settings.mqttUser + ":" + settings.mqttPass;
  if (config != activeConfig) {
    if (mqtt.connected()) mqtt.disconnect();
    activeConfig = config;
    lastAttempt = 0;
  }
  if (!settings.mqttHost[0] || !netConnected()) return;

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if (lastAttempt && millis() - lastAttempt < RETRY_MS) return;
  lastAttempt = millis();

  mqtt.setServer(settings.mqttHost, settings.mqttPort);
  String online = base + "/online";
  String clientId = "cultivo-" + cloudDeviceId();
  if (mqtt.connect(clientId.c_str(), settings.mqttUser, settings.mqttPass, online.c_str(), 1, true, "0")) {
    mqtt.publish(online.c_str(), "1", true);
    mqtt.subscribe((base + "/req").c_str());
    Serial.printf("[cloud] conectado a %s como %s\n", settings.mqttHost, base.c_str());
  } else {
    Serial.printf("[cloud] no se pudo conectar (estado %d)\n", mqtt.state());
  }
}
