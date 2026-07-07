#include "settings_store.h"

#include <EEPROM.h>

#include "config.h"

// Mude o ultimo digito se alterar o layout da struct (invalida o que ja
// estiver gravado e forca a volta ao padrao).
constexpr uint32_t SETTINGS_MAGIC = 0x4C545331;  // "LTS1"
constexpr size_t   EEPROM_SIZE    = 256;

static uint8_t computeChecksum(const Settings &s) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&s);
  uint8_t sum = 0;
  for (size_t i = 0; i < offsetof(Settings, checksum); i++) sum ^= bytes[i];
  return sum;
}

void SettingsStore::resetToDefaults() {
  memset(&data, 0, sizeof(data));
  data.magic        = SETTINGS_MAGIC;
  data.sleepHour    = DEFAULT_SLEEP_HOUR;
  data.wakeHour     = DEFAULT_WAKE_HOUR;
  data.holdMinutes  = DEFAULT_HOLD_MINUTES;
  data.utcOffsetMin = DEFAULT_UTC_OFFSET_MIN;
}

void SettingsStore::begin() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, data);

  const bool valid = (data.magic == SETTINGS_MAGIC) &&
                     (data.checksum == computeChecksum(data));
  if (!valid) {
    Serial.println(F("[eeprom] vazia/invalida -> usando padrao."));
    resetToDefaults();
    return;
  }
  // Garante terminador mesmo se a gravacao anterior veio truncada.
  data.ssid[sizeof(data.ssid) - 1] = '\0';
  data.pass[sizeof(data.pass) - 1] = '\0';
  Serial.print(F("[eeprom] config carregada. rede salva: "));
  Serial.println(hasWifi() ? data.ssid : "(nenhuma)");
}

void SettingsStore::save() {
  data.magic    = SETTINGS_MAGIC;
  data.checksum = computeChecksum(data);
  EEPROM.put(0, data);
  EEPROM.commit();  // no ESP8266 o commit() e que grava na flash de verdade
  Serial.println(F("[eeprom] config salva."));
}

void SettingsStore::setWifi(const char *ssid, const char *pass) {
  strncpy(data.ssid, ssid, sizeof(data.ssid) - 1);
  data.ssid[sizeof(data.ssid) - 1] = '\0';
  strncpy(data.pass, pass, sizeof(data.pass) - 1);
  data.pass[sizeof(data.pass) - 1] = '\0';
}
