#include "clock.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "settings.h"

static const uint8_t DS3231_ADDR = 0x68;
static const time_t MIN_VALID = 1704067200;  // 2024-01-01

static bool hasRtc = false;
static volatile bool ntpSynced = false;
static volatile bool pendingRtcWrite = false;

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static bool rtcRead(time_t& out) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(DS3231_ADDR, (uint8_t)7) != 7) return false;
  uint8_t r[7];
  for (uint8_t i = 0; i < 7; i++) r[i] = Wire.read();

  struct tm tm = {};
  tm.tm_sec = bcd2dec(r[0] & 0x7F);
  tm.tm_min = bcd2dec(r[1] & 0x7F);
  tm.tm_hour = bcd2dec(r[2] & 0x3F);
  tm.tm_mday = bcd2dec(r[4] & 0x3F);
  tm.tm_mon = bcd2dec(r[5] & 0x1F) - 1;
  tm.tm_year = bcd2dec(r[6]) + 100 + ((r[5] & 0x80) ? 100 : 0);

  // mktime usa la TZ local; el RTC guarda UTC, así que se calcula con TZ=UTC.
  String tz = getenv("TZ") ? getenv("TZ") : "";
  setenv("TZ", "UTC0", 1);
  tzset();
  out = mktime(&tm);
  setenv("TZ", tz.c_str(), 1);
  tzset();
  return true;
}

static bool rtcWrite(time_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(dec2bcd(tm.tm_sec));
  Wire.write(dec2bcd(tm.tm_min));
  Wire.write(dec2bcd(tm.tm_hour));
  Wire.write(dec2bcd(tm.tm_wday + 1));
  Wire.write(dec2bcd(tm.tm_mday));
  Wire.write(dec2bcd(tm.tm_mon + 1));
  Wire.write(dec2bcd(tm.tm_year - 100));
  if (Wire.endTransmission() != 0) return false;

  // Limpiar el flag OSF (oscilador detenido) del registro de estado.
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x0F);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

static void onNtpSync(struct timeval*) {
  ntpSynced = true;
  pendingRtcWrite = true;  // escribir el RTC desde el loop, no desde el callback de SNTP
}

void clockSetTimezone(const char* tz) {
  setenv("TZ", tz, 1);
  tzset();
}

void clockBegin() {
  clockSetTimezone(settings.tz);

  Wire.beginTransmission(DS3231_ADDR);
  hasRtc = Wire.endTransmission() == 0;
  time_t t;
  if (hasRtc && rtcRead(t) && t > MIN_VALID) {
    struct timeval tv = {t, 0};
    settimeofday(&tv, nullptr);
    Serial.println("[clock] hora tomada del DS3231");
  } else {
    Serial.println(hasRtc ? "[clock] DS3231 sin hora válida" : "[clock] DS3231 no encontrado");
  }

  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(settings.tz, "pool.ntp.org", "time.google.com");
}

void clockLoop() {
  if (!pendingRtcWrite) return;
  pendingRtcWrite = false;
  if (hasRtc && rtcWrite(time(nullptr))) Serial.println("[clock] DS3231 actualizado desde NTP");
}

void clockSet(time_t utc) {
  struct timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
  if (hasRtc) rtcWrite(utc);
}

bool clockValid() { return time(nullptr) > MIN_VALID; }
bool clockHasRtc() { return hasRtc; }
bool clockNtpSynced() { return ntpSynced; }
