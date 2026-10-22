// game.hpp - regras dos dois minigames (sem dependência de raylib ou de hardware).
// Assim dá para testar a lógica no terminal e trocar a interface quando quiser.
#pragma once
#include <algorithm>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace planta {

// ---------------------------------------------------------------- configuração
// Faixas de criticidade (cm). AJUSTE para os valores reais do projeto EcoTrack.
// Com estes valores o exemplo do PDF fecha: 27 e 28 cm = Alerta, 32 cm = Crítico.
constexpr int LIMITE_ALERTA_CM  = 20;  // a partir daqui é Alerta
constexpr int LIMITE_CRITICO_CM = 30;  // a partir daqui é Crítico

constexpr int RODADAS_POR_PARTIDA = 5;
constexpr float TEMPO_MIN_REVELACAO = 1.0f;   // s antes de aceitar botão na revelação
constexpr float TEMPO_AUTO_REVELACAO = 7.0f;  // s até avançar sozinho
constexpr float TEMPO_AUTO_VENCEDOR = 20.0f;  // s até voltar ao início

// ---------------------------------------------------------------- tipos
enum class Modo { Altura, Criticidade };
enum class Crit { Ok, Alerta, Critico };
// Botões 1-3 (verde, amarelo, Enter) são do jogador 1; 4-6 (mesmas cores) são do jogador 2.
// Nos dois modos cada conjunto só mexe no seu jogador, e os dois jogam ao mesmo tempo.
enum class Botao { B1 = 1, B2 = 2, B3 = 3, B4 = 4, B5 = 5, B6 = 6 };
enum class Fase { Inicio, Jogando, Revelacao, Vencedor };

inline int jogadorDoBotao(Botao b) { return static_cast<int>(b) <= 3 ? 0 : 1; }
// Qual dos 3 botões é: 1 = verde, 2 = amarelo, 3 = Enter
inline int corDoBotao(Botao b) { return (static_cast<int>(b) - 1) % 3 + 1; }

inline Crit classificar(int cm) {
    if (cm >= LIMITE_CRITICO_CM) return Crit::Critico;
    if (cm >= LIMITE_ALERTA_CM) return Crit::Alerta;
    return Crit::Ok;
}

inline const char* nomeCrit(Crit c) {
    switch (c) {
        case Crit::Ok: return "OK";
        case Crit::Alerta: return "Alerta";
        case Crit::Critico: return "Crítico";
    }
    return "?";
}

// Em quais desafios a foto aparece (máscara de bits).
constexpr int PARA_ALTURA = 1;
constexpr int PARA_CRITICIDADE = 2;
constexpr int PARA_AMBOS = PARA_ALTURA | PARA_CRITICIDADE;

struct Rodada {
    std::string imagem;  // caminho relativo a assets/ (pode não existir: vira desenho de teste)
    int alturaCm = 0;
    int desafios = PARA_AMBOS;
    Crit crit() const { return classificar(alturaCm); }
};

struct Resposta {
    int dezena = 0;
    int unidade = 0;
    Crit crit = Crit::Ok;
    bool respondeuCrit = false;  // só importa no modo Criticidade
    bool travada = false;        // jogador já confirmou
    bool perdeuVez = false;      // Criticidade: o outro jogador apertou primeiro
    int cm() const { return dezena * 10 + unidade; }
    // O que aparece no cartão: no modo Altura a cor sai da altura digitada.
    Crit critExibida(Modo m) const { return m == Modo::Altura ? classificar(cm()) : crit; }
};

// ---------------------------------------------------------------- jogo
class Jogo {
public:
    // Define o conjunto de rodadas disponíveis (embaralhado a cada partida).
    void definirRodadas(std::vector<Rodada> r) { pool_ = std::move(r); }

    void atualizar(float dt) { tempoFase_ += dt; avancarSePrecisar(); }

    void apertar(Botao b) {
        switch (fase_) {
            case Fase::Inicio:
                if (corDoBotao(b) == 1) iniciar(Modo::Altura);
                else if (corDoBotao(b) == 2) iniciar(Modo::Criticidade);
                break;
            case Fase::Jogando: jogar(b); break;
            case Fase::Revelacao:
                if (tempoFase_ >= TEMPO_MIN_REVELACAO) proximaRodada();
                break;
            case Fase::Vencedor:
                if (tempoFase_ >= TEMPO_MIN_REVELACAO) voltarAoInicio();
                break;
        }
    }

