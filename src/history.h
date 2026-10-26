#pragma once
#include <ArduinoJson.h>

// Historial de las últimas 24 h en RAM (una muestra cada 5 min).
#define HISTORY_INTERVAL_S 300
#define HISTORY_SIZE 288

void historyLoop();
void historyToJson(JsonObject obj);
