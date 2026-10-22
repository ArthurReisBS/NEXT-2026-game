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
// Os dois montam o número ao mesmo tempo; o Enter do J2 encerra a rodada.
static void testeAlturaComoNoPdf() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}});
    CHECK(j.fase() == Fase::Inicio);
    j.apertar(Botao::B1);  // verde inicia o modo Altura
    CHECK(j.fase() == Fase::Jogando);
    CHECK(j.modo() == Modo::Altura);

    apertarN(j, Botao::B1, 3);  // J1: dezena 3
    apertarN(j, Botao::B4, 2);  // J2: dezena 2
    apertarN(j, Botao::B2, 2);  // J1: unidade 2 -> 32
    apertarN(j, Botao::B5, 8);  // J2: unidade 8 -> 28
    CHECK(j.resposta(0).cm() == 32);
    CHECK(j.resposta(0).critExibida(Modo::Altura) == Crit::Critico);
    CHECK(j.resposta(1).cm() == 28);
    CHECK(j.resposta(1).critExibida(Modo::Altura) == Crit::Alerta);
    j.apertar(Botao::B6);       // Enter do J2

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

// Criticidade é uma corrida: o primeiro clique decide a rodada e o outro jogador perde a vez.
static void testeCriticidadeCorrida() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}});  // 27 cm = Alerta
    j.apertar(Botao::B2);  // amarelo inicia o modo Criticidade
    CHECK(j.modo() == Modo::Criticidade);

    // J1 aperta primeiro e erra (Crítico): J2 perde a vez e ninguém pontua.
    j.apertar(Botao::B3);
    CHECK(j.fase() == Fase::Revelacao);
    CHECK(j.resposta(0).respondeuCrit && j.resposta(0).crit == Crit::Critico);
    CHECK(!j.resposta(1).respondeuCrit && j.resposta(1).perdeuVez);
    CHECK(!j.resposta(0).perdeuVez);
    CHECK(j.placar(0) == 0 && j.placar(1) == 0);
    CHECK(!j.ganhouRodada(0) && !j.ganhouRodada(1));
}

static void testeCriticidadeSegundoJogadorPrimeiro() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}, {"y.jpg", 5}});
    j.apertar(Botao::B2);
    // J2 (botões 4-6) aperta primeiro e acerta: ponto para ele, J1 perde a vez.
    Crit certo = j.rodada().crit();
    j.apertar(certo == Crit::Ok ? Botao::B4 : certo == Crit::Alerta ? Botao::B5 : Botao::B6);
    CHECK(j.fase() == Fase::Revelacao);
    CHECK(j.placar(0) == 0 && j.placar(1) == 1);
    CHECK(j.ganhouRodada(1) && !j.ganhouRodada(0));
    CHECK(j.resposta(0).perdeuVez);
    // na próxima rodada o estado de perdeu-a-vez é zerado
    j.atualizar(1.5f);
    j.apertar(Botao::B1);
    CHECK(j.fase() == Fase::Jogando);
    CHECK(!j.resposta(0).perdeuVez && !j.resposta(1).perdeuVez);
}

// Na Altura cada controle só mexe no seu jogador: os botões do J2 não mudam o número do J1.
static void testeAlturaCadaControleNoSeuJogador() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}});
    j.apertar(Botao::B4);  // verde do J2 também inicia a Altura
    CHECK(j.modo() == Modo::Altura && j.fase() == Fase::Jogando);
    j.apertar(Botao::B4);  // dezena do J2
    j.apertar(Botao::B5);  // unidade do J2
    CHECK(j.resposta(1).cm() == 11);
    CHECK(j.resposta(0).cm() == 0);
    j.apertar(Botao::B1);  // dezena do J1
    CHECK(j.resposta(0).cm() == 10);
    CHECK(j.resposta(1).cm() == 11);
}

// O primeiro Enter encerra a rodada, mesmo que o outro jogador não tenha confirmado.
static void testeAlturaPrimeiroEnterEncerra() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 27}, {"y.jpg", 5}});
    j.apertar(Botao::B1);
    apertarN(j, Botao::B1, 2);  // J1: 20
    apertarN(j, Botao::B4, 3);  // J2: 30, ainda sem confirmar
    j.apertar(Botao::B3);       // Enter do J1
    CHECK(j.fase() == Fase::Revelacao);
    CHECK(j.resposta(0).travada && !j.resposta(1).travada);
    // vale o número que o J2 tinha montado: |30-27| = 3 < |20-27| = 7
    CHECK(j.placar(0) == 0 && j.placar(1) == 1);
    j.apertar(Botao::B6);       // Enter atrasado do J2 não muda nada
    CHECK(j.fase() == Fase::Revelacao && j.placar(1) == 1);
}

static void testeEmpateEPontosDuplos() {
    Jogo j;
    j.definirRodadas({{"x.jpg", 25}});
    j.apertar(Botao::B1);
    // Os dois digitam 24 -> empate na distância, os dois pontuam.
    apertarN(j, Botao::B1, 2);
    apertarN(j, Botao::B2, 4);
    apertarN(j, Botao::B4, 2);
    apertarN(j, Botao::B5, 4);
    j.apertar(Botao::B3);
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
        j.apertar(Botao::B1);  // corrida: um clique decide a rodada
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
    j.apertar(Botao::B3);  // primeiro Enter já encerra
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
    testeCriticidadeCorrida();
    testeCriticidadeSegundoJogadorPrimeiro();
    testeAlturaCadaControleNoSeuJogador();
    testeAlturaPrimeiroEnterEncerra();
    testeEmpateEPontosDuplos();
    testePartidaCompletaEAvancoAutomatico();
    testeBotaoAntesDoTempoMinimoEIgnorado();
    testeSemRodadasNaoInicia();
    testeFaixas();
    if (falhas == 0) std::printf("OK: todos os testes passaram\n");
    return falhas == 0 ? 0 : 1;
}
