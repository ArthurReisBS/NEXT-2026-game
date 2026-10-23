# NEXT 2026 — Game

## Como funciona

```
[3 botões + 3 botões] → 2 ESP32 ──Bluetooth (BLE)──▶ PC rodando desafio_planta (C++ / raylib)
          "BTN1" ... "BTN6"
```

- São **duas placas ESP32, uma por jogador** (`NEXT2026-J1` e `NEXT2026-J2`), cada uma com 3 botões arcade. A cada aperto a placa envia por Bluetooth o texto `BTN1`-`BTN3` (jogador 1) ou `BTN4`-`BTN6` (jogador 2).
- O jogo (**Desafio EcoTrack**, pasta [`desafio-planta`](desafio-planta/README.md)) procura o ESP32 sozinho, conecta e reconecta se cair. Não precisa parear no Windows nem de internet.
- No minigame de **Criticidade** os dois jogadores respondem ao mesmo tempo: **quem apertar primeiro responde e o outro perde a vez** naquela rodada (acertou = 1 ponto; errou = ninguém pontua).
- O teclado funciona sempre, com ou sem ESP32: jogador 1 = teclas **1 2 3**, jogador 2 = teclas **8 9 0**.
- O firmware ainda se atualiza por OTA: ao ligar, usa o Wi-Fi só para checar o manifesto da própria placa (`version_j1.json` ou `version_j2.json`), depois desliga o Wi-Fi e fica só no Bluetooth.

| Placa | Botão | Pino | LED | Envia |
| ----- | ----- | ---- | --- | ----- |
| J1 (`ota_boot_j1`) | verde | GPIO18 | GPIO22 | `BTN1` |
| J1 | amarelo | GPIO19 | GPIO5 | `BTN2` |
| J1 | Enter | GPIO21 | — | `BTN3` |
| J2 (`ota_boot_j2`) | verde | GPIO12 | GPIO14 | `BTN4` |
| J2 | amarelo | GPIO27 | GPIO13 | `BTN5` |
| J2 | Enter | GPIO18 | — | `BTN6` |

No minigame de Altura cada jogador monta o seu número e os dois precisam confirmar; na Criticidade vale o primeiro clique.

Cada botão vai entre o pino e o **GND** (sem resistor, o pull-up é interno). Na placa J2 o GPIO12 decide a tensão da flash no boot: o botão dele vai para o GND, nunca para o 3,3 V.

## Guia rápido

**Só quero jogar** (as placas já foram gravadas):
1. Ligue as duas placas ESP32 na energia (USB de carregador serve) e espere uns 15 segundos.
2. Ligue o Bluetooth do computador e abra o jogo:
   ```bash
   cd desafio-planta
   cmake -B build
   cmake --build build --config Release
   build\Release\desafio_planta.exe
   ```
3. No canto da tela deve aparecer **"Bluetooth: 2 controles"**. Aperte o botão verde ou o amarelo para começar.
4. Sem as placas? Joga pelo teclado: jogador 1 = teclas 1 2 3, jogador 2 = teclas 8 9 0.

**Quero mudar alguma coisa:**

| Quero mudar... | Onde | Depois |
| -------------- | ---- | ------ |
| As fotos do jogo | copie a foto para `desafio-planta/assets/imagens/` e acrescente uma linha em `desafio-planta/assets/rodadas.csv` (`arquivo.jpg;altura;altura` ou `;criticidade`) | só abrir o jogo de novo |
| Os limites de OK / Alerta / Crítico | `LIMITE_ALERTA_CM` e `LIMITE_CRITICO_CM` em `desafio-planta/src/game.hpp` | recompilar (`cmake --build build --config Release`) |
| As regras e a pontuação | `desafio-planta/src/game.hpp` | recompilar e rodar os testes (abaixo) |
| O visual (cores, textos, cenário) | `desafio-planta/src/main.cpp` | recompilar |
| Os botões das placas (pinos, nome) | `ota_boot_j1/ota_boot_j1.ino` e `ota_boot_j2/ota_boot_j2.ino` | gravar a placa de novo pela USB |

Rodar os testes das regras (leva 2 segundos):
```bash
g++ -std=c++17 desafio-planta/tests/test_game.cpp -o test_game && ./test_game
```

**Uma placa não conecta ou voltou a funcionar por MQTT:** grave ela de novo pela USB (passo "Gravar o ESP32" abaixo, e vale para J1 e para J2).

## Como rodar

### 1. Gravar o ESP32

1. No Arduino IDE, instale o core **ESP32** e a biblioteca **NimBLE-Arduino**. Repita os passos 2 a 4 **uma vez para cada placa** (J1 e J2).
2. Crie o `.env` e preencha:
   ```bash
   cp .env.example .env
   ```
   | Variável | O que é |
   | -------- | ------- |
   | `WIFI_SSID` / `WIFI_PASSWORD` | rede Wi-Fi 2,4 GHz (só para o OTA) |
   | `MANIFEST_URL` | URL raw do `version.json` (o script troca por `version_j1.json` / `version_j2.json`, um manifesto por placa) |
3. Gere o `secrets.h` e grave **`ota_boot_j1/ota_boot_j1.ino`** (placa do jogador 1) ou **`ota_boot_j2/ota_boot_j2.ino`** (placa do jogador 2) pela USB (placa **ESP32 Dev Module**):
   ```bash
   ./gerar_secrets.sh --usb
   ```
