#include "relays.h"
#include "settings.h"

static bool states[NUM_RELAYS];
static bool autoStates[NUM_RELAYS];

static void writeRelay(uint8_t i, bool on) {
  digitalWrite(RELAY_PINS[i], on != settings.relayActiveLow ? HIGH : LOW);
  if (states[i] != on) Serial.printf("[relay] %u %s -> %s\n", i + 1, settings.relays[i].name, on ? "ON" : "OFF");
  states[i] = on;
}

void relaysBegin() {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    // Fijar el nivel "apagado" antes de pasar a salida para que el relé no haga clic al arrancar.
    digitalWrite(RELAY_PINS[i], settings.relayActiveLow ? HIGH : LOW);
    pinMode(RELAY_PINS[i], OUTPUT);
    states[i] = false;
    autoStates[i] = false;
  }
  relaysUpdate();
}

void relaysUpdate() {
  for (uint8_t i = 0; i < NUM_RELAYS; i++) {
    bool on;
    switch (settings.relays[i].mode) {
      case RelayMode::On: on = true; break;
      case RelayMode::Auto: on = autoStates[i]; break;
      default: on = false; break;
    }
    writeRelay(i, on);
  }
}

bool relayState(uint8_t i) { return i < NUM_RELAYS && states[i]; }

void relaySetAuto(uint8_t i, bool on) {
  if (i < NUM_RELAYS) autoStates[i] = on;
}
