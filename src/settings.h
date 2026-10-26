#pragma once
#include <ArduinoJson.h>
#include "config.h"

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

struct RelayConfig {
  char name[24];
  RelayType type;
  RelayMode mode;
};

struct Settings {
  RelayConfig relays[NUM_RELAYS];
  bool relayActiveLow;
  float leafOffset;  // °C, temperatura de hoja respecto del aire (para VPD)
  char apPassword[33];
  char tz[48];
};

extern Settings settings;

void settingsBegin();
void settingsSave();
void settingsToJson(JsonObject obj);
// Aplica solo los campos presentes en el JSON. Devuelve false si algún valor es inválido.
bool settingsFromJson(JsonObjectConst obj, String& error);

const char* relayTypeName(RelayType t);
const char* relayModeName(RelayMode m);
bool relayModeFromName(const char* s, RelayMode& out);
