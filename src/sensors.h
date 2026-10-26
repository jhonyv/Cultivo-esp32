#pragma once
#include <Arduino.h>

struct Climate {
  bool valid;
  float temperature;  // °C
  float humidity;     // %HR
  float vpd;          // kPa (de hoja, usando settings.leafOffset)
  float dewPoint;     // °C
  uint32_t updatedMs;
};

void sensorsBegin();
void sensorsLoop();
const Climate& climate();
const char* sensorModel();  // "SHT4x", "SHT3x" o "ninguno"

float calcVpd(float airTemp, float rh, float leafOffset);
