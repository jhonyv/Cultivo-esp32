#pragma once
#include <Arduino.h>

// WiFi: se conecta a la red guardada; si no hay o falla, levanta un AP propio
// ("Cultivo-XXXX") con portal cautivo para elegir la red desde el celular. Con settings.apAlways
// la red propia queda siempre prendida: el panel anda en 192.168.4.1 sin router ni internet.
void netBegin();
void netLoop();

bool netConnected();
bool netApActive();
String netApSsid();
String netSavedSsid();

void netSaveCredentials(const String& ssid, const String& pass);  // guarda y reconecta
void netUseApOnly();                                             // sin WiFi de casa: solo la red propia, siempre prendida
void netForget();                                                 // borra la red y reinicia
