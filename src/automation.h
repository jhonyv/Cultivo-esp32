#pragma once
#include <Arduino.h>

// Decide qué quiere cada relé en modo auto: horario de luces, clima y riego.
void automationLoop();
bool automationIsDay(uint8_t tent);  // alguna luz de la carpa prendida
// Motivo por el que el relé está encendido/apagado en auto (para mostrar en la página).
const char* automationReason(uint8_t i);
// Segundos hasta el próximo cambio de una luz (0 si no aplica).
uint32_t automationNextLightChange(uint8_t i);
