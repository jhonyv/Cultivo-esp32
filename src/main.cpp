#include <Arduino.h>
#include <Wire.h>
#include "automation.h"
#include "clock.h"
#include "config.h"
#include "history.h"
#include "net.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[cultivo] arrancando");

  settingsBegin();
  relaysBegin();  // lo antes posible, para fijar los relés en apagado

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
  clockBegin();
  sensorsBegin();

  netBegin();
  webBegin();
}

void loop() {
  netLoop();
  webLoop();
  sensorsLoop();
  clockLoop();
  historyLoop();
  automationLoop();
  relaysUpdate();
  delay(2);
}
