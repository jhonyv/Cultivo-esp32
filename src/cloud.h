#pragma once
#include <Arduino.h>

// Nube por MQTT (ej. HiveMQ Cloud): el equipo se conecta al broker y atiende la misma API
// que la página local, así el panel funciona desde cualquier lado sin abrir puertos.
// Tópicos: cultivo/<id>/req (pedidos), cultivo/<id>/res (respuestas), cultivo/<id>/online.
void cloudBegin();
void cloudLoop();
bool cloudConnected();
String cloudDeviceId();
