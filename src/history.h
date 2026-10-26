#pragma once
#include <ArduinoJson.h>
#include "config.h"

// Historial:
// - últimas 24 h cada 5 min, en RAM (gráfico de Inicio y vista "día");
// - un registro por hora guardado en la flash (LittleFS) para semana y mes,
//   con promedio/mín/máx de clima y cuánto estuvieron prendidas luces, coolers y riego.
#define HISTORY_INTERVAL_S 300
#define HISTORY_SIZE 288
#define HISTORY_HOURS (24 * 35)

void historyBegin();
void historyLoop();
void historyToJson(JsonObject obj, uint8_t tent);                         // 5 min, últimas 24 h
void historyHourlyToJson(JsonObject obj, uint8_t tent, uint16_t hours);  // por hora
void historyDailyToJson(JsonObject obj, uint8_t tent, uint8_t days);     // por día (hora local)
