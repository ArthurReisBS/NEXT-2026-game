# Desafio EcoTrack

Dois minigames com 3 botões arcade + ESP32 e uma interface em C++ (raylib) que segue o layout do PDF.

```
botões ──► ESP32 ──USB (serial)──► PC rodando desafio_planta (C++)
                  "BTN1" "BTN2" "BTN3"
```

O ESP32 não tem tela, então a interface roda no computador e o ESP32 só avisa qual botão foi apertado.

## Como se joga

Dois jogadores se revezam no mesmo conjunto de 3 botões (o Jogador 1 responde, depois o Jogador 2). As respostas ficam escondidas ("Pronto!") até os dois responderem; aí a foto revela a altura real e a criticidade. São 5 rodadas por partida.

Na tela inicial, o botão **verde** inicia o minigame de altura e o **amarelo** inicia o de criticidade.

| Botão | Minigame 1: altura | Minigame 2: criticidade |
|---|---|---|
| BTN1 verde (GPIO25) | +1 na dezena (0 a 9, volta ao 0) | OK |
| BTN2 amarelo (GPIO26) | +1 na unidade (0 a 9, volta ao 0) | Alerta |
| BTN3 vermelho (GPIO27) | confirmar | Crítico |

No minigame 2 o botão apertado já é a resposta final, porque não sobra botão para "confirmar".

Pontuação: na altura, ganha o ponto quem chutou mais perto (empate, os dois pontuam). Na criticidade, ganha o ponto quem acertou a faixa. Depois da revelação, qualquer botão avança (ou avança sozinho em 7 s).

Faixas de criticidade (em `src/game.hpp`, **ajuste para as faixas reais do EcoTrack**):
OK abaixo de 20 cm, Alerta de 20 a 29 cm, Crítico a partir de 30 cm.

## 1. Gravar o ESP32

1. Abra `firmware/botoes_esp32/botoes_esp32.ino` na Arduino IDE (com o pacote de placas ESP32 instalado).
2. Placa: "ESP32 Dev Module". Grave.
3. Teste: abra o Serial Monitor em 115200. Ao apertar cada botão deve aparecer `BTN1`, `BTN2` ou `BTN3`.
4. Feche o Serial Monitor antes de abrir o jogo (só um programa usa a porta por vez).

## 2. Compilar a interface

Precisa de CMake e de um compilador C++17 (Visual Studio Build Tools ou MinGW no Windows, g++ no Linux). Na primeira vez ele baixa o raylib 5.5 sozinho, então precisa de internet.

```
cmake -B build
cmake --build build --config Release
```

No Linux, instale antes as bibliotecas do raylib (Ubuntu: `sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`).

## 3. Rodar

```
build\Release\desafio_planta.exe COM5        (Windows; veja a porta no Gerenciador de Dispositivos)
./build/desafio_planta /dev/ttyUSB0          (Linux)
```

- Sem argumento, ele lê a porta de `assets/porta.txt`.
- Sem ESP32: rode sem argumento e use o **teclado** (teclas 1, 2 e 3 fazem o papel dos botões). O teclado funciona sempre, mesmo com o ESP32 ligado.
- F11 alterna tela cheia; ESC sai.

## Colocando as fotos reais

1. Copie as fotos para `assets/imagens/`.
2. Liste em `assets/rodadas.csv`, uma por linha: `grama01.jpg;27` (nome do arquivo; altura real em cm).
3. Se tiver mais de 5 fotos, cada partida sorteia 5.

Enquanto o CSV estiver vazio, o jogo usa a foto de exemplo do PDF e desenhos de teste.

## Estrutura

```
src/game.hpp        regras dos dois minigames (sem raylib, sem hardware)
src/main.cpp        interface (telas do PDF, confete, escala da janela)
src/serial.*        leitura da porta serial (Windows, Linux e macOS)
tests/test_game.cpp testes da lógica (ctest ou compile e rode direto)
firmware/           código do ESP32
assets/             logo, fotos, rodadas.csv, porta.txt, fonte opcional
```

Para rodar só os testes da lógica, sem raylib: `g++ -std=c++17 tests/test_game.cpp -o test_game && ./test_game`
