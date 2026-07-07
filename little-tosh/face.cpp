#include "face.h"

#include <Wire.h>

// O core do ESP8266 define DEFAULT == 1; a RoboEyes precisa de DEFAULT == 0.
// (a lib so entra aqui — veja o comentario no face.h)
#ifdef DEFAULT
#undef DEFAULT
#endif
#include <FluxGarage_RoboEyes.h>

Face::Face()
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
      eyes(new RoboEyes<Adafruit_SSD1306>(display)) {}

bool Face::begin() {
  return display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
}

void Face::beginEyes() {
  eyes->begin(SCREEN_WIDTH, SCREEN_HEIGHT, TARGET_FPS);
  eyes->setWidth(EYE_WIDTH, EYE_WIDTH);
  eyes->setHeight(EYE_HEIGHT, EYE_HEIGHT);
  eyes->setBorderradius(EYE_BORDER_RADIUS, EYE_BORDER_RADIUS);
  eyes->setSpacebetween(EYE_SPACING);
  eyes->setAutoblinker(ON, 3, 2);
}

// ============================================================================
//  FRAME
// ============================================================================
void Face::update() {
  const uint32_t now = millis();

  if (state == State::CANSADO) {
    updateTired(now);
  } else if (state == State::ACORDANDO && now - stateSince >= WAKING_DURATION_MS) {
    backToNormal();
  }

  // Dormindo o desenho e por nossa conta (senao a RoboEyes reenviaria o
  // buffer sem o Zzz a cada frame e a tela ficaria piscando).
  if (state == State::DORMINDO) {
    updateSleep(now);
    return;
  }

  eyes->update();
}

// ============================================================================
//  COMPORTAMENTO NORMAL (identico ao visual original)
// ============================================================================
void Face::lookTilt(Tilt t) {
  uint8_t position = DEFAULT;
  switch (t) {
    case Tilt::LEFT:  position = W; break;
    case Tilt::RIGHT: position = E; break;
    case Tilt::UP:    position = N; break;
    case Tilt::DOWN:  position = S; break;
    case Tilt::FLAT:
    default:
      eyes->setPosition(DEFAULT);
      eyes->setCuriosity(OFF);
      eyes->setMood(DEFAULT);
      return;
  }
  eyes->setPosition(position);
  eyes->setCuriosity(ON);
  eyes->setAutoblinker(ON, 3, 5);
}

void Face::laugh() {
  eyes->setMood(HAPPY);
  eyes->setCuriosity(ON);
  eyes->anim_laugh();
  setIdle(false);
}

void Face::calmDown() {
  eyes->setMood(DEFAULT);
  eyes->setCuriosity(OFF);
  eyes->setAutoblinker(ON, 3, 2);
}

void Face::setIdle(bool on) {
  if (on == idleActive) return;
  eyes->setIdleMode(on ? ON : OFF, 2, 2);
  idleActive = on;
}

// ============================================================================
//  CICLO DE SONO
// ============================================================================
void Face::playTired() {
  state      = State::CANSADO;
  stateSince = millis();
  tiredPhase = 0;

  setIdle(false);
  eyes->setCuriosity(OFF);
  eyes->setPosition(DEFAULT);
  eyes->setMood(TIRED);              // palpebras de sono da propria lib
  eyes->setAutoblinker(ON, 2, 2);    // piscadas calmas, de sono
}

// Fases do cansaco (tempos relativos ao inicio da animacao):
//   0-3 s .... so as palpebras caidas (mood TIRED) e piscadas calmas
//   3-6 s .... olhos murcham para ~60% da altura
//   6 s+ ..... murcham para ~35%, olhar cai (S) e comecam as "pescadas":
//              a altura alterna devagar entre ~35% e ~20% — a propria lib
//              interpola a mudanca, entao o movimento e suave, sem tremer.
//              (nada de setVFlicker aqui: o flicker da lib alterna a posicao
//              A CADA frame, 60 Hz — e tremida de susto, nao de sono)
void Face::updateTired(uint32_t now) {
  const uint32_t elapsed = now - stateSince;

  if (tiredPhase == 0 && elapsed >= 3000) {
    tiredPhase = 1;
    eyes->setHeight((EYE_HEIGHT * 6) / 10, (EYE_HEIGHT * 6) / 10);
  } else if (tiredPhase == 1 && elapsed >= 6000) {
    tiredPhase = 2;
    eyes->setHeight((EYE_HEIGHT * 35) / 100, (EYE_HEIGHT * 35) / 100);
    eyes->setPosition(S);
    eyes->setAutoblinker(OFF);
    nodAt   = now;
    nodDown = false;
  } else if (tiredPhase == 2 && now - nodAt >= 1400) {
    nodAt   = now;
    nodDown = !nodDown;
    const uint8_t h = nodDown ? (EYE_HEIGHT * 20) / 100
                              : (EYE_HEIGHT * 35) / 100;
    eyes->setHeight(h, h);
  }
}

