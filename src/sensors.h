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
  bool hasCo2;        // hay un SCD40/41 leyendo
  float co2;          // ppm
  bool hasLux;        // hay un BH1750 leyendo
  float lux;          // lx
};

enum class SensorKind : uint8_t { TempHum, Co2, Light };

// Lectura de un sensor individual (para mostrar en la página).
struct SensorReading {
  bool present;       // detectado en el bus (si no, no se muestra)
  SensorKind kind;
  const char* model;  // "SHT4x", "SHT3x", "SCD4x", "BH1750"
  uint8_t address;
  bool valid;         // tiene una lectura reciente
  float temperature;
  float humidity;
  float co2;
  float lux;
};

// Detección automática: en el bus de cada carpa (carpa 1 en Wire, carpa 2 en Wire1) se buscan
// SHT3x/SHT4x en 0x44 y 0x45, SCD40/41 (CO2) en 0x62 y BH1750 (luz) en 0x23 y 0x5C.
// Los que no están se vuelven a buscar cada 30 s, así se pueden enchufar en caliente.
#define SENSOR_SLOTS 5
void sensorsBegin();  // llamar después de Wire.begin()
void sensorsLoop();
const Climate& climate(uint8_t tent);
SensorReading sensorReading(uint8_t tent, uint8_t slot);

float calcVpd(float airTemp, float rh, float leafOffset);