4. No Serial Monitor (115200) deve aparecer `Bluetooth anunciando como NEXT2026-J1` (ou `J2`).

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
| Jogo fica em "Bluetooth: procurando..." | Ligue o Bluetooth do PC. O ESP32 só começa a anunciar ~10 s depois de ligar (espera o Wi-Fi). Confira com o app **nRF Connect** no celular se `NEXT2026-J1` / `NEXT2026-J2` aparece. |
| Nada acontece ao apertar, e o Serial não mostra `BTN1`... | Botão fora do pino da tabela ou sem o outro lado no GND. Teste com um fio: encoste o pino do botão (ex.: GPIO18 na placa J1) no GND. |
| Serial mostra `(jogo nao conectado por Bluetooth, nao enviado)` | O jogo ainda não conectou. Abra o jogo e espere o "Bluetooth: conectado". |
| `could not open port` / "acesso negado" na COM | Algo já usa a porta (Serial Monitor aberto). Feche e tente de novo. |
| Falha ao gravar | Segure o botão **BOOT** da placa durante o "Connecting...". Confira a porta COM. |
| A placa voltou a funcionar por MQTT / sumiu do Bluetooth | Ela estava com firmware gravado antes do conserto do OTA e baixou o `.bin` antigo (MQTT). **Grave de novo pela USB** com o `gerar_secrets.sh --usb` atual: a partir daí ela consulta o manifesto certo e nunca mais volta atrás. Para conferir, o Serial mostra `Versao instalada: 3.4` e `A versao instalada ja e a mais recente.` |
| Serial Monitor reinicia a placa | Normal: abrir a porta serial reseta o ESP32. Espere ~10 s. |
| Erro de compilação com `cmake` | Rode pelo "Developer Command Prompt for VS" ou instale o CMake e as ferramentas C++ do Visual Studio Build Tools. |
| Linux/macOS | O Bluetooth nativo só existe no Windows. Use o jogo por USB: `./desafio_planta /dev/ttyUSB0` (o ESP32 também escreve `BTN1`... na serial). |

## Publicar uma nova versão (OTA)

Cada placa tem o seu firmware e o seu manifesto, publicados na raiz do repositório:

| Placa | Manifesto | Binário |
| ----- | --------- | ------- |
| J1 | `version_j1.json` | `current_firmware_j1.bin` |
| J2 | `version_j2.json` | `current_firmware_j2.bin` |

1. Altere `VERSAO_FIRMWARE` nos dois `.ino` (`ota_boot_j1` e `ota_boot_j2`).
2. Gere o `secrets.h` **sem** Wi-Fi (`./gerar_secrets.sh`) e exporte o binário de cada sketch (`arduino-cli compile --fqbn esp32:esp32:esp32 ota_boot_j1 --output-dir saida_j1`; o arquivo é o `ota_boot_j1.ino.bin`). Salve como `current_firmware_j1.bin` e `current_firmware_j2.bin`. Assim a senha do Wi-Fi não vai para o repositório.
3. Coloque a mesma versão em `version_j1.json` e `version_j2.json`.
4. Faça commit e push dos quatro arquivos na `main` (o OTA lê da `main`).

**Nunca** deixe um manifesto com versão diferente da instalada apontando para um binário de outro firmware: a placa baixa e se regrava na hora. Foi assim que as placas voltaram para o MQTT antes. Os antigos `version.json` e `current_firmware.bin` (MQTT) foram removidos; as placas ainda com firmware antigo recebem erro 404 no manifesto, não se atualizam e precisam de uma gravação pela USB.

Na próxima vez que for ligada, a placa confere o manifesto dela. O firmware ocupa ~98% da partição OTA, então há pouca folga para crescer.

## Protocolo Bluetooth

Cada ESP32 é um periférico BLE chamado `NEXT2026-J1` ou `NEXT2026-J2`, e o jogo conecta nos dois.

| | UUID |
| --- | --- |
| Serviço | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Característica (notify) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

Cada notificação é o texto `BTN1` a `BTN6` (1-3 = jogador 1, 4-6 = jogador 2).

## Estrutura

```
├── ota_boot_j1/ota_boot_j1.ino   # firmware da placa do jogador 1 (BTN1-3)
├── ota_boot_j2/ota_boot_j2.ino   # firmware da placa do jogador 2 (BTN4-6)
├── ponte_ble/                    # ponte Bluetooth → serial virtual, só para Linux
├── desafio-planta/               # o jogo (C++ / raylib) — src/ble.cpp é o receptor Bluetooth
├── .env.example                  # modelo de configuração
├── gerar_secrets.sh              # gera o secrets.h de cada placa a partir do .env
├── version_j1.json / version_j2.json            # manifesto de OTA de cada placa
└── current_firmware_j1.bin / _j2.bin            # firmware publicado de cada placa
```

## Status

Testado no Windows com um ESP32 real (placa J1): o jogo conecta por Bluetooth, mostra "Bluetooth: 1 de 2 controles" e reage aos `BTN1`-`BTN3` simulados pelo Serial Monitor. A placa J2, os **botões físicos** e o OTA ponta a ponta (a placa baixando o `.bin` da `main`) ainda não foram testados.
