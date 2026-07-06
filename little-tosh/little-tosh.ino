/*
  LittleTosh
  ----------
  Olhos animados (FluxGarage RoboEyes) num OLED SSD1306 128x64 que reagem ao
  movimento lido por um sensor da familia MPU (MPU-6050 / MPU-6500 / MPU-9250),
  rodando num Wemos D1 mini (ESP8266).

  Comportamento:
    - Parado e nivelado .......... olhos relaxam e entram em "idle".
    - Inclinado ................... os olhos "caem" para o lado da inclinacao.
    - Toque/tapinha (pico curto) .. animacao de risada.
    - Chacoalhada (varios picos) .. entra em MODO DEBUG por alguns segundos,
                                    mostrando os dados crus do MPU.

  Ligacoes (I2C padrao do D1 mini):
    SDA -> D2 (GPIO4)   |   SCL -> D1 (GPIO5)
    OLED SSD1306 = 0x3C   |   MPU = 0x68 (ou 0x69 se AD0 em nivel alto)

  O sensor e lido por um driver I2C minimo proprio (aceita 6050/6500/9250).

  Bibliotecas (Library Manager): Adafruit GFX, Adafruit SSD1306,
  FluxGarage RoboEyes.  Placa: "LOLIN(WEMOS) D1 R2 & mini".
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// O core do ESP8266 define DEFAULT == 1; a RoboEyes precisa de DEFAULT == 0.
#ifdef DEFAULT
#undef DEFAULT
#endif
#include <FluxGarage_RoboEyes.h>

// ============================================================================
//  HARDWARE / DISPLAY
// ============================================================================
constexpr uint8_t  SCREEN_WIDTH  = 128;
constexpr uint8_t  SCREEN_HEIGHT = 64;
constexpr int8_t   OLED_RESET    = -1;
constexpr uint8_t  OLED_ADDR     = 0x3C;
constexpr uint32_t I2C_CLOCK_HZ        = 400000UL;
constexpr uint32_t I2C_CLOCK_DETECT_HZ = 100000UL;

// Pinos I2C do D1 mini (para o diagnostico fisico do barramento).
constexpr uint8_t  PIN_SDA = 4;  // D2 (GPIO4)
constexpr uint8_t  PIN_SCL = 5;  // D1 (GPIO5)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> eyes(display);

// ============================================================================
//  MPU (driver I2C minimo) — registradores comuns a 6050/6500/9250
// ============================================================================
constexpr uint8_t MPU_REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t MPU_REG_SMPLRT_DIV = 0x19;
constexpr uint8_t MPU_REG_CONFIG     = 0x1A;
constexpr uint8_t MPU_REG_GYRO_CFG   = 0x1B;
constexpr uint8_t MPU_REG_ACCEL_CFG  = 0x1C;
constexpr uint8_t MPU_REG_ACCEL_XOUT = 0x3B;
constexpr uint8_t MPU_REG_WHOAMI     = 0x75;
constexpr float ACCEL_LSB_PER_G  = 4096.0f;  // +-8 g
constexpr float GYRO_LSB_PER_DPS = 65.5f;    // +-500 dps

uint8_t mpuAddr   = 0x68;
int     mpuWhoAmI = 0;

// ============================================================================
//  APARENCIA DOS OLHOS  (mude o tamanho aqui)
//
//  LIMITES: a RoboEyes centraliza com (tela - tamanho)/2. Se o olho for grande
//  demais esse calculo fica negativo e os olhos vao para FORA da tela (OLED
//  preto). Com EYE_SPACING = 10, mantenha:
//      EYE_WIDTH  <= 58   (2*largura + espaco <= 128)
//      EYE_HEIGHT <= 56   (sobra 8 px para o "olhar curioso")
// ============================================================================
constexpr uint8_t EYE_WIDTH         = 36;
constexpr uint8_t EYE_HEIGHT        = 36;
constexpr uint8_t EYE_BORDER_RADIUS = 10;
constexpr uint8_t EYE_SPACING       = 10;

// Trava de seguranca: se passar do tamanho, a COMPILACAO falha com mensagem
// clara (em vez de silenciosamente desenhar os olhos fora da tela).
static_assert(2 * EYE_WIDTH + EYE_SPACING <= SCREEN_WIDTH,
              "EYE_WIDTH grande demais: 2*EYE_WIDTH + EYE_SPACING precisa caber em 128.");
static_assert(EYE_HEIGHT + 8 <= SCREEN_HEIGHT,
              "EYE_HEIGHT grande demais: use no maximo 56 (sobra p/ o olhar curioso).");

// ============================================================================
//  TEMPOS (ms)
// ============================================================================
constexpr uint8_t  TARGET_FPS        = 60;
constexpr uint32_t SENSOR_SAMPLE_MS  = 20;
constexpr uint32_t IDLE_DELAY_MS     = 2500;
constexpr uint32_t TAP_DEBOUNCE_MS   = 600;
constexpr uint32_t REACTION_MS       = 1200;
constexpr uint32_t DEBUG_DURATION_MS = 6000;
constexpr uint32_t DEBUG_REFRESH_MS  = 60;

// ============================================================================
//  LIMIARES DE MOVIMENTO (m/s^2)
// ============================================================================
constexpr float    GRAVITY            = 9.81f;
constexpr float    TILT_THRESHOLD     = 4.0f;
constexpr float    TAP_THRESHOLD      = 12.0f;
constexpr float    SHAKE_PEAK_HI      = 14.0f;
constexpr float    SHAKE_PEAK_LO      = 6.0f;
constexpr uint8_t  SHAKE_PEAKS_NEEDED = 4;
constexpr uint32_t SHAKE_WINDOW_MS    = 1200;

// Inverta se as direcoes sairem trocadas conforme a montagem do sensor.
constexpr bool INVERT_X = true;
constexpr bool INVERT_Y = false;

// ============================================================================
//  ESTADO
// ============================================================================
enum class Tilt : uint8_t { FLAT, LEFT, RIGHT, UP, DOWN };
enum class Mode : uint8_t { EYES, DEBUG };

struct MpuSample {
  float ax, ay, az;
  float gx, gy, gz;
  float tempC;
  float dynAccel;
};

MpuSample sample = {};
Mode mode     = Mode::EYES;
Tilt tilt     = Tilt::FLAT;
Tilt prevTilt = Tilt::FLAT;

uint32_t lastSampleAt      = 0;
uint32_t tiltSince         = 0;
uint32_t lastTapAt         = 0;
uint32_t reactionStartedAt = 0;
bool     reacting          = false;
bool     idleActive        = false;

uint32_t debugStartedAt    = 0;
uint32_t lastDebugDrawAt   = 0;

uint8_t  shakePeaks   = 0;
uint32_t firstPeakAt  = 0;
bool     armedForPeak = true;

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(F("=== LittleTosh boot ==="));

  // Diagnostico fisico: com pull-up interno, uma linha I2C livre fica em HIGH.
  // Se ler LOW (0), aquela linha esta presa -> curto, fio solto ou modulo travado.
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  delay(2);
  Serial.print(F("Linhas I2C: SDA(D2)="));
  Serial.print(digitalRead(PIN_SDA));
  Serial.print(F(" SCL(D1)="));
  Serial.print(digitalRead(PIN_SCL));
  Serial.println(F("  (1=livre/ok, 0=presa em LOW -> fiacao/curto)"));

  Wire.begin();
  Wire.setClock(I2C_CLOCK_DETECT_HZ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERRO] OLED nao iniciou."));
    for (;;) delay(1000);
  }
  Serial.println(F("[ok] OLED iniciado."));

  scanI2C();  // varre o barramento (e estabiliza o MPU antes da deteccao)

  if (!mpuInit()) {
    Serial.println(F("[ERRO] MPU nao encontrado."));
    showFatal(F("MPU nao encontrado"));
    for (;;) delay(1000);
  }
  Serial.print(F("[ok] MPU em 0x"));
  Serial.print(mpuAddr, HEX);
  Serial.print(F(" WHO_AM_I=0x"));
  Serial.println(mpuWhoAmI, HEX);

  Wire.setClock(I2C_CLOCK_HZ);

  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, TARGET_FPS);
  eyes.setWidth(EYE_WIDTH, EYE_WIDTH);
  eyes.setHeight(EYE_HEIGHT, EYE_HEIGHT);
  eyes.setBorderradius(EYE_BORDER_RADIUS, EYE_BORDER_RADIUS);
  eyes.setSpacebetween(EYE_SPACING);
  eyes.setAutoblinker(ON, 3, 2);

  Serial.println(F("[ok] LittleTosh pronto."));
}

// ============================================================================
//  LOOP
// ============================================================================
void loop() {
  const uint32_t now = millis();
  const bool freshSample = readMpu(now);

  if (mode == Mode::DEBUG) {
    runDebugMode(now);
    return;
  }

  if (freshSample) {
    if (detectShake(now)) {
      enterDebug(now);
      return;
    }
    tilt = classifyTilt(sample);

    if (!reacting && detectTap(now)) {
      triggerLaugh(now);
    }
  }

  if (reacting && (now - reactionStartedAt >= REACTION_MS)) {
    endReaction(now);
  }

  if (!reacting && tilt != prevTilt) {
    applyTilt(tilt);
    prevTilt  = tilt;
    tiltSince = now;
  }

  updateIdle(now);
  eyes.update();
}

// ============================================================================
//  LEITURA DO MPU
// ============================================================================
bool readMpu(uint32_t now) {
  if (now - lastSampleAt < SENSOR_SAMPLE_MS) return false;
  lastSampleAt = now;

  Wire.beginTransmission(mpuAddr);
  Wire.write(MPU_REG_ACCEL_XOUT);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, (uint8_t)14) != 14) return false;

  const int16_t axr = read16();
  const int16_t ayr = read16();
  const int16_t azr = read16();
  const int16_t tr  = read16();
  const int16_t gxr = read16();
  const int16_t gyr = read16();
  const int16_t gzr = read16();

  sample.ax = (axr / ACCEL_LSB_PER_G) * GRAVITY;
  sample.ay = (ayr / ACCEL_LSB_PER_G) * GRAVITY;
  sample.az = (azr / ACCEL_LSB_PER_G) * GRAVITY;
  sample.gx = (gxr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  sample.gy = (gyr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  sample.gz = (gzr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  sample.tempC = mpuTempC(tr);

  const float mag = sqrtf(sample.ax * sample.ax +
                          sample.ay * sample.ay +
                          sample.az * sample.az);
  sample.dynAccel = fabsf(mag - GRAVITY);
  return true;
}

int16_t read16() {
  const uint8_t hi = Wire.read();
  const uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

float mpuTempC(int16_t raw) {
  if (mpuWhoAmI == 0x68) return raw / 340.0f + 36.53f;  // MPU-6050
  return raw / 333.87f + 21.0f;                         // MPU-6500/9250
}

Tilt classifyTilt(const MpuSample &s) {
  const float ax = INVERT_X ? -s.ax : s.ax;
  const float ay = INVERT_Y ? -s.ay : s.ay;
  const float az = s.az;
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

bool detectTap(uint32_t now) {
  if (sample.dynAccel < TAP_THRESHOLD)   return false;
  if (now - lastTapAt < TAP_DEBOUNCE_MS) return false;
  lastTapAt = now;
  return true;
}

bool detectShake(uint32_t now) {
  if (shakePeaks > 0 && (now - firstPeakAt > SHAKE_WINDOW_MS)) {
    shakePeaks = 0;
  }
  if (armedForPeak && sample.dynAccel >= SHAKE_PEAK_HI) {
    if (shakePeaks == 0) firstPeakAt = now;
    shakePeaks++;
    armedForPeak = false;
  } else if (!armedForPeak && sample.dynAccel <= SHAKE_PEAK_LO) {
    armedForPeak = true;
  }
  if (shakePeaks >= SHAKE_PEAKS_NEEDED) {
    shakePeaks = 0;
    return true;
  }
  return false;
}

// ============================================================================
//  ANIMACOES
// ============================================================================
void applyTilt(Tilt t) {
  switch (t) {
    case Tilt::LEFT:  lookTo(W); break;
    case Tilt::RIGHT: lookTo(E); break;
    case Tilt::UP:    lookTo(N); break;
    case Tilt::DOWN:  lookTo(S); break;
    case Tilt::FLAT:
    default:
      eyes.setPosition(DEFAULT);
      eyes.setCuriosity(OFF);
      eyes.setMood(DEFAULT);
      break;
  }
}

void lookTo(uint8_t position) {
  eyes.setPosition(position);
  eyes.setCuriosity(ON);
  eyes.setAutoblinker(ON, 3, 5);
}

void triggerLaugh(uint32_t now) {
  eyes.setMood(HAPPY);
  eyes.setCuriosity(ON);
  eyes.anim_laugh();
  reacting          = true;
  reactionStartedAt = now;
  tiltSince         = now;
  setIdle(false);
}

void endReaction(uint32_t now) {
  eyes.setMood(DEFAULT);
  eyes.setCuriosity(OFF);
  eyes.setAutoblinker(ON, 3, 2);
  reacting  = false;
  prevTilt  = Tilt::FLAT;
  tiltSince = now;
}

void updateIdle(uint32_t now) {
  const bool shouldIdle = (tilt == Tilt::FLAT) && !reacting &&
                          (now - tiltSince >= IDLE_DELAY_MS);
  setIdle(shouldIdle);
}

void setIdle(bool on) {
  if (on == idleActive) return;
  eyes.setIdleMode(on ? ON : OFF, 2, 2);
  idleActive = on;
}

// ============================================================================
//  MODO DEBUG (acionado pela chacoalhada)
// ============================================================================
void enterDebug(uint32_t now) {
  mode            = Mode::DEBUG;
  debugStartedAt  = now;
  lastDebugDrawAt = 0;
  reacting        = false;
  setIdle(false);
}

void runDebugMode(uint32_t now) {
  if (now - debugStartedAt >= DEBUG_DURATION_MS) {
    exitDebug(now);
    return;
  }
  if (now - lastDebugDrawAt >= DEBUG_REFRESH_MS) {
    lastDebugDrawAt = now;
    drawDebugOverlay(now);
  }
}

void exitDebug(uint32_t now) {
  mode         = Mode::EYES;
  prevTilt     = Tilt::FLAT;
  tiltSince    = now;
  shakePeaks   = 0;
  armedForPeak = true;
  display.clearDisplay();
  display.display();
}

void drawDebugOverlay(uint32_t now) {
  const uint32_t remaining =
      (DEBUG_DURATION_MS - (now - debugStartedAt)) / 1000 + 1;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("= MPU DEBUG ="));

  dbgField(0,  10, F("ax"),  sample.ax, 1);
  dbgField(64, 10, F("ay"),  sample.ay, 1);
  dbgField(0,  19, F("az"),  sample.az, 1);
  dbgField(64, 19, F("|d|"), sample.dynAccel, 1);
  dbgField(0,  28, F("gx"),  sample.gx, 2);
  dbgField(64, 28, F("gy"),  sample.gy, 2);
  dbgField(0,  37, F("gz"),  sample.gz, 2);
  dbgField(64, 37, F("T"),   sample.tempC, 1);

  display.setCursor(0, 47);
  display.print(F("pos: "));
  display.print(tiltLabel(tilt));

  display.setCursor(0, 56);
  display.print(F("saindo em "));
  display.print(remaining);
  display.print(F("s"));

  display.display();
}

void dbgField(int16_t x, int16_t y, const __FlashStringHelper *label,
              float value, uint8_t decimals) {
  display.setCursor(x, y);
  display.print(label);
  display.print(' ');
  display.print(value, decimals);
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

// ============================================================================
//  DRIVER MPU + DIAGNOSTICO I2C
// ============================================================================
void mpuWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Detecta o MPU em 0x68/0x69, acorda e configura escalas/filtro.
bool mpuInit() {
  int who = readWhoAmI(0x68);
  if (who != -1) {
    mpuAddr = 0x68;
  } else {
    who = readWhoAmI(0x69);
    if (who == -1) return false;
    mpuAddr = 0x69;
  }
  mpuWhoAmI = who;

  mpuWriteReg(MPU_REG_PWR_MGMT_1, 0x00);  // sai do sleep
  delay(10);
  mpuWriteReg(MPU_REG_SMPLRT_DIV, 0x00);  // 1 kHz
  mpuWriteReg(MPU_REG_CONFIG,     0x03);  // DLPF ~44 Hz
  mpuWriteReg(MPU_REG_GYRO_CFG,   0x08);  // +-500 dps
  mpuWriteReg(MPU_REG_ACCEL_CFG,  0x10);  // +-8 g
  return true;
}

int readWhoAmI(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(MPU_REG_WHOAMI);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

void scanI2C() {
  Serial.println(F("Scan I2C..."));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found++;
      Serial.print(F("  achei 0x"));
      Serial.println(addr, HEX);
    }
  }
  if (found == 0) Serial.println(F("  NADA no barramento!"));
}

// ============================================================================
//  ERRO FATAL NO DISPLAY
// ============================================================================
void showFatal(const __FlashStringHelper *msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 24);
  display.print(F("ERRO:"));
  display.setCursor(0, 36);
  display.print(msg);
  display.display();
}
