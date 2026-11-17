<div align="center">

# ⚡ LittleTosh

**Pequeno robô de mesa com olhos animados que reagem quando você inclina, cutuca ou chacoalha.**

![ESP8266](https://img.shields.io/badge/Wemos_D1_mini-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Arduino](https://img.shields.io/badge/Arduino-00878F?style=for-the-badge&logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Fusion 360](https://img.shields.io/badge/Fusion_360-F58220?style=for-the-badge&logo=autodesk&logoColor=white)

</div>

---

## Sobre

O **LittleTosh** é um companheiro de mesa simples, feito com poucas peças baratas, mas que parece
ter personalidade. Dois olhos animados numa telinha OLED reagem ao movimento: olham para o lado
quando você inclina, dão risada com um tapinha e, à noite, ficam cansados e dormem.

A case foi modelada no **Fusion 360** (`LittleTosh.f3d`) e impressa em 3D.

## Comportamentos

| Ação | Reação |
|---|---|
| Parado e nivelado | Olhos relaxam e entram em modo *idle* |
| Inclinar | Os olhos "caem" para o lado da inclinação |
| Tapinha / toque | Animação de risada |
| Chacoalhar | Entra no **modo debug** por alguns segundos (dados do sensor, hora e rede) |
| 23h (configurável) | Fica cansado, "pesca" e dorme (Zzz) |
| Mexer enquanto dorme | Acorda e, depois de alguns minutos quieto, volta a dormir |

## Hardware

| Peça | Detalhe |
|---|---|
| Wemos D1 mini (ESP8266) | Placa principal |
| OLED SSD1306 128×64 | Endereço I2C `0x3C` |
| Acelerômetro MPU-6050 / 6500 / 9250 | Endereço `0x68` (ou `0x69` com AD0 em nível alto) |
| Case impressa em 3D | Modelo em `LittleTosh.f3d` |

**Ligações (I2C padrão do D1 mini):** `SDA → D2 (GPIO4)` · `SCL → D1 (GPIO5)`

## Rede e Configuração

- No boot, tenta a rede Wi-Fi salva. Se não conectar em 20 s, abre o Access Point **`LittleTosh`**
  com portal captivo para configurar tudo pelo navegador.
- Conectado, pega a hora certa via **NTP** e serve um painel em **http://littletosh.local**.
- No painel dá para trocar o Wi-Fi e ajustar o sono: hora de dormir, hora de acordar, quantos
  minutos fica acordado depois de uma interação e o fuso horário.

## Estrutura

Cada arquivo cuida de uma coisa só:

```text
little-tosh/
|-- little-tosh.ino      liga tudo no setup() e no loop()
|-- config.h             todas as constantes ajustáveis (pinos, limiares, tempos)
|-- settings_store.*     configurações persistentes na EEPROM
|-- mpu_sensor.*         driver I2C do MPU + gestos (inclinação, toque, chacoalhada)
|-- face.*               display + RoboEyes + animações (inclusive sono)
|-- night_clock.*        hora certa via NTP
|-- sleep_scheduler.*    máquina de estados do ciclo de sono
|-- net_manager.*        Wi-Fi (cliente/AP), portal captivo e painel de config
|-- web_page.h           HTML do painel, embutido na flash
`-- debug_screen.*       tela de diagnóstico da chacoalhada
LittleTosh.f3d           modelo 3D da case (Fusion 360)
```

## Rodando

1. Na **Arduino IDE**, instale o pacote de placas **ESP8266** e selecione
   **LOLIN(WEMOS) D1 R2 & mini**.
2. Pelo Library Manager, instale: **Adafruit GFX**, **Adafruit SSD1306** e **FluxGarage RoboEyes**.
3. Abra `little-tosh/little-tosh.ino` e envie para a placa.
4. Conecte na rede **`LittleTosh`** pelo celular e configure o seu Wi-Fi.

> Os limiares de inclinação, toque e chacoalhada ficam em `config.h`
> (`TILT_THRESHOLD`, `TAP_THRESHOLD`, `SHAKE_PEAK_HI`...). Se os eixos ficarem invertidos
> na sua montagem, ajuste `INVERT_X` / `INVERT_Y`.

## Créditos

Animações dos olhos com a biblioteca [FluxGarage RoboEyes](https://github.com/FluxGarage/RoboEyes).
