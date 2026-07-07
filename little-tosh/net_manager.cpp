#include "net_manager.h"

#include <ESP8266mDNS.h>

#include "config.h"
#include "web_page.h"

// ============================================================================
//  CICLO DE VIDA
// ============================================================================
void NetManager::begin() {
  // As credenciais vivem na NOSSA EEPROM; nao deixa o SDK gravar copia na
  // flash dele (evita desgaste e estado fantasma de redes antigas).
  WiFi.persistent(false);
  WiFi.hostname(MDNS_NAME);

  // O NTP ja fica configurado desde o boot; ele sincroniza sozinho
  // assim que existir internet.
  clock.begin(store.data.utcOffsetMin);

  if (store.hasWifi()) {
    Serial.print(F("[wifi] tentando rede salva: "));
    Serial.println(store.data.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(store.data.ssid, store.data.pass);
    mod              = NetMode::CONNECTING;
    connectStartedAt = millis();
  } else {
    Serial.println(F("[wifi] nenhuma rede salva."));
    startAP();
  }
}

void NetManager::loop(uint32_t now) {
  // Reinicio agendado (apos salvar rede nova): da tempo do navegador
  // receber a resposta antes do chip cair.
  if (restartAt != 0 && (int32_t)(now - restartAt) >= 0) {
    ESP.restart();
  }

  switch (mod) {
    case NetMode::CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        becomeOnline();
      } else if (now - connectStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println(F("[wifi] nao conectou -> abrindo AP."));
        startAP();
      }
      break;

    case NetMode::ONLINE:
      server.handleClient();
      MDNS.update();
      break;

    case NetMode::AP_MODE:
      dns.processNextRequest();
      server.handleClient();

      // A rede salva pode ter voltado (queda de luz, roteador reiniciou...).
      // Sem ninguem mexendo no AP, tenta de novo de tempos em tempos.
      if (store.hasWifi() && WiFi.softAPgetStationNum() == 0 &&
          now - lastRetryAt >= WIFI_RETRY_FROM_AP_MS) {
        lastRetryAt = now;
        Serial.println(F("[wifi] AP ocioso, tentando a rede salva de novo..."));
        WiFi.begin(store.data.ssid, store.data.pass);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("[wifi] rede salva voltou! fechando o AP."));
        dns.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        becomeOnline();
      }
      break;
  }
}

void NetManager::startAP() {
  // AP_STA (e nao AP puro) para continuar podendo escanear redes e
  // re-tentar a rede salva com o AP no ar.
  WiFi.mode(WIFI_AP_STA);
  if (strlen(AP_PASS) >= 8) {
    WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    WiFi.softAP(AP_SSID);  // AP aberto
  }

  const IPAddress apIP = WiFi.softAPIP();
  // DNS coringa: qualquer site digitado leva ao painel (portal captivo).
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(DNS_PORT, "*", apIP);

  startServerOnce();
  mod         = NetMode::AP_MODE;
  lastRetryAt = millis();

  Serial.print(F("[wifi] AP \""));
  Serial.print(AP_SSID);
  Serial.print(F("\" no ar. painel: http://"));
  Serial.println(apIP);
}

void NetManager::becomeOnline() {
  mod = NetMode::ONLINE;
  startServerOnce();

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
  }

  Serial.print(F("[wifi] conectado! painel: http://"));
  Serial.print(WiFi.localIP());
  Serial.print(F("  (ou http://"));
  Serial.print(MDNS_NAME);
  Serial.println(F(".local)"));
}

void NetManager::startServerOnce() {
  if (serverStarted) return;
  serverStarted = true;

  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server.on("/scan", HTTP_GET, [this]() { handleScan(); });
  server.on("/wifi", HTTP_POST, [this]() { handleSaveWifi(); });
  server.on("/config", HTTP_POST, [this]() { handleSaveConfig(); });
  server.onNotFound([this]() { handleNotFound(); });
  server.begin();
}

// ============================================================================
//  HANDLERS HTTP
// ============================================================================
void NetManager::handleRoot() {
  server.send_P(200, "text/html", WEB_PAGE);
}

// Escapa aspas e barras p/ montar JSON na mao (sem lib externa).
static String jsonEscape(const char *s) {
  String out;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if ((uint8_t)*s >= 0x20) out += *s;
  }
  return out;
}

