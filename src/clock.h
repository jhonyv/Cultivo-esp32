#pragma once
#include <time.h>

// Hora del sistema: NTP cuando hay internet, DS3231 como respaldo (guarda UTC).
void clockBegin();
void clockLoop();
bool clockValid();
bool clockHasRtc();
bool clockNtpSynced();
void clockSetTimezone(const char* tz);
// Fija la hora a mano (epoch UTC), útil sin internet. También actualiza el RTC.
void clockSet(time_t utc);
