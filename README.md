# NEXT 2026 — Game

## Como funciona

```
[6 botões] → ESP32 ──Bluetooth (BLE)──▶ PC rodando desafio_planta (C++ / raylib)
          "BTN1" ... "BTN6"
```

- O ESP32 lê 6 botões arcade (3 por jogador) e, a cada aperto, envia por Bluetooth o texto `BTN1` a `BTN6`.
- O jogo (**Desafio EcoTrack**, pasta [`desafio-planta`](desafio-planta/README.md)) procura o ESP32 sozinho, conecta e reconecta se cair. Não precisa parear no Windows nem de internet.
- No minigame de **Criticidade** os dois jogadores respondem ao mesmo tempo: **quem apertar primeiro responde e o outro perde a vez** naquela rodada (acertou = 1 ponto; errou = ninguém pontua).
- O teclado funciona sempre, com ou sem ESP32: jogador 1 = teclas **1 2 3**, jogador 2 = teclas **8 9 0**.
- O firmware ainda se atualiza por OTA: ao ligar, usa o Wi-Fi só para checar o `version.json`, depois desliga o Wi-Fi e fica só no Bluetooth.

| Jogador | Botão | Pino | Envia |
| ------- | ----- | ---- | ----- |
| 1 | verde | GPIO25 | `BTN1` |
| 1 | amarelo | GPIO26 | `BTN2` |
| 1 | vermelho | GPIO27 | `BTN3` |
| 2 | verde | GPIO32 | `BTN4` |
| 2 | amarelo | GPIO33 | `BTN5` |
| 2 | vermelho | GPIO23 | `BTN6` |

No minigame de Altura cada jogador usa o seu conjunto e os dois precisam confirmar; na Criticidade vale o primeiro clique.

Cada botão vai entre o pino e o **GND** (sem resistor, o pull-up é interno).

## Como rodar

### 1. Gravar o ESP32

1. No Arduino IDE, instale o core **ESP32** e a biblioteca **NimBLE-Arduino**.
2. Crie o `.env` e preencha:
   ```bash
   cp .env.example .env
   ```
   | Variável | O que é |
   | -------- | ------- |
   | `WIFI_SSID` / `WIFI_PASSWORD` | rede Wi-Fi 2,4 GHz (só para o OTA) |
   | `MANIFEST_URL` | URL raw do `version.json` |
3. Gere o `secrets.h` e grave o `ota_boot/ota_boot.ino` pela USB (placa **ESP32 Dev Module**):
   ```bash
   ./gerar_secrets.sh --usb
   ```
4. No Serial Monitor (115200) deve aparecer `Bluetooth anunciando como NEXT2026-botoes`.

### 2. Compilar e abrir o jogo (Windows)

Precisa de CMake e do Visual Studio Build Tools (C++). Na primeira vez baixa o raylib sozinho.

```bash
cd desafio-planta
cmake -B build
cmake --build build --config Release
build\Release\desafio_planta.exe
```

Com o Bluetooth do PC ligado, o canto da tela muda de "Bluetooth: procurando..." para **"Bluetooth: conectado"**. Mais detalhes (regras, fotos, Linux/macOS) em [`desafio-planta/README.md`](desafio-planta/README.md).

### Testar sem os botões

- **Teclado**: jogador 1 = teclas 1, 2, 3 e jogador 2 = teclas 8, 9, 0.
- **Pelo ESP32**: com ele ligado na USB, digite `1` a `6` no Serial Monitor. É como apertar o botão: o ESP32 manda `BTN1` a `BTN6` pelo Bluetooth.
- **Lógica do jogo**: `g++ -std=c++17 desafio-planta/tests/test_game.cpp -o test_game && ./test_game`

## Problemas comuns

| Sintoma | O que fazer |
| ------- | ----------- |
| Jogo fica em "Bluetooth: procurando..." | Ligue o Bluetooth do PC. O ESP32 só começa a anunciar ~10 s depois de ligar (espera o Wi-Fi). Confira com o app **nRF Connect** no celular se `NEXT2026-botoes` aparece. |
| Nada acontece ao apertar, e o Serial não mostra `BTN1`... | Botão fora do pino da tabela ou sem o outro lado no GND. Teste com um fio: encoste o GPIO25 no GND. |
| Serial mostra `(jogo nao conectado por Bluetooth, nao enviado)` | O jogo ainda não conectou. Abra o jogo e espere o "Bluetooth: conectado". |
| `could not open port` / "acesso negado" na COM | Algo já usa a porta (Serial Monitor aberto). Feche e tente de novo. |
| Falha ao gravar | Segure o botão **BOOT** da placa durante o "Connecting...". Confira a porta COM. |
| O ESP32 voltou a uma versão antiga | O OTA baixou o `.bin` do `version.json`. Publique o firmware novo (abaixo) ou grave de novo pela USB. |
| Serial Monitor reinicia a placa | Normal: abrir a porta serial reseta o ESP32. Espere ~10 s. |
| Erro de compilação com `cmake` | Rode pelo "Developer Command Prompt for VS" ou instale o CMake e as ferramentas C++ do Visual Studio Build Tools. |
| Linux/macOS | O Bluetooth nativo só existe no Windows. Use o jogo por USB: `./desafio_planta /dev/ttyUSB0` (o ESP32 também escreve `BTN1`... na serial). |

## Publicar uma nova versão (OTA)

1. Altere `VERSAO_FIRMWARE` em `ota_boot.ino`.
2. Gere o `secrets.h` **sem** Wi-Fi (`./gerar_secrets.sh`), exporte o binário e salve como `current_firmware.bin`. Assim a senha não vai para o repositório.
3. Coloque a mesma versão em `version.json`.
4. Faça commit e push dos dois arquivos.

Na próxima vez que for ligado, o ESP32 se atualiza. O firmware ocupa ~98% da partição OTA, então há pouca folga para crescer.

## Protocolo Bluetooth

O ESP32 é um periférico BLE chamado `NEXT2026-botoes`.

| | UUID |
| --- | --- |
| Serviço | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Característica (notify) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

Cada notificação é o texto `BTN1` a `BTN6` (1-3 = jogador 1, 4-6 = jogador 2).

## Estrutura

```
├── ota_boot/ota_boot.ino   # firmware do ESP32 (botões → Bluetooth)
├── desafio-planta/         # o jogo (C++ / raylib) — src/ble.cpp é o receptor Bluetooth
├── .env.example            # modelo de configuração
├── gerar_secrets.sh        # gera ota_boot/secrets.h a partir do .env
├── version.json            # versão disponível + URL do .bin
└── current_firmware.bin    # firmware publicado
```

## Status

Testado com um ESP32 real (firmware 3.1) e o jogo no Windows: o jogo conecta por Bluetooth, mostra "Bluetooth: conectado" e reage a `BTN1`/`BTN2`/`BTN3` (simulados pelo Serial Monitor). A corrida da Criticidade (firmware 3.2, `BTN4-6`) foi testada na tela pelo teclado; ainda falta gravar o 3.2 e testar com o ESP32. Os **botões físicos** ainda não foram testados.