void NetManager::handleStatus() {
  char timeBuf[8];
  clock.hhmm(timeBuf, sizeof(timeBuf));

  String json = "{";
  switch (mod) {
    case NetMode::ONLINE:     json += F("\"mode\":\"conectado\""); break;
    case NetMode::AP_MODE:    json += F("\"mode\":\"AP de configuracao\""); break;
    case NetMode::CONNECTING: json += F("\"mode\":\"conectando...\""); break;
  }
  json += F(",\"ssid\":\"");
  json += (mod == NetMode::ONLINE) ? jsonEscape(WiFi.SSID().c_str()) : "";
  json += F("\",\"savedSsid\":\"");
  json += jsonEscape(store.data.ssid);
  json += F("\",\"ip\":\"");
  json += (mod == NetMode::AP_MODE) ? WiFi.softAPIP().toString()
                                    : WiFi.localIP().toString();
  json += F("\",\"rssi\":");
  json += (mod == NetMode::ONLINE) ? String(WiFi.RSSI()) : "0";
  json += F(",\"time\":\"");
  json += timeBuf;
  json += F("\",\"sleepHour\":");
  json += store.data.sleepHour;
  json += F(",\"wakeHour\":");
  json += store.data.wakeHour;
  json += F(",\"holdMinutes\":");
  json += store.data.holdMinutes;
  json += F(",\"utcOffsetMin\":");
  json += store.data.utcOffsetMin;
  json += '}';
  server.send(200, "application/json", json);
}

// Scan assincrono: a 1a chamada dispara a varredura e responde "scanning";
// o JS da pagina fica perguntando ate a lista sair. Assim os olhos nao
// congelam enquanto o radio varre os canais.
void NetManager::handleScan() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(/*async=*/true);
    server.send(200, "application/json", F("{\"status\":\"scanning\"}"));
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", F("{\"status\":\"scanning\"}"));
    return;
  }

  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i) json += ',';
    json += F("{\"ssid\":\"");
    json += jsonEscape(WiFi.SSID(i).c_str());
    json += F("\",\"rssi\":");
    json += WiFi.RSSI(i);
    json += F(",\"sec\":");
    json += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F("false") : F("true");
    json += '}';
  }
  json += ']';
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void NetManager::handleSaveWifi() {
  if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
    server.send(400, "text/plain", F("faltou o ssid"));
    return;
  }
  store.setWifi(server.arg("ssid").c_str(), server.arg("pass").c_str());
  store.save();

  server.send_P(200, "text/html", WEB_PAGE_REBOOT);
  restartAt = millis() + 1500;  // reinicia depois que a resposta sair
  Serial.println(F("[wifi] rede nova salva, reiniciando..."));
}

void NetManager::handleSaveConfig() {
  auto clampByte = [&](const String &name, long lo, long hi, uint8_t fallback) {
    if (!server.hasArg(name)) return fallback;
    long v = server.arg(name).toInt();
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (uint8_t)v;
  };

  store.data.sleepHour   = clampByte("sleepHour",   0, 23, store.data.sleepHour);
  store.data.wakeHour    = clampByte("wakeHour",    0, 23, store.data.wakeHour);
  store.data.holdMinutes = clampByte("holdMinutes", 1, 60, store.data.holdMinutes);

  if (server.hasArg("utcOffsetH")) {
    long h = server.arg("utcOffsetH").toInt();
    if (h < -12) h = -12;
    if (h > 14) h = 14;
    store.data.utcOffsetMin = (int16_t)(h * 60);
    clock.setOffset(store.data.utcOffsetMin);
  }
  store.save();

  // Volta para o painel (funciona com e sem JavaScript).
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// Portal captivo: em AP, qualquer URL desconhecida (inclusive os testes de
// conectividade do celular, tipo generate_204) redireciona para o painel.
void NetManager::handleNotFound() {
  if (mod == NetMode::AP_MODE) {
    server.sendHeader("Location",
                      String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", F("nao achei essa pagina"));
  }
}

// ============================================================================
//  STATUS (p/ tela de debug)
// ============================================================================
void NetManager::statusLine(char *buf, size_t len) const {
  switch (mod) {
    case NetMode::ONLINE:
      snprintf(buf, len, "%s", WiFi.localIP().toString().c_str());
      break;
    case NetMode::AP_MODE:
      snprintf(buf, len, "AP %s", WiFi.softAPIP().toString().c_str());
      break;
    case NetMode::CONNECTING:
    default:
      snprintf(buf, len, "wifi...");
      break;
  }
}
