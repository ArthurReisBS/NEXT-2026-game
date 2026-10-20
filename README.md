# NEXT 2026 — Game

## Como funciona

```
[botões] → ESP32 ──Bluetooth (BLE)──▶ PC (jogo.py)
```

- Cada ESP32 é um **controle** com 3 botões. Ao apertar, ele envia por Bluetooth a letra + o número do jogador: `G1`, `Y1`, `E2`...
- O **jogo** (Python) procura os controles sozinho, conecta (até 2 ao mesmo tempo) e reconecta se algum cair. Não precisa parear no Windows nem de internet.
- Regras: `G` soma 1, `Y` soma 10, `E` confirma o número.
- O firmware ainda se atualiza por OTA: ao ligar, usa o Wi-Fi só para checar o `version.json`, depois desliga o Wi-Fi e fica só no Bluetooth.

| Botão | Pino | Letra |
| ----- | ---- | ----- |
| 1 | D18 | `G` |
| 2 | D19 | `Y` |
| 3 | D21 | `E` |

Cada botão vai entre o pino e o **GND** (sem resistor, o pull-up é interno).

## Como rodar

### 1. Gravar o ESP32 (uma vez por controle)

1. No Arduino IDE, instale o core **ESP32** e a biblioteca **NimBLE-Arduino**.
2. Crie o `.env` e preencha:
   ```bash
   cp .env.example .env
   ```
   | Variável | O que é |
   | -------- | ------- |
   | `WIFI_SSID` / `WIFI_PASSWORD` | rede Wi-Fi 2,4 GHz (só para o OTA) |
   | `MANIFEST_URL` | URL raw do `version.json` |
   | `JOGADOR` | `1` ou `2` — um valor diferente em cada controle |
3. Gere o `secrets.h` e grave o `ota_boot/ota_boot.ino` pela USB (placa **ESP32 Dev Module**):
   ```bash
   ./gerar_secrets.sh --usb
   ```
4. No Serial Monitor (115200) deve aparecer `Bluetooth anunciando como NEXT2026-J1`.

### 2. Abrir o jogo

Com o Bluetooth do PC ligado:

```bash
cd jogo
pip install -r requirements.txt
python jogo.py
```

O rodapé muda de "procurando controles..." para **"Bluetooth: 1 controle(s)"**. Aperte os botões e o número do jogador sobe.

### Testar sem o ESP32

```bash
cd jogo
python -m unittest test_bluetooth -v
```

Testa o receptor Bluetooth (com BLE simulado) e as regras do jogo.

## Problemas comuns

| Sintoma | O que fazer |
| ------- | ----------- |
| Jogo fica em "procurando controles..." | Ligue o Bluetooth do PC. O ESP32 só começa a anunciar ~10 s depois de ligar (espera o Wi-Fi). Confira com o app **nRF Connect** no celular se `NEXT2026-J1` aparece. |
| `Bluetooth indisponivel` no terminal | Bluetooth do Windows desligado ou sem adaptador BLE (precisa de Bluetooth 4.0+). |
| `(jogo nao conectado, nao enviado)` no Serial | O jogo ainda não conectou. Abra o jogo e espere o contador de controles. |
| Nenhuma linha `Botao ...` ao apertar | Botão fora do pino D18/D19/D21 ou sem o outro lado no GND. Teste com um fio: encoste D18 no GND. |
| `could not open port` / "acesso negado" na COM | Algo já usa a porta (Serial Monitor aberto). Feche e tente de novo. |
| Falha ao gravar | Segure o botão **BOOT** da placa durante o "Connecting...". Confira a porta COM. |
| O ESP32 voltou a uma versão antiga | O OTA baixou o `.bin` do `version.json`. Publique o firmware novo (abaixo) ou grave de novo pela USB. |
| Jogador errado no controle | O `JOGADOR` do `.env` vale na hora de gravar. Ajuste, rode `./gerar_secrets.sh --usb` e grave de novo. |
| Dois controles somam no mesmo jogador | Os dois foram gravados com o mesmo `JOGADOR`. Use `1` e `2`. |
| Serial Monitor reinicia a placa | Normal: abrir a porta serial reseta o ESP32. Espere ~10 s. |

## Publicar uma nova versão (OTA)

1. Altere `VERSAO_FIRMWARE` em `ota_boot.ino`.
2. Gere o `secrets.h` **sem** Wi-Fi (`./gerar_secrets.sh`), exporte o binário e salve como `current_firmware.bin`. Assim a senha não vai para o repositório.
3. Coloque a mesma versão em `version.json`.
4. Faça commit e push dos dois arquivos.

Na próxima vez que for ligado, o ESP32 se atualiza. O firmware ocupa ~98% da partição OTA, então há pouca folga para crescer.

## Protocolo Bluetooth

O ESP32 é um periférico BLE chamado `NEXT2026-J<jogador>`.

| | UUID |
| --- | --- |
| Serviço | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Característica (notify) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

Cada notificação é o texto `<letra><jogador>` (`G1`, `Y2`, `E1`...).

## Estrutura

```
├── ota_boot/ota_boot.ino   # firmware do ESP32
├── jogo/                   # jogo (pygame-ce + bleak) e testes
├── .env.example            # modelo de configuração
├── gerar_secrets.sh        # gera ota_boot/secrets.h a partir do .env
├── version.json            # versão disponível + URL do .bin
└── current_firmware.bin    # firmware publicado
```

## Status

O firmware compila e grava, o ESP32 anuncia e o jogo conecta nele (testado com hardware real). O envio por **botões físicos** ainda não foi testado, só o receptor com BLE simulado.
