#include <Arduino.h>
#include <Wire.h>
#include "api.h"
#include "automation.h"
#include "cloud.h"
#include "clock.h"
#include "config.h"
#include "history.h"
#include "net.h"
#include "relays.h"
#include "sensors.h"
#include "settings.h"
#include "soil.h"
#include "tank.h"
#include "telegram.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[cultivo] arrancando");

  settingsBegin();
  relaysBegin();  // lo antes posible, para fijar los relés en apagado

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
  relaysBeginExpander();
  clockBegin();
  historyBegin();
  sensorsBegin();
  soilBegin();
  tankBegin();

  netBegin();
  webBegin();
  telegramBegin();
  cloudBegin();
}

void loop() {
  netLoop();
  webLoop();
  sensorsLoop();
  soilLoop();
  tankLoop();
  clockLoop();
  historyLoop();
  automationLoop();
  telegramLoop();
  cloudLoop();
  apiLoop();
  relaysUpdate();
  delay(2);
}
