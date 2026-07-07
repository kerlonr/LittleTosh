/*
  night_clock.h
  -------------
  Hora certa via NTP (SNTP do proprio core do ESP8266).
  Depois do primeiro sync a hora continua andando sozinha (millis) e o core
  re-sincroniza de tempos em tempos enquanto houver internet.
*/
#pragma once

#include <Arduino.h>

class NightClock {
 public:
  void begin(int16_t utcOffsetMin);       // configura NTP + fuso
  void setOffset(int16_t utcOffsetMin);   // reconfigura (ao salvar novo fuso)

  bool hasTime() const;                   // ja sincronizou alguma vez?
  int  hour() const;                      // 0-23 (-1 se sem hora)
  int  minute() const;                    // 0-59 (-1 se sem hora)
  void hhmm(char *buf, size_t len) const; // "23:41" (ou "--:--" sem hora)
};
