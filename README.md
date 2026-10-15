# NEXT 2026 — Game

Controle de jogo com ESP32 e três botões. Cada aperto publica uma letra via MQTT, que o jogo recebe pela internet. O firmware também se atualiza sozinho (OTA): ao ligar, lê o `version.json` deste repositório e, se a versão for diferente da instalada, baixa o novo `.bin`, grava e reinicia.

| Botão | Pino | Envia |
| ----- | ---- | ----- |
| 1 | D18 | `G` |
| 2 | D19 | `Y` |
| 3 | D21 | `E` |

Cada botão vai entre o pino e o **GND** (sem resistor, o pull-up é interno).

## Estrutura

```
NEXT/
├── ota_boot/
│   └── ota_boot.ino      # firmware
├── .env.example          # modelo de configuração
├── gerar_secrets.sh      # gera ota_boot/secrets.h a partir do .env
├── jogo/                 # jogo em Python (pygame-ce)
├── version.json          # versão disponível + URL do .bin
└── current_firmware.bin  # firmware mais recente
```

## Como usar

1. Instale o core do ESP32 e a biblioteca **PubSubClient** no Arduino IDE ou no `arduino-cli`.
2. Crie o `.env` a partir do modelo e preencha:
   ```bash
   cp .env.example .env
   ```
   | Variável | O que é |
   | -------- | ------- |
   | `WIFI_SSID` / `WIFI_PASSWORD` | rede Wi-Fi (2,4 GHz) |
   | `MANIFEST_URL` | URL raw do `version.json` |
   | `MQTT_BROKER` | broker MQTT |
   | `MQTT_TOPIC` | tópico onde as letras são publicadas |
3. Gere o `secrets.h` **com** Wi-Fi, compile e grave o `ota_boot` no ESP32 pela USB:
   ```bash
   ./gerar_secrets.sh --usb
   ```
   A rede fica salva na memória do ESP32 e continua lá depois das atualizações OTA. Só é preciso repetir este passo para trocar de rede.

### Receber no jogo

Assine o tópico `MQTT_TOPIC` no broker `MQTT_BROKER` (porta 1883). Cada mensagem é uma letra: `G`, `Y` ou `E`.

### Jogo

Campo numérico controlado pelos botões: `G` soma 1, `Y` soma 10 e `E` confirma. As teclas G, Y e Enter fazem o mesmo, para testar sem o ESP32.

```bash
cd jogo
pip install -r requirements.txt
python jogo.py
```

### Publicar uma nova versão

1. Altere `VERSAO_FIRMWARE` em `ota_boot.ino`.
2. Gere o `secrets.h` **sem** Wi-Fi (`./gerar_secrets.sh`), exporte o binário e salve como `current_firmware.bin`. Assim a senha não vai para o repositório.
3. Coloque a mesma versão em `version.json`.
4. Faça commit e push dos dois arquivos.

Na próxima vez que for ligado, o ESP32 se atualiza.
