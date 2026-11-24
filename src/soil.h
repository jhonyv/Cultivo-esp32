#pragma once
#include <Arduino.h>
#include "config.h"

// Humedad de suelo con sensores capacitivos (v1.2/v2.0) directo a pines analógicos del ESP32,
// 2 por carpa (SOIL_PINS). Se detectan solos: un pin sin sensor queda en 0 por la resistencia de
// 100k a GND (o al máximo si flota) y no se muestra. El control del riego se activa en
// settings.tents[t].soil.enabled; la lectura aparece siempre que haya un sensor enchufado.
struct SoilReading {
  bool valid;                       // al menos un sensor conectado
  float percent;                    // promedio de los sensores conectados, 0 = seco, 100 = en agua
  uint16_t raw;                     // promedio en mV (para calibrar)
  bool chValid[SOIL_SENSORS_MAX];
  uint16_t chRaw[SOIL_SENSORS_MAX];
  float chPercent[SOIL_SENSORS_MAX];
};

void soilBegin();
void soilLoop();
const SoilReading& soil(uint8_t tent);
bool soilPresent(uint8_t tent);  // hay al menos un sensor enchufado
