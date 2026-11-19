#include "relays.h"
#include <Wire.h>
#include "settings.h"

static bool states[NUM_RELAYS];
static bool autoStates[NUM_RELAYS];
static uint32_t pulseStart[NUM_RELAYS];
static uint32_t pulseMs[NUM_RELAYS];

// Relés 9-16 en el PCF8574: se escribe el byte entero cuando cambia (y cada tanto, por si se reconectó).
static bool expanderReady = false;
static bool expanderPresent = false;
static uint8_t expanderOut = 0xFF;
static int16_t expanderWritten = -1;
static uint32_t expanderLastWrite = 0;

static bool levelFor(bool on) { return on != settings.relayActiveLow; }

static void writeRelay(uint8_t i, bool on) {
  if (i < NUM_GPIO_RELAYS) {
    digitalWrite(RELAY_PINS[i], levelFor(on) ? HIGH : LOW);
  } else {
    uint8_t bit = 1 << (i - NUM_GPIO_RELAYS);
    expanderOut = levelFor(on) ? (expanderOut | bit) : (expanderOut & ~bit);
  }
  if (states[i] != on) Serial.printf("[relay] %u %s -> %s\n", i + 1, settings.relays[i].name, on ? "ON" : "OFF");
  states[i] = on;
}

static void flushExpander() {
  if (!expanderReady) return;
  if (expanderOut == expanderWritten && millis() - expanderLastWrite < 5000) return;
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(expanderOut);
  bool ok = Wire.endTransmission() == 0;
  if (ok != expanderPresent) Serial.printf("[relay] expansor PCF8574 %s\n", ok ? "encontrado" : "no responde");
  expanderPresent = ok;
  expanderWritten = ok ? expanderOut : -1;
  expanderLastWrite = millis();
}

void relaysBegin() {
  for (uint8_t i = 0; i < NUM_GPIO_RELAYS; i++) {
    // Fijar el nivel "apagado" antes de pasar a salida para que el relé no haga clic al arrancar.
    digitalWrite(RELAY_PINS[i], levelFor(false) ? HIGH : LOW);
    pinMode(RELAY_PINS[i], OUTPUT);
  }
  relaysUpdate();
}

void relaysBeginExpander() {
  expanderReady = true;
  flushExpander();
}

void relaysUpdate() {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    if (pulseMs[i] && millis() - pulseStart[i] >= pulseMs[i]) pulseMs[i] = 0;
    bool on;
    switch (settings.relays[i].mode) {
      case RelayMode::On: on = true; break;
      case RelayMode::Auto: on = autoStates[i]; break;
      default: on = false; break;
    }
    writeRelay(i, on || pulseMs[i]);
  }
  flushExpander();
}

bool relayState(uint8_t i) { return i < NUM_RELAYS && states[i]; }
bool relayPresent(uint8_t i) { return i < NUM_GPIO_RELAYS || (i < NUM_RELAYS && expanderPresent); }

void relaySetAuto(uint8_t i, bool on) {
  if (i < NUM_RELAYS) autoStates[i] = on;
}

void relayPulse(uint8_t i, uint32_t seconds) {
  if (i >= NUM_RELAYS) return;
  pulseStart[i] = millis();
  pulseMs[i] = seconds * 1000;
}

uint32_t relayPulseRemaining(uint8_t i) {
  if (i >= NUM_RELAYS || !pulseMs[i]) return 0;
  uint32_t elapsed = millis() - pulseStart[i];
  return elapsed >= pulseMs[i] ? 0 : (pulseMs[i] - elapsed + 999) / 1000;
}
