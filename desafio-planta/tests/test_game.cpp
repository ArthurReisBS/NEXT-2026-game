// Testes da lógica do jogo (rodam no terminal, sem hardware e sem interface).
#include <cstdio>
#include <cstdlib>
#include "../src/game.hpp"

using namespace planta;

static int falhas = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) { std::printf("FALHOU linha %d: %s\n", __LINE__, #cond); ++falhas; } \
    } while (0)

static void apertarN(Jogo& j, Botao b, int n) { for (int i = 0; i < n; ++i) j.apertar(b); }

// Cenário do PDF: foto real = 27 cm (Alerta). J1 digita 32 (Crítico), J2 digita 28 (Alerta).
static void testeAlturaComoNoPdf() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}});
    CHECK(j.fase() == Fase::Inicio);
    j.apertar(Botao::B1);  // verde inicia o modo Altura
    CHECK(j.fase() == Fase::Jogando);
    CHECK(j.modo() == Modo::Altura);

    apertarN(j, Botao::B1, 3);  // dezena 3
    apertarN(j, Botao::B2, 2);  // unidade 2 -> 32
    CHECK(j.resposta(0).cm() == 32);
    CHECK(j.resposta(0).critExibida(Modo::Altura) == Crit::Critico);
    j.apertar(Botao::B3);       // enter do jogador 1
    CHECK(j.jogadorDaVez() == 1);

    apertarN(j, Botao::B1, 2);
    apertarN(j, Botao::B2, 8);  // 28
    CHECK(j.resposta(1).cm() == 28);
    CHECK(j.resposta(1).critExibida(Modo::Altura) == Crit::Alerta);
    j.apertar(Botao::B3);

    CHECK(j.fase() == Fase::Revelacao);
    CHECK(j.placar(0) == 0 && j.placar(1) == 1);  // "0 - 1" como no PDF
    CHECK(!j.ganhouRodada(0) && j.ganhouRodada(1));

    j.atualizar(1.5f);
    j.apertar(Botao::B2);  // passa para o fim (só 1 rodada)
    CHECK(j.fase() == Fase::Vencedor);
    CHECK(j.vencedor() == 1);
    j.atualizar(1.5f);
    j.apertar(Botao::B1);
    CHECK(j.fase() == Fase::Inicio);
}

static void testeVoltaDoContador() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 10}});
    j.apertar(Botao::B1);
    apertarN(j, Botao::B1, 10);  // 10 apertos = volta a 0
    apertarN(j, Botao::B2, 9);
    CHECK(j.resposta(0).dezena == 0);
    CHECK(j.resposta(0).unidade == 9);
    apertarN(j, Botao::B2, 1);
    CHECK(j.resposta(0).unidade == 0);
}

static void testeCriticidade() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}});
    j.apertar(Botao::B2);  // amarelo inicia o modo Criticidade
    CHECK(j.modo() == Modo::Criticidade);
    j.apertar(Botao::B3);  // J1: Crítico (errado) - já confirma
    CHECK(j.jogadorDaVez() == 1);
    j.apertar(Botao::B2);  // J2: Alerta (certo)
    CHECK(j.fase() == Fase::Revelacao);
    CHECK(j.placar(0) == 0 && j.placar(1) == 1);
}

static void testeEmpateEPontosDuplos() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 25}});
    j.apertar(Botao::B1);
    // Os dois digitam 24 -> empate na distância, os dois pontuam.
    for (int jogador = 0; jogador < 2; ++jogador) {
        apertarN(j, Botao::B1, 2);
        apertarN(j, Botao::B2, 4);
        j.apertar(Botao::B3);
    }
    CHECK(j.placar(0) == 1 && j.placar(1) == 1);
    j.atualizar(1.5f);
    j.apertar(Botao::B1);
    CHECK(j.fase() == Fase::Vencedor);
    CHECK(j.vencedor() == -1);
}

static void testePartidaCompletaEAvancoAutomatico() {
    Jogo j;
    std::vector<Rodada> r;
    for (int i = 0; i < 8; ++i) r.push_back({"x.jpg", 5 + i * 5});
    j.definirRodadas(r);
    j.apertar(Botao::B2);
    CHECK(j.totalRodadas() == RODADAS_POR_PARTIDA);
    for (int i = 0; i < RODADAS_POR_PARTIDA; ++i) {
        CHECK(j.fase() == Fase::Jogando);
        j.apertar(Botao::B1);
        j.apertar(Botao::B1);
        CHECK(j.fase() == Fase::Revelacao);
        j.atualizar(TEMPO_AUTO_REVELACAO + 0.1f);  // avança sozinho
    }
    CHECK(j.fase() == Fase::Vencedor);
    j.atualizar(TEMPO_AUTO_VENCEDOR + 0.1f);
    CHECK(j.fase() == Fase::Inicio);
}

static void testeBotaoAntesDoTempoMinimoEIgnorado() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 40}, {"y.jpg", 12}});
    j.apertar(Botao::B1);
    j.apertar(Botao::B3);
    j.apertar(Botao::B3);
    CHECK(j.fase() == Fase::Revelacao);
    j.apertar(Botao::B1);  // cedo demais
    CHECK(j.fase() == Fase::Revelacao);
    j.atualizar(1.2f);
    j.apertar(Botao::B1);
    CHECK(j.fase() == Fase::Jogando);
    CHECK(j.rodadaAtual() == 1);
}

static void testeSemRodadasNaoInicia() {
    Jogo j;
    j.apertar(Botao::B1);
    CHECK(j.fase() == Fase::Inicio);
}

static void testeFaixas() {
    CHECK(classificar(0) == Crit::Ok);
    CHECK(classificar(19) == Crit::Ok);
    CHECK(classificar(20) == Crit::Alerta);
    CHECK(classificar(29) == Crit::Alerta);
    CHECK(classificar(30) == Crit::Critico);
    CHECK(classificar(99) == Crit::Critico);
}

int main() {
    testeAlturaComoNoPdf();
    testeVoltaDoContador();
    testeCriticidade();
    testeEmpateEPontosDuplos();
    testePartidaCompletaEAvancoAutomatico();
    testeBotaoAntesDoTempoMinimoEIgnorado();
    testeSemRodadasNaoInicia();
    testeFaixas();
    if (falhas == 0) std::printf("OK: todos os testes passaram\n");
    return falhas == 0 ? 0 : 1;
}
