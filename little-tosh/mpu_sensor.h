/*
  mpu_sensor.h
  ------------
  Driver I2C minimo para a familia MPU (6050 / 6500 / 9250) + deteccao de
  gestos: inclinacao (tilt), tapinha (tap) e chacoalhada (shake).
  Sem dependencia de biblioteca externa alem da Wire.
*/
#pragma once

#include <Arduino.h>

enum class Tilt : uint8_t { FLAT, LEFT, RIGHT, UP, DOWN };

struct MpuSample {
  float ax, ay, az;   // aceleracao (m/s^2)
  float gx, gy, gz;   // giro (rad/s)
  float tempC;        // temperatura interna do chip
  float dynAccel;     // |aceleracao| - gravidade (movimento "liquido")
};

class MpuSensor {
 public:
  bool begin();                    // detecta em 0x68/0x69 e configura escalas
  bool read(uint32_t now);         // true se colheu amostra nova (a cada SENSOR_SAMPLE_MS)

  Tilt classifyTilt() const;       // FLAT / LEFT / RIGHT / UP / DOWN
  bool detectTap(uint32_t now);    // pico curto (com debounce)
  bool detectShake(uint32_t now);  // varios picos numa janela de tempo
  void resetShake();               // zera a contagem (ao sair do debug)

  const MpuSample &sample() const { return smp; }
  uint8_t address() const { return addr; }
  int     whoAmI()  const { return who; }

 private:
  void    writeReg(uint8_t reg, uint8_t value);
  int     readWhoAmI(uint8_t address);
  int16_t read16();
  float   rawToTempC(int16_t raw) const;

  MpuSample smp = {};
  uint8_t addr = 0x68;
  int     who  = 0;

  uint32_t lastSampleAt = 0;
  uint32_t lastTapAt    = 0;

  uint8_t  shakePeaks   = 0;
  uint32_t firstPeakAt  = 0;
  bool     armedForPeak = true;
};

const char *tiltLabel(Tilt t);