void Face::playSleep() {
  state       = State::DORMINDO;
  stateSince  = millis();
  sleepStatic = false;
  closedSince = 0;
  sleepKey    = 0xFF;

  eyes->setAutoblinker(OFF);
  setIdle(false);
  eyes->setPosition(DEFAULT);
  eyes->close();                     // palpebras fecham (a lib anima a descida)
}

// Enquanto as palpebras descem, a RoboEyes segue desenhando (transicao
// suave). Quando os olhos chegam na altura minima e assentam (~meio
// segundo), congelamos a geometria REAL desenhada pela lib e passamos a
// desenhar o frame inteiro por conta propria: olhos fechados identicos +
// Zzz + respiracao, tudo num envio so — sem piscar.
void Face::updateSleep(uint32_t now) {
  if (!sleepStatic) {
    eyes->update();
    if (eyes->eyeLheightCurrent <= 1 && eyes->eyeRheightCurrent <= 1) {
      if (closedSince == 0) {
        closedSince = now;
      } else if (now - closedSince >= 500) {  // espera a posicao assentar
        captureClosedEyes();
        sleepStatic = true;
      }
    } else {
      closedSince = 0;
    }
    return;
  }
  drawSleepFrame(now);
}

void Face::captureClosedEyes() {
  closedEyes.lx = eyes->eyeLx;
  closedEyes.ly = eyes->eyeLy;
  closedEyes.rx = eyes->eyeRx;
  closedEyes.ry = eyes->eyeRy;
  closedEyes.w  = eyes->eyeLwidthCurrent;
  closedEyes.h  = 1;  // altura de olho fechado da propria lib
}

void Face::playWake(bool happyMorning) {
  state       = State::ACORDANDO;
  stateSince  = millis();
  sleepStatic = false;

  eyes->setHeight(EYE_HEIGHT, EYE_HEIGHT);   // desfaz o murchado do cansaco
  eyes->setPosition(DEFAULT);
  eyes->open();

  if (happyMorning) {
    eyes->setMood(HAPPY);            // bom dia :)
  } else {
    eyes->setMood(DEFAULT);
    eyes->anim_confused();           // sustinho de quem foi acordado
  }
}

void Face::backToNormal() {
  state = State::NORMAL;
  eyes->setMood(DEFAULT);
  eyes->setCuriosity(OFF);
  eyes->setAutoblinker(ON, 3, 2);
}

// Frame completo do sono: olhos fechados (na posicao exata em que a lib os
// deixou) + "Zzz" subindo em passos lentos + respiracao de 1 px. So manda
// um frame novo ao OLED quando algum desses elementos muda (~1x por
// segundo) — cada frame ja sai inteiro, entao nada pisca.
void Face::drawSleepFrame(uint32_t now) {
  const uint32_t elapsed = now - stateSince;
  const uint8_t  zzzStep = (elapsed / 900) % 4;   // 0..3: nada, z, zz, zzZ
  const uint8_t  breath  = (elapsed / 1700) % 2;  // olhos sobem/descem 1 px

  const uint8_t key = zzzStep * 2 + breath;
  if (key == sleepKey) return;  // nada mudou, nao reenvia o buffer
  sleepKey = key;

  display.clearDisplay();

  // olhos fechados (linhas), respirando devagarinho
  display.fillRect(closedEyes.lx, closedEyes.ly + breath, closedEyes.w,
                   closedEyes.h, SSD1306_WHITE);
  display.fillRect(closedEyes.rx, closedEyes.ry + breath, closedEyes.w,
                   closedEyes.h, SSD1306_WHITE);

  // os "z" nascem pequenos perto dos olhos e sobem crescendo
  display.setTextColor(SSD1306_WHITE);
  if (zzzStep >= 1) {
    display.setTextSize(1);
    display.setCursor(92, 20);
    display.print('z');
  }
  if (zzzStep >= 2) {
    display.setTextSize(1);
    display.setCursor(102, 11);
    display.print('z');
  }
  if (zzzStep >= 3) {
    display.setTextSize(2);
    display.setCursor(112, 0);
    display.print('Z');
  }

  display.display();
}

// ============================================================================
//  UTILIDADES
// ============================================================================
void Face::clear() {
  display.clearDisplay();
  display.display();
}

void Face::showFatal(const __FlashStringHelper *msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 24);
  display.print(F("ERRO:"));
  display.setCursor(0, 36);
  display.print(msg);
  display.display();
}
