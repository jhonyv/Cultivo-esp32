#pragma once
#include <Arduino.h>
#include "config.h"

// Clima de una carpa: promedio de sus sensores que estén respondiendo.
struct Climate {
  bool valid;
  float temperature;  // °C
  float humidity;     // %HR
  float vpd;          // kPa (de hoja, usando settings.leafOffset)
  float dewPoint;     // °C
  uint32_t updatedMs;
};

// Lectura de un sensor individual (para mostrar en la página).
struct SensorReading {
  const char* model;  // "SHT4x", "SHT3x" o "ninguno"
  uint8_t address;
  bool valid;
  float temperature;
  float humidity;
};

// Carpa 1 en el bus principal (Wire), carpa 2 en el segundo bus (Wire1), sensores en 0x44 y 0x45.
void sensorsBegin();  // llamar después de Wire.begin()
void sensorsLoop();
const Climate& climate(uint8_t tent);
SensorReading sensorReading(uint8_t tent, uint8_t index);

float calcVpd(float airTemp, float rh, float leafOffset);
