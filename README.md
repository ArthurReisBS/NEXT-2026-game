# NEXT 2026 — Game

Firmware de ESP32 que se atualiza pela internet (OTA). Ao ligar, o ESP32 lê o `version.json` deste repositório e, se a versão for diferente da instalada, baixa o novo `.bin`, grava e reinicia.

## Estrutura

```
NEXT/
├── ota_boot/
│   └── ota_boot.ino      # firmware
├── .env.example          # modelo de configuração
├── gerar_secrets.sh      # gera ota_boot/secrets.h a partir do .env
├── version.json          # versão disponível + URL do .bin
└── current_firmware.bin  # firmware mais recente
```

## Como usar

1. Instale o core do ESP32 no Arduino IDE ou no `arduino-cli`.
2. Crie o `.env` a partir do modelo e preencha `WIFI_SSID`, `WIFI_PASSWORD` e `MANIFEST_URL`:
   ```bash
   cp .env.example .env
   ./gerar_secrets.sh
   ```
3. Compile e grave o `ota_boot` no ESP32 pela USB.

### Publicar uma nova versão

1. Altere `VERSAO_FIRMWARE` em `ota_boot.ino`.
2. Exporte o binário e salve como `current_firmware.bin`.
3. Coloque a mesma versão em `version.json`.
4. Faça commit e push dos dois arquivos.

Na próxima vez que for ligado, o ESP32 se atualiza.
