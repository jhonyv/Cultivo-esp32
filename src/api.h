#pragma once
#include <ArduinoJson.h>

// API JSON compartida entre la página local (HTTP) y la nube (MQTT).
// Devuelve el código HTTP y deja la respuesta en `out`.
// `query` = parámetros como "range=week&tent=1". `remote` = pedido desde la nube: no se permite tocar el WiFi.
int apiHandle(const String& method, const String& path, const String& query, const String& body, JsonDocument& out,
              bool remote);
void apiLoop();  // reinicio pedido por /api/restart
