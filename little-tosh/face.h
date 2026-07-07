/*
  face.h
  ------
  Tudo que aparece na tela mora aqui: o display OLED + os olhos RoboEyes.
  O resto do codigo nunca fala com a RoboEyes direto — pede ao Face
  ("olhe pra esquerda", "ria", "durma") e o Face traduz para a lib.

  Animacoes extras (a RoboEyes nao tem sono, entao foram compostas com o
  que ela ja desenha, mantendo a MESMA estetica):
    - cansado ... mood TIRED + palpebras caindo + olhos "pescando" (VFlicker)
    - dormindo .. olhos fechados + "Zzz" flutuando + fps reduzido
    - acordar ... olhos abrem + sustinho (anim_confused) ou feliz de manha
*/
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "mpu_sensor.h"  // enum Tilt

// A FluxGarage_RoboEyes.h define macros de uma letra (N, S, E, W...) e nomes
// como TIRED/DEFAULT que conflitam com o resto do codigo. Por isso a lib so
// e incluida DENTRO do face.cpp; aqui basta a declaracao do template.
template <typename AdafruitDisplay> class RoboEyes;

class Face {
 public:
  Face();

  bool begin();            // inicia o OLED (false se nao achou) — chama antes do resto
  void beginEyes();        // configura a RoboEyes (depois do diagnostico I2C)
  void update();           // desenha o frame (chamar todo loop)

  // --- comportamento normal (mesmo visual de antes) ---
  void lookTilt(Tilt t);   // olhos "caem" para o lado da inclinacao
  void laugh();            // animacao de risada (tapinha)
  void calmDown();         // volta ao neutro apos a risada
  void setIdle(bool on);   // modo "idle" (olhar vagando sozinho)

  // --- ciclo de sono ---
  void playTired();        // comeca a animacao de cansado
  void playSleep();        // dorme (olhos fechados + Zzz)
  void playWake(bool happyMorning);  // acorda (sustinho, ou feliz de manha)
  bool isAsleep() const { return state == State::DORMINDO; }

  // --- utilidades ---
  void clear();                                // limpa a tela (ao sair do debug)
  void showFatal(const __FlashStringHelper *msg);
  Adafruit_SSD1306 &raw() { return display; } // acesso cru p/ a tela de debug

 private:
  enum class State : uint8_t { NORMAL, CANSADO, DORMINDO, ACORDANDO };

  void updateTired(uint32_t now);
  void updateSleep(uint32_t now);
  void captureClosedEyes();
  void drawSleepFrame(uint32_t now);
  void backToNormal();

  Adafruit_SSD1306 display;
  RoboEyes<Adafruit_SSD1306> *eyes;  // alocado no construtor (face.cpp)

  State    state       = State::NORMAL;
  uint32_t stateSince  = 0;
  uint8_t  tiredPhase  = 0;
  bool     idleActive  = false;

  // cansado: "pescadas" (olhos afundam e voltam devagar)
  uint32_t nodAt   = 0;
  bool     nodDown = false;

  // dormindo: depois que os olhos fecham, o Face assume o desenho do frame
  // (a RoboEyes reenviaria o buffer sem o Zzz e a tela piscaria)
  bool     sleepStatic  = false;  // ja assumiu o desenho?
  uint32_t closedSince  = 0;      // quando as palpebras terminaram de descer
  uint8_t  sleepKey     = 0xFF;   // "assinatura" do ultimo frame desenhado
  struct { int16_t lx, ly, rx, ry, w, h; } closedEyes = {0, 0, 0, 0, 0, 0};
};
