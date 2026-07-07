#include "mpu_sensor.h"

#include <Wire.h>

#include "config.h"

// Registradores comuns a 6050/6500/9250.
constexpr uint8_t MPU_REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t MPU_REG_SMPLRT_DIV = 0x19;
constexpr uint8_t MPU_REG_CONFIG     = 0x1A;
constexpr uint8_t MPU_REG_GYRO_CFG   = 0x1B;
constexpr uint8_t MPU_REG_ACCEL_CFG  = 0x1C;
constexpr uint8_t MPU_REG_ACCEL_XOUT = 0x3B;
constexpr uint8_t MPU_REG_WHOAMI     = 0x75;
constexpr float ACCEL_LSB_PER_G  = 4096.0f;  // +-8 g
constexpr float GYRO_LSB_PER_DPS = 65.5f;    // +-500 dps

bool MpuSensor::begin() {
  int w = readWhoAmI(0x68);
  if (w != -1) {
    addr = 0x68;
  } else {
    w = readWhoAmI(0x69);
    if (w == -1) return false;
    addr = 0x69;
  }
  who = w;

  writeReg(MPU_REG_PWR_MGMT_1, 0x00);  // sai do sleep
  delay(10);
  writeReg(MPU_REG_SMPLRT_DIV, 0x00);  // 1 kHz
  writeReg(MPU_REG_CONFIG,     0x03);  // DLPF ~44 Hz
  writeReg(MPU_REG_GYRO_CFG,   0x08);  // +-500 dps
  writeReg(MPU_REG_ACCEL_CFG,  0x10);  // +-8 g
  return true;
}

bool MpuSensor::read(uint32_t now) {
  if (now - lastSampleAt < SENSOR_SAMPLE_MS) return false;
  lastSampleAt = now;

  Wire.beginTransmission(addr);
  Wire.write(MPU_REG_ACCEL_XOUT);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)14) != 14) return false;

  const int16_t axr = read16();
  const int16_t ayr = read16();
  const int16_t azr = read16();
  const int16_t tr  = read16();
  const int16_t gxr = read16();
  const int16_t gyr = read16();
  const int16_t gzr = read16();

  smp.ax = (axr / ACCEL_LSB_PER_G) * GRAVITY;
  smp.ay = (ayr / ACCEL_LSB_PER_G) * GRAVITY;
  smp.az = (azr / ACCEL_LSB_PER_G) * GRAVITY;
  smp.gx = (gxr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  smp.gy = (gyr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  smp.gz = (gzr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  smp.tempC = rawToTempC(tr);

  const float mag = sqrtf(smp.ax * smp.ax + smp.ay * smp.ay + smp.az * smp.az);
  smp.dynAccel = fabsf(mag - GRAVITY);
  return true;
}

Tilt MpuSensor::classifyTilt() const {
  const float ax = INVERT_X ? -smp.ax : smp.ax;
  const float ay = INVERT_Y ? -smp.ay : smp.ay;
  const float az = smp.az;
  const float aax = fabsf(ax), aay = fabsf(ay), aaz = fabsf(az);

  if (aaz > aax && aaz > aay && aaz > (GRAVITY - TILT_THRESHOLD)) {
    return Tilt::FLAT;
  }
  if (aax >= aay) {
    if (ax >=  TILT_THRESHOLD) return Tilt::RIGHT;
    if (ax <= -TILT_THRESHOLD) return Tilt::LEFT;
  } else {
    if (ay >=  TILT_THRESHOLD) return Tilt::DOWN;
    if (ay <= -TILT_THRESHOLD) return Tilt::UP;
  }
  return Tilt::FLAT;
}

bool MpuSensor::detectTap(uint32_t now) {
  if (smp.dynAccel < TAP_THRESHOLD)      return false;
  if (now - lastTapAt < TAP_DEBOUNCE_MS) return false;
  lastTapAt = now;
  return true;
}

bool MpuSensor::detectShake(uint32_t now) {
  if (shakePeaks > 0 && (now - firstPeakAt > SHAKE_WINDOW_MS)) {
    shakePeaks = 0;
  }
  if (armedForPeak && smp.dynAccel >= SHAKE_PEAK_HI) {
    if (shakePeaks == 0) firstPeakAt = now;
    shakePeaks++;
    armedForPeak = false;
  } else if (!armedForPeak && smp.dynAccel <= SHAKE_PEAK_LO) {
    armedForPeak = true;
  }
  if (shakePeaks >= SHAKE_PEAKS_NEEDED) {
    shakePeaks = 0;
    return true;
  }
  return false;
}

void MpuSensor::resetShake() {
  shakePeaks   = 0;
  armedForPeak = true;
}

void MpuSensor::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int MpuSensor::readWhoAmI(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(MPU_REG_WHOAMI);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(address, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

int16_t MpuSensor::read16() {
  const uint8_t hi = Wire.read();
  const uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

float MpuSensor::rawToTempC(int16_t raw) const {
  if (who == 0x68) return raw / 340.0f + 36.53f;  // MPU-6050
  return raw / 333.87f + 21.0f;                   // MPU-6500/9250
}

const char *tiltLabel(Tilt t) {
  switch (t) {
    case Tilt::FLAT:  return "FLAT";
    case Tilt::LEFT:  return "LEFT";
    case Tilt::RIGHT: return "RIGHT";
    case Tilt::UP:    return "UP";
    case Tilt::DOWN:  return "DOWN";
    default:          return "?";
  }
}
