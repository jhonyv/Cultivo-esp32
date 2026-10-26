#pragma once
#include "config.h"

void relaysBegin();
// Aplica el modo de cada relé: on/off manual, o en auto lo que pidan las automatizaciones.
void relaysUpdate();
bool relayState(uint8_t i);
// Lo que las automatizaciones quieren para un relé en modo auto.
void relaySetAuto(uint8_t i, bool on);
