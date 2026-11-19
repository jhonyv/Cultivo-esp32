#pragma once
#include <Arduino.h>
#include "config.h"

// Humedad de suelo con sensores capacitivos (v1.2/v2.0) leídos por un ADS1115 por carpa:
// carpa 1 en el bus principal, carpa 2 en el segundo bus, siempre en 0x48, canales A0..A3.
// Es opcional: se activa por carpa en Ajustes (settings.tents[t].soil).
struct SoilReading {
  bool valid;                       // al menos un sensor responde
  float percent;                    // promedio de los sensores válidos, 0 = seco, 100 = en agua
  uint16_t raw;                     // promedio crudo (para calibrar)
  bool chValid[SOIL_SENSORS_MAX];
  uint16_t chRaw[SOIL_SENSORS_MAX];
  float chPercent[SOIL_SENSORS_MAX];
};

void soilBegin();  // llamar después de sensorsBegin() (que arranca el segundo bus)
void soilLoop();
const SoilReading& soil(uint8_t tent);
bool soilPresent(uint8_t tent);  // el ADS1115 responde
