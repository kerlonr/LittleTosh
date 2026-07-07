/*
  sleep_scheduler.h
  -----------------
  Maquina de estados do ciclo de sono:

      AWAKE ──(deu a hora de dormir / acabou o tempo acordado)──> TIRED
      TIRED ──(fim da animacao de cansado)──────────────────────> SLEEPING
      TIRED/SLEEPING ──(interacao no acelerometro)──────────────> WAKING
      SLEEPING ──(chegou a hora de acordar)─────────────────────> WAKING (feliz)
      WAKING ──(fim da animacao)────────────────────────────────> AWAKE

  Durante a noite, apos acordar ele fica "holdMinutes" acordado; qualquer
  interacao (tapinha, pegar na mao, inclinar) renova esse tempo. Sem hora
  sincronizada (nunca conectou), o sono fica desativado.
*/
#pragma once

#include <Arduino.h>

#include "face.h"
#include "mpu_sensor.h"
#include "night_clock.h"
#include "settings_store.h"

enum class SleepState : uint8_t { AWAKE, TIRED, SLEEPING, WAKING };

class SleepScheduler {
 public:
  SleepScheduler(Face &face, NightClock &clock, SettingsStore &store)
      : face(face), clock(clock), store(store) {}

  void update(uint32_t now, bool freshSample, const MpuSample &smp, Tilt tilt);

  // Enquanto "descansando" o loop principal nao roda os comportamentos
  // normais (tilt/risada/debug) — o rosto pertence ao ciclo de sono.
  bool resting() const { return state != SleepState::AWAKE; }
  SleepState current() const { return state; }

 private:
  bool isNight(int h) const;
  bool isInteraction(bool freshSample, const MpuSample &smp, Tilt tilt) const;
  void enter(SleepState s, uint32_t now);

  Face          &face;
  NightClock    &clock;
  SettingsStore &store;

  SleepState state       = SleepState::AWAKE;
  uint32_t   stateSince  = 0;
  uint32_t   awakeSince  = 0;      // p/ contar os minutos acordado a noite
  bool       wasNight    = false;
};
