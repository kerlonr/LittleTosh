/*
  settings_store.h
  ----------------
  Configuracoes persistentes na EEPROM (flash emulada do ESP8266):
  credenciais do WiFi + horarios de sono. Sobrevivem ao reboot.

  Formato: struct com "magic" (identifica/versiona o layout) + checksum.
  Se a EEPROM estiver virgem ou corrompida, cai nos valores padrao.
*/
#pragma once

#include <Arduino.h>

struct Settings {
  uint32_t magic;            // identificacao + versao do layout
  char     ssid[33];         // SSID da rede de casa ("" = nunca configurado)
  char     pass[65];         // senha da rede
  uint8_t  sleepHour;        // hora de dormir (0-23)
  uint8_t  wakeHour;         // hora de acordar (0-23)
  uint8_t  holdMinutes;      // minutos acordado apos interacao a noite
  int16_t  utcOffsetMin;     // fuso em minutos (Brasilia = -180)
  uint8_t  checksum;         // XOR de todos os bytes anteriores
};

class SettingsStore {
 public:
  void begin();                    // carrega da EEPROM (ou padrao se invalida)
  void save();                     // grava a struct atual na EEPROM
  void resetToDefaults();          // volta ao padrao (nao grava sozinho)

  bool hasWifi() const { return data.ssid[0] != '\0'; }
  void setWifi(const char *ssid, const char *pass);

  Settings data;                   // acesso direto para leitura/ajuste
};
