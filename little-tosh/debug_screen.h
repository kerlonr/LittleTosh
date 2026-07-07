/*
  debug_screen.h
  --------------
  Tela de diagnostico acionada pela chacoalhada: dados crus do MPU +
  (novidade) hora atual no topo e IP/estado do WiFi no rodape.
  Sai sozinha depois de alguns segundos.
*/
#pragma once

#include <Adafruit_SSD1306.h>

#include "mpu_sensor.h"
#include "net_manager.h"
#include "night_clock.h"

class DebugScreen {
 public:
  DebugScreen(Adafruit_SSD1306 &display, MpuSensor &mpu, NetManager &net,
              NightClock &clock)
      : display(display), mpu(mpu), net(net), clock(clock) {}

  void enter(uint32_t now);
  void run(uint32_t now, Tilt tilt);  // desenha/atualiza; expira sozinho
  bool active() const { return isActive; }

 private:
  void draw(uint32_t now, Tilt tilt);
  void field(int16_t x, int16_t y, const __FlashStringHelper *label,
             float value, uint8_t decimals);

  Adafruit_SSD1306 &display;
  MpuSensor        &mpu;
  NetManager       &net;
  NightClock       &clock;

  bool     isActive   = false;
  uint32_t startedAt  = 0;
  uint32_t lastDrawAt = 0;
};
