#pragma once
#include <ArduinoJson.h>
#include "config.h"

#define MAX_IRRIGATION_EVENTS 8

enum class RelayType : uint8_t {
  Generic,
  Light,
  FanIn,
  FanOut,
  Irrigation,
  Humidifier,
  Dehumidifier,
  Heater,
};

enum class RelayMode : uint8_t { Off, On, Auto };

// Ciclo de luz: encendida onMin minutos, apagada offMin, repitiendo desde anchor.
// Con on+off = 24 h es un horario diario común (18/6, 12/12...); si no, un ciclo libre (13/13).
struct LightSchedule {
  uint16_t onMin;
  uint16_t offMin;
  uint32_t anchor;  // epoch UTC de un encendido
};

struct IrrigationEvent {
  uint16_t startMin;  // minuto del día (hora local)
  uint16_t durSec;
  bool enabled;
};

struct RelayConfig {
  char name[24];
  uint8_t tent;  // carpa a la que pertenece (0 o 1)
  RelayType type;
  RelayMode mode;
  LightSchedule light;  // usado si type == Light
  uint8_t eventCount;   // usado si type == Irrigation
  IrrigationEvent events[MAX_IRRIGATION_EVENTS];
};

struct ClimateTargets {
  float tempMin, tempMax;  // °C
  float humMin, humMax;    // %HR
};

struct ClimateConfig {
  ClimateTargets day, night;  // día = alguna luz encendida
  float tempHyst, humHyst;
  uint16_t ventOnMin, ventEveryMin;  // renovación de aire: extractor X min cada Y min (0 = desactivado)
};

struct TentConfig {
  char name[24];
  bool enabled;
  ClimateConfig climate;
};

struct Settings {
  RelayConfig relays[NUM_RELAYS];
  TentConfig tents[NUM_TENTS];
  bool relayActiveLow;
  float leafOffset;  // °C, temperatura de hoja respecto del aire (para VPD)
  char apPassword[33];
  char tz[48];
  char webPassword[33];     // vacía = página sin contraseña (usuario "cultivo")
  char telegramToken[64];   // token del bot de @BotFather, vacío = bot apagado
  char telegramChat[24];    // chat autorizado; vacío = el bot solo responde su chat id
  bool telegramAlerts;
  char mqttHost[80];  // broker MQTT (ej. HiveMQ Cloud), vacío = nube apagada
  uint16_t mqttPort;  // TLS, normalmente 8883
  char mqttUser[48];
  char mqttPass[64];
};

extern Settings settings;

void settingsBegin();
void settingsSave();
// Con secrets=false (para la página) no incluye la contraseña web ni el token del bot.
void settingsToJson(JsonObject obj, bool secrets = false);
// Aplica solo los campos presentes en el JSON. Devuelve false si algún valor es inválido.
bool settingsFromJson(JsonObjectConst obj, String& error);

const char* relayTypeName(RelayType t);
const char* relayModeName(RelayMode m);
bool relayModeFromName(const char* s, RelayMode& out);
