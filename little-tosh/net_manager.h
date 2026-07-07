/*
  net_manager.h
  -------------
  Cuida de toda a rede, sem nunca travar o loop (os olhos continuam animando):

    Boot:  tem rede salva na EEPROM? -> tenta conectar (ate 20 s)
              conectou  -> ONLINE: sincroniza NTP, sobe o site de config
                           em http://<ip> (ou http://littletosh.local)
              falhou    -> AP_MODE: abre o AP "LittleTosh" com portal captivo
                           (conectou no AP, o site de config abre sozinho)
    Em AP: de tempos em tempos (sem ninguem conectado no AP) tenta de novo
           a rede salva; se ela voltou, fecha o AP e fica ONLINE.

    Salvar uma rede nova pelo site grava na EEPROM e reinicia o bichinho.
*/
#pragma once

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "config.h"
#include "night_clock.h"
#include "settings_store.h"

enum class NetMode : uint8_t { CONNECTING, ONLINE, AP_MODE };

class NetManager {
 public:
  NetManager(SettingsStore &store, NightClock &clock)
      : store(store), clock(clock), server(HTTP_PORT) {}

  void begin();
  void loop(uint32_t now);

  NetMode mode() const { return mod; }
  // Linha curta p/ a tela de debug: IP, "AP 192.168.4.1" ou "conectando".
  void statusLine(char *buf, size_t len) const;

 private:
  void startAP();
  void becomeOnline();
  void startServerOnce();

  // handlers HTTP
  void handleRoot();
  void handleStatus();
  void handleScan();
  void handleSaveWifi();
  void handleSaveConfig();
  void handleNotFound();

  SettingsStore &store;
  NightClock    &clock;

  ESP8266WebServer server;
  DNSServer        dns;

  NetMode  mod              = NetMode::CONNECTING;
  uint32_t connectStartedAt = 0;
  uint32_t lastRetryAt      = 0;
  uint32_t restartAt        = 0;   // != 0 -> reinicia quando chegar a hora
  bool     serverStarted    = false;
};
