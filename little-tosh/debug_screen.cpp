#include "debug_screen.h"

#include "config.h"

void DebugScreen::enter(uint32_t now) {
  isActive   = true;
  startedAt  = now;
  lastDrawAt = 0;
}

void DebugScreen::run(uint32_t now, Tilt tilt) {
  if (now - startedAt >= DEBUG_DURATION_MS) {
    isActive = false;
    return;
  }
  if (now - lastDrawAt >= DEBUG_REFRESH_MS) {
    lastDrawAt = now;
    draw(now, tilt);
  }
}

void DebugScreen::draw(uint32_t now, Tilt tilt) {
  const MpuSample &s = mpu.sample();
  const uint32_t remaining = (DEBUG_DURATION_MS - (now - startedAt)) / 1000 + 1;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("= MPU DEBUG ="));

  // hora atual no canto (ou --:-- se nunca sincronizou)
  char timeBuf[8];
  clock.hhmm(timeBuf, sizeof(timeBuf));
  display.setCursor(98, 0);
  display.print(timeBuf);

  field(0,  10, F("ax"),  s.ax, 1);
  field(64, 10, F("ay"),  s.ay, 1);
  field(0,  19, F("az"),  s.az, 1);
  field(64, 19, F("|d|"), s.dynAccel, 1);
  field(0,  28, F("gx"),  s.gx, 2);
  field(64, 28, F("gy"),  s.gy, 2);
  field(0,  37, F("gz"),  s.gz, 2);
  field(64, 37, F("T"),   s.tempC, 1);

  display.setCursor(0, 47);
  display.print(F("pos: "));
  display.print(tiltLabel(tilt));

  // rodape alterna entre a contagem regressiva e o estado da rede
  display.setCursor(0, 56);
  if ((now / 2000) % 2 == 0) {
    display.print(F("saindo em "));
    display.print(remaining);
    display.print(F("s"));
  } else {
    char netBuf[24];
    net.statusLine(netBuf, sizeof(netBuf));
    display.print(netBuf);
  }

  display.display();
}

void DebugScreen::field(int16_t x, int16_t y, const __FlashStringHelper *label,
                        float value, uint8_t decimals) {
  display.setCursor(x, y);
  display.print(label);
  display.print(' ');
  display.print(value, decimals);
}
