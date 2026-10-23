# Desafio EcoTrack

Dois minigames com 3 botões arcade + ESP32 e uma interface em C++ (raylib) que segue o layout do PDF.

```
botões ──► ESP32 ──Bluetooth (BLE)──► PC rodando desafio_planta (C++)
                  "BTN1" "BTN2" "BTN3"
```

O ESP32 não tem tela, então a interface roda no computador e o ESP32 só avisa qual botão foi apertado.

## Como se joga

Cada jogador tem 3 botões (jogador 1 = BTN1-3, jogador 2 = BTN4-6; no teclado, 1 2 3 e 8 9 0). São 5 rodadas por partida.

- **Altura**: os **dois jogam ao mesmo tempo**, cada um com o seu conjunto de botões: monta o número (dezena e unidade) e confirma com o vermelho. Quem confirma fica travado ("Pronto!") e a rodada só termina quando os **dois** confirmaram; aí a foto revela a altura real.
- **Criticidade (corrida)**: os dois jogam ao mesmo tempo. **Quem apertar primeiro responde e o outro perde a vez** naquela rodada (o cartão dele fica cinza com "perdeu a vez"). Acertou a faixa = 1 ponto; errou = ninguém pontua.

Na tela inicial, o botão **verde** inicia o minigame de altura e o **amarelo** inicia o de criticidade.

| Botão (J1 / J2) | Pino (J1 / J2) | Minigame 1: altura | Minigame 2: criticidade |
|---|---|---|---|
| verde (BTN1 / BTN4) | GPIO18 / GPIO12 | +1 na dezena (0 a 9, volta ao 0) | OK |
| amarelo (BTN2 / BTN5) | GPIO19 / GPIO27 | +1 na unidade (0 a 9, volta ao 0) | Alerta |
| Enter (BTN3 / BTN6) | GPIO21 / GPIO18 | confirmar | Crítico |

Na criticidade o botão apertado já é a resposta final, porque não sobra botão para "confirmar".

Pontuação: em cada rodada pontua no máximo um jogador. Na altura, ganha o ponto quem chutou mais perto (empate de distância: ninguém pontua). Na criticidade, ganha o ponto quem apertou primeiro e acertou a faixa. Depois da revelação, qualquer botão avança (ou avança sozinho em 7 s).

Faixas de criticidade (em `src/game.hpp`, **ajuste para as faixas reais do EcoTrack**):
OK abaixo de 20 cm, Alerta de 20 a 29 cm, Crítico a partir de 30 cm.

## 1. Gravar o ESP32

Há um firmware por placa: `../ota_boot_j1/ota_boot_j1.ino` (jogador 1, envia `BTN1`-`BTN3`) e `../ota_boot_j2/ota_boot_j2.ino` (jogador 2, `BTN4`-`BTN6`), cada um com os pinos da tabela acima. O passo a passo para gravar está no [README da raiz](../README.md).

Para testar: no Serial Monitor (115200), apertar cada botão mostra `BTN1`, `BTN2` ou `BTN3`; digitar `1` a `6` faz o mesmo sem os botões. Feche o Serial Monitor antes de abrir o jogo por serial (só um programa usa a porta por vez).

## 2. Compilar a interface

Precisa de CMake e de um compilador C++17 (Visual Studio Build Tools ou MinGW no Windows, g++ no Linux). Na primeira vez ele baixa o raylib 5.5 sozinho, então precisa de internet.

```
cmake -B build
cmake --build build --config Release
```

No Linux, instale antes as bibliotecas do raylib (Ubuntu: `sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`).

## 3. Rodar

No Windows, com o Bluetooth ligado, é só abrir o programa: ele acha o ESP32 sozinho (o canto da tela mostra "Bluetooth: conectado").

```
build\Release\desafio_planta.exe COM5        (Windows; veja a porta no Gerenciador de Dispositivos)
./build/desafio_planta /dev/ttyUSB0          (Linux)
```

- Sem argumento: usa a porta de `assets/porta.txt`, se tiver uma; senão procura o ESP32 por **Bluetooth** (Windows). Passar `COMx` força a serial USB (a única opção no Linux/macOS).
- `--teclado` desliga o Bluetooth e a serial.
- Sem ESP32: use o **teclado** (jogador 1 = teclas 1, 2 e 3; jogador 2 = teclas 8, 9 e 0). O teclado funciona sempre, mesmo com o ESP32 ligado.
- F11 alterna tela cheia; ESC sai.

## Fotos

As 10 fotos reais já estão em `assets/imagens/` (o nome do arquivo é a altura da grama em cm) e listadas em `assets/rodadas.csv`, 5 para cada desafio:

| Desafio | Fotos |
|---|---|
| Altura em cm | 5, 9, 12, 13 e 30 cm |
| Criticidade | 6 e 15 cm (OK), 25 cm (Alerta), 32 e 35 cm (Crítico) |

Para trocar ou acrescentar fotos:

1. Copie as fotos para `assets/imagens/` (de preferência em pé, tipo celular).
2. Liste em `assets/rodadas.csv`, uma por linha: `grama01.jpg;27;altura` (arquivo; altura real em cm; desafio).
3. O desafio pode ser `altura`, `criticidade` ou `ambos` (sem a 3ª coluna a foto vale para os dois). Cada partida sorteia até 5 fotos do desafio escolhido.

Se o CSV não tiver nenhuma linha válida, o jogo usa a foto de exemplo do PDF e desenhos de teste.

## Estrutura

```
src/game.hpp        regras dos dois minigames (sem raylib, sem hardware)
src/main.cpp        interface (telas do PDF, confete, escala da janela)
src/serial.*        leitura da porta serial (Windows, Linux e macOS)
tests/test_game.cpp testes da lógica (ctest ou compile e rode direto)
src/ble.*           receptor Bluetooth LE (Windows, C++/WinRT)
assets/             logo, fotos, rodadas.csv, porta.txt, fonte opcional
```

Para rodar só os testes da lógica, sem raylib: `g++ -std=c++17 tests/test_game.cpp -o test_game && ./test_game`