    // ---- leitura para a interface
    Fase fase() const { return fase_; }
    Modo modo() const { return modo_; }
    int rodadaAtual() const { return indice_; }
    int totalRodadas() const { return static_cast<int>(rodadas_.size()); }
    const Rodada& rodada() const { return rodadas_.at(static_cast<size_t>(indice_)); }
    const Resposta& resposta(int j) const { return resp_[j]; }
    int placar(int j) const { return placar_[j]; }
    bool ganhouRodada(int j) const { return ganhou_[j]; }
    bool empatouRodada() const { return empate_; }  // Altura: os dois erraram pela mesma distância
    float tempoFase() const { return tempoFase_; }
    // -1 = empate, 0 ou 1 = jogador vencedor (valem na fase Vencedor)
    int vencedor() const {
        if (placar_[0] == placar_[1]) return -1;
        return placar_[0] > placar_[1] ? 0 : 1;
    }
    // Contador que muda sempre que a interface precisa reagir (ex.: confete).
    int eventoRevelacao() const { return eventoRev_; }

private:
    std::mt19937 rng_{std::random_device{}()};
    std::vector<Rodada> pool_, rodadas_;
    Fase fase_ = Fase::Inicio;
    Modo modo_ = Modo::Altura;
    int indice_ = 0, eventoRev_ = 0;
    Resposta resp_[2];
    int placar_[2] = {0, 0};
    bool ganhou_[2] = {false, false};
    bool empate_ = false;
    float tempoFase_ = 0.0f;

    void mudarFase(Fase f) { fase_ = f; tempoFase_ = 0.0f; }

    void iniciar(Modo m) {
        if (pool_.empty()) return;
        modo_ = m;
        // só as fotos deste desafio; se nenhuma foi marcada para ele, usa todas
        const int bit = m == Modo::Altura ? PARA_ALTURA : PARA_CRITICIDADE;
        rodadas_.clear();
        for (const Rodada& r : pool_)
            if (r.desafios & bit) rodadas_.push_back(r);
        if (rodadas_.empty()) rodadas_ = pool_;
        std::shuffle(rodadas_.begin(), rodadas_.end(), rng_);
        if (static_cast<int>(rodadas_.size()) > RODADAS_POR_PARTIDA)
            rodadas_.resize(RODADAS_POR_PARTIDA);
        placar_[0] = placar_[1] = 0;
        indice_ = 0;
        novaRodada();
        mudarFase(Fase::Jogando);
    }

    void novaRodada() {
        resp_[0] = Resposta{};
        resp_[1] = Resposta{};
        ganhou_[0] = ganhou_[1] = false;
        empate_ = false;
    }

    void jogar(Botao b) {
        const int cor = corDoBotao(b);
        const int j = jogadorDoBotao(b);
        Resposta& r = resp_[j];
        if (modo_ == Modo::Altura) {
            // Cada um monta o seu número ao mesmo tempo e confirma com o Enter. Quem confirma fica
            // travado; a rodada só termina quando os dois confirmaram.
            if (r.travada) return;
            if (cor == 1) r.dezena = (r.dezena + 1) % 10;
            else if (cor == 2) r.unidade = (r.unidade + 1) % 10;
            else {
                r.travada = true;
                if (resp_[0].travada && resp_[1].travada) avaliar();
            }
        } else {
            // Criticidade é uma corrida: o primeiro clique de qualquer jogador é a resposta final
            // dele e o outro perde a vez nesta rodada.
            r.crit = cor == 1 ? Crit::Ok : cor == 2 ? Crit::Alerta : Crit::Critico;
            r.respondeuCrit = true;
            r.travada = true;
            resp_[1 - j].perdeuVez = true;
            avaliar();
        }
    }

    void avaliar() {
        const Rodada& r = rodada();
        if (modo_ == Modo::Altura) {
            int d0 = std::abs(resp_[0].cm() - r.alturaCm);
            int d1 = std::abs(resp_[1].cm() - r.alturaCm);
            ganhou_[0] = d0 < d1;  // só o mais perto pontua; empate: ninguém
            ganhou_[1] = d1 < d0;
            empate_ = d0 == d1;
        } else {
            // só pontua quem respondeu (o outro perdeu a vez); errou = ninguém pontua
            for (int j = 0; j < 2; ++j) ganhou_[j] = resp_[j].respondeuCrit && resp_[j].crit == r.crit();
        }
        for (int j = 0; j < 2; ++j) placar_[j] += ganhou_[j] ? 1 : 0;
        ++eventoRev_;
        mudarFase(Fase::Revelacao);
    }

    void proximaRodada() {
        if (indice_ + 1 >= totalRodadas()) { mudarFase(Fase::Vencedor); return; }
        ++indice_;
        novaRodada();
        mudarFase(Fase::Jogando);
    }

    void voltarAoInicio() { mudarFase(Fase::Inicio); }

    void avancarSePrecisar() {
        if (fase_ == Fase::Revelacao && tempoFase_ >= TEMPO_AUTO_REVELACAO) proximaRodada();
        else if (fase_ == Fase::Vencedor && tempoFase_ >= TEMPO_AUTO_VENCEDOR) voltarAoInicio();
    }
};

}  // namespace planta
