#pragma once
#include <Arduino.h>

// Ver docs/hardware.md para el esquema de conexión.

#define NUM_TENTS 2
// Relés por carpa. Se elige con el environment de PlatformIO:
//   cultivo-4 (por defecto): 4 por carpa, 8 en total, todos directo al ESP32.
//   cultivo-8: 8 por carpa, 16 en total; los relés 9-16 van por un expansor PCF8574.
#ifndef RELAYS_PER_TENT
#define RELAYS_PER_TENT 4
#endif
#define NUM_RELAYS (RELAYS_PER_TENT * NUM_TENTS)
// Los primeros 8 relés van directo en GPIO (con 4 por carpa: carpa 1 = 16-19, carpa 2 = 23-27).
static const uint8_t RELAY_PINS[8] = {16, 17, 18, 19, 23, 25, 26, 27};
#define NUM_GPIO_RELAYS (NUM_RELAYS < 8 ? NUM_RELAYS : 8)
#define HAS_RELAY_EXPANDER (NUM_RELAYS > NUM_GPIO_RELAYS)
#define PCF8574_ADDR 0x20

// Bus I2C principal: DS3231, PCF8574 y los sensores de la carpa 1.
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
// Segundo bus I2C: sensores de la carpa 2.
#define PIN_I2C2_SDA 4
#define PIN_I2C2_SCL 5
#define I2C_FREQ 100000
// Humedad de suelo (opcional): 2 sensores capacitivos analógicos por carpa, en pines ADC1
// (los únicos que leen con el WiFi prendido). Cada entrada lleva una resistencia de 100k a GND
// para que, sin sensor, lea 0 y el equipo sepa que no hay nada conectado.
#define SOIL_SENSORS_MAX 2
static const uint8_t SOIL_PINS[2][SOIL_SENSORS_MAX] = {{36, 39}, {32, 33}};

// Nivel del tanque (opcional): ultrasónico JSN-SR04T por carpa. ECHO por divisor 1k/2k (sale a 5 V).
static const uint8_t TANK_TRIG_PINS[NUM_TENTS] = {13, 14};
static const uint8_t TANK_ECHO_PINS[NUM_TENTS] = {34, 35};  // solo entrada

#define PIN_BUTTON 0  // botón BOOT: mantener 5 s para borrar el WiFi
#define PIN_LED 2

#define AP_SSID_PREFIX "Cultivo-"
#define AP_PASSWORD_DEFAULT "cultivo123"  // mínimo 8 caracteres, "" = red abierta
#define HOSTNAME "cultivo"                // http://cultivo.local
#define TZ_DEFAULT "<-03>3"               // Argentina (POSIX TZ)

#define SENSOR_INTERVAL_MS 2000
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_MS 60000
#define WIFI_RETRY_AP_CLIENT_MS 300000  // con alguien conectado a la red propia se reintenta menos (cada intento la corta un momento)
#define AP_OFF_DELAY_MS 120000  // apaga el AP 2 min después de conectarse a tu WiFi
