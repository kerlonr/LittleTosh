#include "sleep_scheduler.h"

#include "config.h"

bool SleepScheduler::isNight(int h) const {
  if (h < 0) return false;  // sem hora sincronizada
  const uint8_t sleepH = store.data.sleepHour;
  const uint8_t wakeH  = store.data.wakeHour;
  if (sleepH == wakeH) return false;         // janela nula = nunca dorme
  if (sleepH < wakeH) return h >= sleepH && h < wakeH;   // ex: 1h -> 6h
  return h >= sleepH || h < wakeH;                       // ex: 23h -> 7h
}

// Qualquer mexida conta: solavanco (dynAccel) ou sair do nivelado.
bool SleepScheduler::isInteraction(bool freshSample, const MpuSample &smp,
                                   Tilt tilt) const {
  if (!freshSample) return false;
  return smp.dynAccel >= WAKE_MOTION_THRESHOLD || tilt != Tilt::FLAT;
}

void SleepScheduler::enter(SleepState s, uint32_t now) {
  state      = s;
  stateSince = now;
}

void SleepScheduler::update(uint32_t now, bool freshSample,
                            const MpuSample &smp, Tilt tilt) {
  const bool night       = isNight(clock.hour());
  const bool interaction = isInteraction(freshSample, smp, tilt);

  switch (state) {
    case SleepState::AWAKE:
      if (interaction) awakeSince = now;  // carinho renova o tempo acordado

      if (night && !wasNight) {
        // acabou de dar a hora de dormir
        enter(SleepState::TIRED, now);
        face.playTired();
        Serial.println(F("[sono] deu a hora... ficando cansado."));
      } else if (night && (now - awakeSince >=
                           store.data.holdMinutes * 60000UL)) {
        // ja ficou acordado tempo suficiente apos a ultima interacao
        enter(SleepState::TIRED, now);
        face.playTired();
        Serial.println(F("[sono] cansou de novo..."));
      }
      break;

    case SleepState::TIRED:
      if (interaction) {
        enter(SleepState::WAKING, now);
        face.playWake(false);
        Serial.println(F("[sono] opa, mexeram em mim!"));
      } else if (now - stateSince >= TIRED_DURATION_MS) {
        enter(SleepState::SLEEPING, now);
        face.playSleep();
        Serial.println(F("[sono] zzz..."));
      }
      break;

    case SleepState::SLEEPING:
      if (!night) {
        enter(SleepState::WAKING, now);
        face.playWake(true);  // amanheceu: acorda feliz
        Serial.println(F("[sono] bom dia!"));
      } else if (interaction) {
        enter(SleepState::WAKING, now);
        face.playWake(false);
        Serial.println(F("[sono] acordei! quem foi?"));
      }
      break;

    case SleepState::WAKING:
      if (now - stateSince >= WAKING_DURATION_MS) {
        enter(SleepState::AWAKE, now);
        awakeSince = now;
      }
      break;
  }

  wasNight = night;
}
