/*
  LittleTosh
*/

#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <FluxGarage_RoboEyes.h>
roboEyes roboEyes;

#include <Wire.h>
#include "SparkFun_MMA8452Q.h"
MMA8452Q accel;

// --- CONTROLES DE ESTADO ---
typedef enum {
  UP,         // Estados de Posição
  DOWN,
  LEFT,
  RIGHT,
  FLAT,
  TAP,        // Estados de animação
  SHAKING,
  UNKNOWN     // Quando não consegue ler / Ao Inicializar
} State;

State state = UNKNOWN;
State Pstate = UNKNOWN;
// --------------------------------------------------------------


// --- CONTROLES DE TEMPO ---
const unsigned long IDLE_DELAY_MS = 2000;    // Tempo parado para entrar em idle
const unsigned long TAP_DEBOUNCE_MS = 400;   // Debounce entre os taps
const unsigned long ANIM_DURATION_MS = 1200; // Tempo para reodar as animações

unsigned long stateSince = 0;         // quando entrou no estado atual
unsigned long lastTapAt = 0;          // debounce do tap
unsigned long animationStartedAt = 0;  // quando começou a risada
bool animating = false;               // Trava para evitar animações consecutivas
int consecutivePosVariation = 0;      // Pega a quantidade de variações seguidas.
// --------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Inicia o acelerômetro MMA8452Q
  if (accel.begin() == false) {
    Serial.println("MMA8452Q não está conectado. Cheque as conexões.");
    for(;;);
  }

  // Inicia o Display OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Inicialização do SSD1306 falhou."));
    for(;;);
  }

  // Inicia os olhos robô
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); // screen-width, screen-height, max framerate

  // Define o formato dos olhos, todos valores em pixels
  roboEyes.setWidth(30, 30);      // byte leftEye, byte rightEye
  roboEyes.setHeight(30, 30);     // byte leftEye, byte rightEye
  roboEyes.setBorderradius(8, 8); // byte leftEye, byte rightEye
  roboEyes.setSpacebetween(10);   // espaço entre os olhos
}

void loop() {
  unsigned long now = millis();

  // Atualiza os olhos e pega estado do sensor
  roboEyes.update();
  state = getActualState();

  if (shouldDoTapAnimation(now)) {
    tapAnimation(now);
  }

  // Controla tempo e estado para apenas parar animações quando concluidas
  if (shouldStopAnimation(now)) {
    stopAnimation();
  }

  // Controla o que aparece no display
  if (shouldUpdateDisplay()) {
    updateDisplay();
  }

  // Ativa/Desativa a animação do IDLE
  if (shouldEnterIdle(now)) {
    roboEyes.setIdleMode(ON, 2, 2);
  } else roboEyes.setIdleMode(OFF);
}

// --- LEITURA DOS SENSORES ---

// Pega o estado atual do Sensor e devolve como um Enum
State getActualState() {
  if (!accel.available()) return UNKNOWN;

  if (accel.isRight()) return DOWN;
  if (accel.isLeft())  return UP;
  if (accel.isUp())    return LEFT;
  if (accel.isDown())  return RIGHT;
  if (accel.isFlat())  return FLAT;
  
  return UNKNOWN;
}

bool shouldDoTapAnimation(unsigned long actual_time) {
  if (animating) return false;
  if (accel.readTap() <= 0) return false;
  return (actual_time - lastTapAt) > TAP_DEBOUNCE_MS;
}

// --------------------------------------------------------------


// --- CONTROLES DE ANIMAÇÕES ---

// Retorna um boolean para saber quando deve entrar em IDLE
bool shouldEnterIdle(const unsigned long actual_time) {
  return state == FLAT && !animating && (actual_time - stateSince >= IDLE_DELAY_MS);
}

// Retorna um boolean para saber quando deve desativar a animação atual.
bool shouldStopAnimation(const unsigned long actual_time) {
 return animating && (actual_time - animationStartedAt) >= ANIM_DURATION_MS;
}

// Para a animação atual e volta para o mood DEFAULT
void stopAnimation() {
  roboEyes.setCuriosity(OFF);
  roboEyes.setMood(DEFAULT);
  roboEyes.setAutoblinker(ON, 3, 2);
  
  animating = false;
  stateSince = millis(); // reinicia timer do estado atual
}

// --------------------------------------------------------------


// --- CONTROLES DOS ESTADOS ---

bool shouldUpdateDisplay() {
  return (state != Pstate) && !animating;
}

void updateDisplay() {
  switch (state) {
    case LEFT:
      Serial.println("Esquerda");
      setInclinedConfigs(W); // e
      break;
    case RIGHT:
      Serial.println("Direita");
      setInclinedConfigs(E); // w
      break;
    case UP:
      Serial.println("Cima");
      setInclinedConfigs(S); // s
      break;
    case DOWN:
      Serial.println("Abaixo");
      setInclinedConfigs(N); // n
      break;
    case FLAT:
      Serial.println("Flat");
      roboEyes.setPosition(DEFAULT);
      roboEyes.setMood(DEFAULT);
      roboEyes.setCuriosity(OFF);
      break;
    default:
      Serial.println("Estado desconhecido :(");
      break;
  }

  stateSince = millis();
  Pstate = state;
}

void tapAnimation(unsigned long actual_time) {
  lastTapAt = actual_time;

  // Ação do TAP
  roboEyes.anim_laugh();
  roboEyes.setCuriosity(ON);
  roboEyes.setMood(HAPPY);
  animating = true;
  animationStartedAt = actual_time;

  // Garante que não apareça o idle;
  stateSince = actual_time;
}


void setInclinedConfigs(const int eyePosition) {
  roboEyes.setPosition(eyePosition);    // cardinal directions, can be N, NE, E, SE, S, SW, W, NW
  roboEyes.setAutoblinker(ON, 3, 5);    // bool active, int interval, int variation
  roboEyes.setCuriosity(ON);
}

// --------------------------------------------------------------

// Set horizontal or vertical flickering
//roboEyes.setHFlicker(ON, 2); // bool on/off, byte amplitude -> horizontal flicker: alternately displacing the eyes in the defined amplitude in pixels
//roboEyes.setVFlicker(ON, 2); // bool on/off, byte amplitude -> vertical flicker: alternately displacing the eyes in the defined amplitude in pixels
