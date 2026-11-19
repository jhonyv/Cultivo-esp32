#pragma once
#include <Arduino.h>
#include "config.h"

// Nivel del tanque de riego con un ultrasónico JSN-SR04T por carpa (opcional, settings.tents[t].tank).
struct TankReading {
  bool valid;
  float cm;       // distancia del sensor al agua
  float percent;  // 0 = vacío, 100 = lleno
  float liters;
};

void tankBegin();
void tankLoop();
const TankReading& tank(uint8_t tent);
// La bomba de esa carpa no tiene que prender (nivel por debajo del corte).
bool tankLow(uint8_t tent);
