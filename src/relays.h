#pragma once
#include "config.h"

void relaysBegin();
// Aplica el modo de cada relé: on/off manual, o en auto lo que pidan las automatizaciones.
void relaysUpdate();
bool relayState(uint8_t i);
// Lo que las automatizaciones quieren para un relé en modo auto.
void relaySetAuto(uint8_t i, bool on);
// Enciende el relé un rato sin importar el modo (ej. "regar ahora"). 0 cancela.
void relayPulse(uint8_t i, uint32_t seconds);
uint32_t relayPulseRemaining(uint8_t i);  // segundos
