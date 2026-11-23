#pragma once
#include "config.h"

void relaysBegin();
// Llamar después de Wire.begin(): habilita los relés 9-16 del PCF8574.
void relaysBeginExpander();
// Aplica el modo de cada relé: on/off manual, o en auto lo que pidan las automatizaciones.
void relaysUpdate();
bool relayState(uint8_t i);
// false para los relés 9-16 si el PCF8574 no está conectado.
bool relayPresent(uint8_t i);
// Lo que las automatizaciones quieren para un relé en modo auto.
void relaySetAuto(uint8_t i, bool on);
// Enciende el relé un rato sin importar el modo (ej. "regar ahora"). 0 cancela.
void relayPulse(uint8_t i, uint32_t seconds);
uint32_t relayPulseRemaining(uint8_t i);  // segundos
