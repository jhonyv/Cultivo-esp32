#pragma once
#include <Arduino.h>

// WiFi: se conecta a la red guardada; si no hay o falla, levanta un AP propio
// ("Cultivo-XXXX") con portal cautivo para elegir la red desde el celular.
void netBegin();
void netLoop();

bool netConnected();
bool netApActive();
String netApSsid();
String netSavedSsid();

void netSaveCredentials(const String& ssid, const String& pass);  // guarda y reconecta
void netForget();                                                 // borra la red y reinicia
