#pragma once
#include <Arduino.h>

// Ver docs/hardware.md para el esquema de conexión.

#define NUM_RELAYS 8
static const uint8_t RELAY_PINS[NUM_RELAYS] = {16, 17, 18, 19, 23, 25, 26, 27};

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_FREQ 100000

#define PIN_BUTTON 0  // botón BOOT: mantener 5 s para borrar el WiFi
#define PIN_LED 2

#define AP_SSID_PREFIX "Cultivo-"
#define AP_PASSWORD_DEFAULT "cultivo123"  // mínimo 8 caracteres, "" = red abierta
#define HOSTNAME "cultivo"                // http://cultivo.local
#define TZ_DEFAULT "<-03>3"               // Argentina (POSIX TZ)

#define SENSOR_INTERVAL_MS 2000
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_MS 60000
#define AP_OFF_DELAY_MS 120000  // apaga el AP 2 min después de conectarse a tu WiFi
