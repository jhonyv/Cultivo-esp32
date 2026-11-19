#pragma once
#include <ArduinoJson.h>
#include "config.h"

#define MAX_IRRIGATION_EVENTS 8
#define MAX_FAN_WINDOWS 4

enum class RelayType : uint8_t {
  Generic,
  Light,
  FanIn,
  FanOut,
  Irrigation,
  Humidifier,
  Dehumidifier,
  Heater,
  AirConditioner,  // se prende con temperatura alta
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

// Cómo maneja Auto a un extractor/intractor.
enum class FanMode : uint8_t {
  Climate,   // por temperatura, humedad y renovación
  Schedule,  // prendido en las franjas horarias
  Always,    // permanente (24/7)
};

struct FanWindow {
  uint16_t startMin, endMin;  // minuto del día (hora local); si end < start cruza la medianoche
  bool enabled;
};

// Riego según humedad de suelo (0 = opción apagada). Solo actúa si la carpa tiene lectura de suelo.
struct IrrigationSoil {
  uint8_t skipAbove;  // % : saltear un riego programado si la tierra está por encima
  uint8_t waterBelow; // % : regar fuera de horario si baja de esto (solo con luces prendidas)
  uint16_t durSec;    // duración de ese riego extra
  uint8_t maxPerDay;  // tope de riegos extra por día
};

struct RelayConfig {
  char name[24];
  uint8_t tent;  // carpa a la que pertenece (0 o 1)
  RelayType type;
  RelayMode mode;
  LightSchedule light;  // usado si type == Light
  uint8_t eventCount;   // usado si type == Irrigation
  IrrigationEvent events[MAX_IRRIGATION_EVENTS];
  IrrigationSoil soil;  // usado si type == Irrigation y la carpa tiene sensores de suelo
  FanMode fanMode;      // usados si type == FanIn/FanOut
  uint8_t fanWindowCount;
  FanWindow fanWindows[MAX_FAN_WINDOWS];
};

struct ClimateTargets {
  float tempMin, tempMax;  // °C
  float vpdMin, vpdMax;    // kPa (de hoja): la humedad se maneja por VPD
};

struct ClimateConfig {
  ClimateTargets day, night;  // día = alguna luz encendida
  float tempHyst;
  uint16_t ventOnMin, ventEveryMin;  // renovación de aire: extractor X min cada Y min (0 = desactivado)
};

// Sensores capacitivos de suelo (opcional). Calibración en cuentas del ADS1115.
struct SoilConfig {
  bool enabled;
  uint8_t count;  // sensores conectados en A0..A(count-1)
  uint16_t dry;   // lectura al aire (0 %)
  uint16_t wet;   // lectura en un vaso de agua (100 %)
};

// Tanque de riego con sensor ultrasónico en la tapa (opcional).
struct TankConfig {
  bool enabled;
  uint16_t emptyCm;  // distancia del sensor al agua con el tanque vacío (al fondo)
  uint16_t fullCm;   // distancia del sensor al agua con el tanque lleno
  uint16_t liters;   // capacidad, para mostrar litros
  uint8_t alertPct;  // avisar por Telegram debajo de esto (0 = no)
  uint8_t cutPct;    // no prender la bomba debajo de esto (0 = no)
};

struct TentConfig {
  char name[24];
  bool enabled;
  ClimateConfig climate;
  SoilConfig soil;
  TankConfig tank;
};

struct Settings {
  RelayConfig relays[NUM_RELAYS];
  TentConfig tents[NUM_TENTS];
  bool relayActiveLow;
  float leafOffset;  // °C, temperatura de hoja respecto del aire (para VPD)
  char apPassword[33];
  bool apAlways;  // red propia "Cultivo-XXXX" siempre prendida, no solo cuando falla el WiFi
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
