/*
  config.h
  --------
  Todas as constantes ajustaveis do LittleTosh num lugar so.
  Mexeu no hardware, nos tempos ou nos limiares? E aqui.
*/
#pragma once

#include <Arduino.h>

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
//  REDE (AP de configuracao + conexao na rede de casa)
// ============================================================================
constexpr char     AP_SSID[]          = "LittleTosh";   // nome do AP de config
constexpr char     AP_PASS[]          = "";             // AP aberto ("" = sem senha)
constexpr char     MDNS_NAME[]        = "littletosh";   // http://littletosh.local
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;     // espera no boot antes de abrir o AP
constexpr uint32_t WIFI_RETRY_FROM_AP_MS   = 300000;    // em AP, tenta a rede salva a cada 5 min
constexpr uint16_t HTTP_PORT          = 80;
constexpr uint16_t DNS_PORT           = 53;

// Servidores NTP (o primeiro e brasileiro, responde mais rapido daqui).
constexpr char NTP_SERVER_1[] = "a.st1.ntp.br";
constexpr char NTP_SERVER_2[] = "pool.ntp.org";

// ============================================================================
//  SONO (valores PADRAO — os efetivos ficam salvos na EEPROM e
//  podem ser mudados pelo site de configuracao)
// ============================================================================
constexpr uint8_t  DEFAULT_SLEEP_HOUR    = 23;   // hora de dormir
constexpr uint8_t  DEFAULT_WAKE_HOUR     = 7;    // hora de acordar
constexpr uint8_t  DEFAULT_HOLD_MINUTES  = 3;    // minutos acordado apos interacao a noite
constexpr int16_t  DEFAULT_UTC_OFFSET_MIN = -180; // Brasilia = UTC-3

constexpr uint32_t TIRED_DURATION_MS  = 9000;  // duracao da animacao de cansado
constexpr uint32_t WAKING_DURATION_MS = 2200;  // duracao da animacao de acordar
constexpr float    WAKE_MOTION_THRESHOLD = 3.0f; // m/s^2 dinamico p/ acordar (tapinha leve)
