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
                                    mostrando dados do MPU, hora e rede.
    - 23h (configuravel) .......... fica cansado, "pesca" e dorme (Zzz).
                                    Mexer nele acorda; depois de alguns
                                    minutos quieto, volta a dormir.

  Rede:
    - No boot tenta a rede WiFi salva na EEPROM; se nao der em 20 s, abre o
      AP "LittleTosh" com portal captivo para configurar tudo pelo navegador.
    - Conectado, pega a hora via NTP e serve o painel de configuracao em
      http://<ip-dele> (ou http://littletosh.local).

  Arquitetura (cada aba cuida de uma coisa so):
    config.h ........... todas as constantes ajustaveis
    settings_store ..... configuracoes persistentes na EEPROM
    mpu_sensor ......... driver I2C do MPU + gestos (tilt/tap/shake)
    face ............... display + RoboEyes + animacoes (inclusive sono)
    night_clock ........ hora certa via NTP
    sleep_scheduler .... maquina de estados do ciclo de sono
    net_manager ........ WiFi (STA/AP), portal captivo e site de config
    web_page.h ......... HTML do painel, embutido na flash
    debug_screen ....... tela de diagnostico da chacoalhada
    little-tosh.ino .... este arquivo: liga tudo no setup() e no loop()

  Ligacoes (I2C padrao do D1 mini):
    SDA -> D2 (GPIO4)   |   SCL -> D1 (GPIO5)
    OLED SSD1306 = 0x3C   |   MPU = 0x68 (ou 0x69 se AD0 em nivel alto)

  Bibliotecas (Library Manager): Adafruit GFX, Adafruit SSD1306,
  FluxGarage RoboEyes.  Placa: "LOLIN(WEMOS) D1 R2 & mini".
*/

#include <Wire.h>

#include "config.h"
#include "debug_screen.h"
#include "face.h"
#include "mpu_sensor.h"
#include "net_manager.h"
#include "night_clock.h"
#include "settings_store.h"
#include "sleep_scheduler.h"

// ============================================================================
//  MODULOS
// ============================================================================
SettingsStore  settings;
Face           face;
MpuSensor      mpu;
NightClock     nightClock;
NetManager     net(settings, nightClock);
SleepScheduler sleeper(face, nightClock, settings);
DebugScreen    debugScreen(face.raw(), mpu, net, nightClock);

// ============================================================================
//  ESTADO DO COMPORTAMENTO NORMAL
// ============================================================================
Tilt tilt     = Tilt::FLAT;
Tilt prevTilt = Tilt::FLAT;

uint32_t tiltSince         = 0;
uint32_t reactionStartedAt = 0;
bool     reacting          = false;

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(F("=== LittleTosh boot ==="));

  settings.begin();  // carrega WiFi + horarios de sono da EEPROM

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

  if (!face.begin()) {
    Serial.println(F("[ERRO] OLED nao iniciou."));
    for (;;) delay(1000);
  }
  Serial.println(F("[ok] OLED iniciado."));

  scanI2C();  // varre o barramento (e estabiliza o MPU antes da deteccao)

  if (!mpu.begin()) {
    Serial.println(F("[ERRO] MPU nao encontrado."));
    face.showFatal(F("MPU nao encontrado"));
    for (;;) delay(1000);
  }
  Serial.print(F("[ok] MPU em 0x"));
  Serial.print(mpu.address(), HEX);
  Serial.print(F(" WHO_AM_I=0x"));
  Serial.println(mpu.whoAmI(), HEX);

  Wire.setClock(I2C_CLOCK_HZ);

  face.beginEyes();

  net.begin();  // nao bloqueia: os olhos ja animam enquanto o WiFi conecta

  Serial.println(F("[ok] LittleTosh pronto."));
}

// ============================================================================
//  LOOP
// ============================================================================
void loop() {
  const uint32_t now = millis();

  net.loop(now);  // WiFi / portal / site de config (nunca bloqueia)

  const bool freshSample = mpu.read(now);

  // --- modo debug tem prioridade total sobre a tela ---
  if (debugScreen.active()) {
    debugScreen.run(now, tilt);
    if (!debugScreen.active()) exitDebug(now);
    return;
  }

  if (freshSample) tilt = mpu.classifyTilt();

  // --- ciclo de sono ---
  const bool wasResting = sleeper.resting();
  sleeper.update(now, freshSample, mpu.sample(), tilt);

  if (sleeper.resting()) {
    // dormindo/cansado/acordando: o rosto pertence ao ciclo de sono
    // (a interacao pelo acelerometro e tratada dentro do scheduler)
    reacting = false;
    face.update();
    return;
  }
  if (wasResting) {
    // acabou de acordar: re-sincroniza o comportamento normal
    prevTilt  = Tilt::FLAT;
    tiltSince = now;
    mpu.resetShake();
  }

  // --- comportamento normal (identico ao original) ---
  if (freshSample) {
    if (mpu.detectShake(now)) {
      enterDebug(now);
      return;
    }
    if (!reacting && mpu.detectTap(now)) {
      triggerLaugh(now);
    }
  }

  if (reacting && (now - reactionStartedAt >= REACTION_MS)) {
    endReaction(now);
  }

  if (!reacting && tilt != prevTilt) {
    face.lookTilt(tilt);
    prevTilt  = tilt;
    tiltSince = now;
  }

  updateIdle(now);
  face.update();
}

// ============================================================================
//  REACOES DO COMPORTAMENTO NORMAL
// ============================================================================
void triggerLaugh(uint32_t now) {
  face.laugh();
  reacting          = true;
  reactionStartedAt = now;
  tiltSince         = now;
}

void endReaction(uint32_t now) {
  face.calmDown();
  reacting  = false;
  prevTilt  = Tilt::FLAT;
  tiltSince = now;
}

void updateIdle(uint32_t now) {
  const bool shouldIdle = (tilt == Tilt::FLAT) && !reacting &&
                          (now - tiltSince >= IDLE_DELAY_MS);
  face.setIdle(shouldIdle);
}

// ============================================================================
//  ENTRADA/SAIDA DO MODO DEBUG
// ============================================================================
void enterDebug(uint32_t now) {
  reacting = false;
  face.setIdle(false);
  debugScreen.enter(now);
}

void exitDebug(uint32_t now) {
  prevTilt  = Tilt::FLAT;
  tiltSince = now;
  mpu.resetShake();
  face.clear();
}

// ============================================================================
//  DIAGNOSTICO I2C
// ============================================================================
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
