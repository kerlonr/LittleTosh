#include "night_clock.h"

#include <time.h>

#include "config.h"

// Qualquer coisa antes disso = o relogio ainda esta em 1970 (sem sync).
constexpr time_t MIN_VALID_EPOCH = 1000000000;  // ~2001

void NightClock::begin(int16_t utcOffsetMin) {
  configTime(utcOffsetMin * 60, 0, NTP_SERVER_1, NTP_SERVER_2);
}

void NightClock::setOffset(int16_t utcOffsetMin) {
  begin(utcOffsetMin);
}

bool NightClock::hasTime() const {
  return time(nullptr) > MIN_VALID_EPOCH;
}

int NightClock::hour() const {
  if (!hasTime()) return -1;
  time_t t = time(nullptr);
  struct tm *lt = localtime(&t);
  return lt->tm_hour;
}

int NightClock::minute() const {
  if (!hasTime()) return -1;
  time_t t = time(nullptr);
  struct tm *lt = localtime(&t);
  return lt->tm_min;
}

void NightClock::hhmm(char *buf, size_t len) const {
  if (!hasTime()) {
    strncpy(buf, "--:--", len);
    if (len) buf[len - 1] = '\0';
    return;
  }
  time_t t = time(nullptr);
  struct tm *lt = localtime(&t);
  snprintf(buf, len, "%02d:%02d", lt->tm_hour, lt->tm_min);
}
