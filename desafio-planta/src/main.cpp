// Desafio EcoTrack - interface dos dois minigames.
// Recebe os botões do ESP32 pela USB (linhas BTN1..BTN6) ou do teclado.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "game.hpp"
#include "raylib.h"
#include "rlgl.h"
#include "ble.hpp"
#include "serial.hpp"

using namespace planta;

namespace {

constexpr int W = 1280;  // resolução lógica; o conteúdo escala para caber em qualquer tamanho de janela
constexpr int H = 720;

// Paleta: clima de rodovia em dia de sol (céu, morros verdes, asfalto) com acento roxo.
const Color CEU_TOPO{120, 184, 234, 255};
const Color CEU_BAIXO{216, 238, 250, 255};
const Color MORRO_LONGE{92, 152, 112, 255};
const Color MORRO_PERTO{58, 140, 74, 255};
const Color GRAMA_TOPO{116, 192, 86, 255};
const Color GRAMA_BAIXO{56, 144, 60, 255};
const Color ASFALTO{76, 82, 92, 255};
const Color ASFALTO_ESC{40, 45, 54, 255};
const Color FAIXA{244, 244, 240, 255};
const Color ROXO{104, 52, 238, 255};
const Color ROXO_ESC{66, 28, 176, 255};
const Color TEAL{0, 140, 168, 255};
const Color LED{255, 198, 58, 255};
const Color NAVY{20, 30, 58, 255};
const Color CARTAO{255, 255, 255, 255};
const Color CARTAO_BORDA{190, 200, 218, 255};
const Color BOTAO_INICIAR{30, 36, 54, 255};
const Color VERDE{63, 174, 73, 255};
const Color VERDE_ESC{36, 128, 52, 255};
const Color AMARELO{236, 186, 24, 255};
const Color VERMELHO{228, 44, 48, 255};
const Color BRANCO{250, 250, 250, 255};

// Cor de cada botão do controle: verde, amarelo e Enter (branco).
Color CorBotao(int i) { return i == 0 ? VERDE : i == 1 ? AMARELO : BRANCO; }
Color CorCrit(Crit c) { return c == Crit::Ok ? VERDE : c == Crit::Alerta ? AMARELO : VERMELHO; }

// ------------------------------------------------------------------ arquivos
std::string CaminhoAsset(const std::string& rel) {
    std::string a = std::string(GetApplicationDirectory()) + "assets/" + rel;
    if (FileExists(a.c_str())) return a;
    return "assets/" + rel;  // também funciona rodando da pasta do projeto
}

std::string Aparar(std::string s) {
    const char* esp = " \t\r\n";
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF) s.erase(0, 3);  // BOM
    size_t a = s.find_first_not_of(esp);
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(esp);
    return s.substr(a, b - a + 1);
}

// assets/rodadas.csv: "arquivo.jpg;altura_em_cm;desafio" (arquivo dentro de assets/imagens/;
// desafio = altura, criticidade ou ambos, e pode faltar).
std::vector<Rodada> CarregarRodadas() {
    std::vector<Rodada> v;
    std::ifstream in(CaminhoAsset("rodadas.csv"));
    std::string linha;
    while (std::getline(in, linha)) {
        linha = Aparar(linha);
        if (linha.empty() || linha[0] == '#') continue;
        size_t pos = linha.find(';');
        if (pos == std::string::npos) pos = linha.find(',');
        if (pos == std::string::npos) continue;
        std::string arq = Aparar(linha.substr(0, pos));
        std::string alt = Aparar(linha.substr(pos + 1));
        int desafios = PARA_AMBOS;
        size_t pos2 = alt.find_first_of(";,");
        if (pos2 != std::string::npos) {
            std::string d = Aparar(alt.substr(pos2 + 1));
            alt = Aparar(alt.substr(0, pos2));
            for (char& c : d) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (d.rfind("alt", 0) == 0) desafios = PARA_ALTURA;
            else if (d.rfind("crit", 0) == 0) desafios = PARA_CRITICIDADE;
        }
        if (arq.empty() || alt.empty() || alt[0] < '0' || alt[0] > '9') continue;
        v.push_back({"imagens/" + arq, std::atoi(alt.c_str()), desafios});
    }
    if (v.empty()) {  // modo demonstração: as imagens que não existem viram desenhos de teste
        v = {{"imagens/exemplo_27cm.jpg", 27}, {"imagens/demo_8cm.jpg", 8},
             {"imagens/demo_14cm.jpg", 14},    {"imagens/demo_33cm.jpg", 33},
             {"imagens/demo_42cm.jpg", 42},    {"imagens/demo_22cm.jpg", 22}};
    }
    return v;
}

// Porta serial: 1º argumento, ou primeira linha útil de assets/porta.txt.
// Sem porta, o ESP32 chega por Bluetooth. "--teclado" = sem ESP32.
std::string LerPorta(int argc, char** argv) {
    if (argc > 1) return std::string(argv[1]) == "--teclado" ? "" : argv[1];
    std::ifstream in(CaminhoAsset("porta.txt"));
    std::string linha;
    while (std::getline(in, linha)) {
        linha = Aparar(linha);
        if (!linha.empty() && linha[0] != '#') return linha;
    }
    return "";
}

// ------------------------------------------------------------------ texto
Font gFonte;
bool gFontePadrao = true;

float Esp(float size) { return gFontePadrao ? size / 10.0f : size * 0.01f; }

float LarguraTexto(const std::string& t, float size) {
    return MeasureTextEx(gFonte, t.c_str(), size, Esp(size)).x;
}

// alinh: 0 = centralizado em x, -1 = começa em x, 1 = termina em x
void Texto(const std::string& t, float x, float y, float size, Color c, int alinh = 0) {
    float w = LarguraTexto(t, size);
    float px = alinh == 0 ? x - w / 2 : alinh < 0 ? x : x - w;
    DrawTextEx(gFonte, t.c_str(), {px, y}, size, Esp(size), c);
    if (gFontePadrao && size >= 30)  // a fonte padrão é fina; um segundo traço dá peso
        DrawTextEx(gFonte, t.c_str(), {px + size * 0.03f, y}, size, Esp(size), c);
}

void CarregarFonte() {
    // 1) assets/fonts/fonte.ttf, 2) fonte arredondada do Windows (parecida com a do PDF), 3) padrão do raylib
    const std::string candidatas[] = {CaminhoAsset("fonts/fonte.ttf"), "C:/Windows/Fonts/ARLRDBD.TTF",
                                      "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/trebucbd.ttf"};
    std::vector<int> cps;
    for (int i = 32; i < 256; ++i) cps.push_back(i);  // inclui acentos do português
    for (const auto& p : candidatas) {
        if (!FileExists(p.c_str())) continue;
        gFonte = LoadFontEx(p.c_str(), 128, cps.data(), static_cast<int>(cps.size()));
        if (gFonte.texture.id != 0) {
            GenTextureMipmaps(&gFonte.texture);
            SetTextureFilter(gFonte.texture, TEXTURE_FILTER_TRILINEAR);
            gFontePadrao = false;
            return;
        }
    }
    gFonte = GetFontDefault();
    SetTextureFilter(gFonte.texture, TEXTURE_FILTER_BILINEAR);
    gFontePadrao = true;
}

// ------------------------------------------------------------------ desenho
void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cruz = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cruz > 0) DrawTriangle(a, c, b, col);
    else DrawTriangle(a, b, c, col);
}

// ------------------------------------------------------------------ cenário de rodovia
// Uma câmera de rodovia em dia de sol: céu, morros verdes, viaduto, pista em perspectiva com carros
// e caminhões indo embora. Tudo desenhado por código em coordenadas lógicas (1280x720).
struct Cena {
    float ox, oy, k;
    Vector2 p(float x, float y) const { return {ox + x * k, oy + y * k}; }
};

void Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) {
    Tri(a, b, c, col);
    Tri(a, c, d, col);
}

// Pista em perspectiva: f = 0 no horizonte, f = 1 perto da base da tela.
constexpr float HORIZONTE = 200.0f;
constexpr float FUGA_X = 900.0f;
constexpr float BASE_Y = 940.0f;
constexpr float BASE_X = 520.0f;
constexpr float BASE_MEIA = 380.0f;  // meia largura da pista em f = 1

struct PontoPista { float x, y, meia; };

PontoPista NaPista(float f) {
    return {FUGA_X + (BASE_X - FUGA_X) * f, HORIZONTE + (BASE_Y - HORIZONTE) * f, 4.0f + (BASE_MEIA - 4.0f) * f};
}

// Faixa ao longo da pista entre f0 e f1, centrada em 'lado' (-1 a 1 = borda a borda) e com 'larg'
// de meia largura (ambos como fração da meia largura da pista).
void FaixaDaPista(const Cena& c, float f0, float f1, float lado, float larg, Color col) {
    PontoPista a = NaPista(f0), b = NaPista(f1);
    Quad(c.p(a.x + (lado - larg) * a.meia, a.y), c.p(a.x + (lado + larg) * a.meia, a.y),
         c.p(b.x + (lado + larg) * b.meia, b.y), c.p(b.x + (lado - larg) * b.meia, b.y), col);
}

struct Veiculo {
    float lane;  // -0.667, 0 ou 0.667: centro de cada uma das 3 faixas
    float vel;   // voltas da pista por segundo
    float fase;
    int tipo;    // 0 = carro, 1 = caminhão baú
    Color cor;
};

const Veiculo VEICULOS[] = {
    {-0.667f, 0.040f, 0.00f, 0, {244, 244, 246, 255}},  // carro branco
    {0.000f, 0.028f, 0.30f, 1, {238, 238, 242, 255}},   // caminhão branco
    {0.667f, 0.055f, 0.62f, 0, {52, 64, 96, 255}},      // carro escuro
    {-0.667f, 0.032f, 0.78f, 1, {232, 236, 240, 255}},  // outro caminhão
    {0.000f, 0.048f, 0.90f, 0, {196, 52, 56, 255}},     // carro vermelho
};

// Veículo visto de trás, indo para o horizonte; encolhe e some no fundo.
void DesenharVeiculo(const Cena& c, const Veiculo& v, float t) {
    const float s = 1.0f - std::fmod(t * v.vel + v.fase, 1.0f);
    const float f = s * s;
    if (f < 0.03f) return;
    const float a = std::min(1.0f, (1.0f - s) * 14.0f) * std::min(1.0f, f * 10.0f);
    const PontoPista pp = NaPista(f);
    const float larguraFaixa = pp.meia * 0.667f;
    const float w = larguraFaixa * (v.tipo == 1 ? 0.56f : 0.5f) * c.k;
    const float h = v.tipo == 1 ? w * 1.05f : w * 0.62f;
    const Vector2 b = c.p(pp.x + v.lane * pp.meia, pp.y);  // ponto de contato com o asfalto
    DrawEllipse(static_cast<int>(b.x), static_cast<int>(b.y), w * 0.62f, w * 0.10f, Fade(BLACK, 0.28f * a));
    const Color luz = Fade(Color{255, 70, 60, 255}, a);
    if (v.tipo == 0) {
        DrawRectangleRounded({b.x - w / 2, b.y - h, w, h}, 0.3f, 6, Fade(v.cor, a));
        DrawRectangleRounded({b.x - w * 0.36f, b.y - h * 1.5f, w * 0.72f, h * 0.7f}, 0.4f, 6, Fade(v.cor, a));
        DrawRectangleRounded({b.x - w * 0.30f, b.y - h * 1.42f, w * 0.60f, h * 0.45f}, 0.3f, 6,
                             Fade(Color{40, 56, 78, 255}, a));
        DrawRectangleRec({b.x - w * 0.46f, b.y - h * 0.78f, w * 0.16f, h * 0.22f}, luz);
        DrawRectangleRec({b.x + w * 0.30f, b.y - h * 0.78f, w * 0.16f, h * 0.22f}, luz);
    } else {
        DrawRectangleRounded({b.x - w / 2, b.y - h * 1.15f, w, h * 1.0f}, 0.06f, 6, Fade(v.cor, a));
        DrawRectangleRec({b.x - w / 2, b.y - h * 0.22f, w, h * 0.12f}, Fade(Color{70, 76, 86, 255}, a));
        DrawRectangleRec({b.x - w * 0.46f, b.y - h * 0.34f, w * 0.12f, h * 0.10f}, luz);
        DrawRectangleRec({b.x + w * 0.34f, b.y - h * 0.34f, w * 0.12f, h * 0.10f}, luz);
    }
}

void Palmeira(const Cena& c, float x, float yBase, float altura) {
    const Vector2 base = c.p(x, yBase), topo = c.p(x + altura * 0.12f, yBase - altura);
    DrawLineEx(base, topo, 7 * c.k, Color{112, 86, 54, 255});
    for (int i = 0; i < 7; ++i) {
        const float ang = (-160.0f + i * 50.0f) * DEG2RAD;  // leque de folhas
        const Vector2 ponta{topo.x + std::cos(ang) * altura * 0.42f * c.k,
                            topo.y + (std::sin(ang) * altura * 0.30f + altura * 0.10f) * c.k};
        DrawLineEx(topo, ponta, 6 * c.k, Color{38, 116, 54, 255});
    }
}

void Nuvem(const Cena& c, float x, float y, float e) {
    const Vector2 p = c.p(x, y);
    const Color nuv = Fade(WHITE, 0.86f);
    DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), 70 * e * c.k, 16 * e * c.k, nuv);
    DrawEllipse(static_cast<int>(p.x + 40 * e * c.k), static_cast<int>(p.y - 8 * e * c.k), 46 * e * c.k,
                16 * e * c.k, nuv);
    DrawEllipse(static_cast<int>(p.x - 42 * e * c.k), static_cast<int>(p.y + 3 * e * c.k), 40 * e * c.k,
                12 * e * c.k, nuv);
}

// Fundo no tamanho da janela; o cenário se escala e se centraliza como o resto do conteúdo.
void FundoDecorado(float sw, float sh, float k) {
    const Cena c{(sw - W * k) / 2, (sh - H * k) / 2, k};
    const float t = static_cast<float>(GetTime());
    const float hor = c.p(0, HORIZONTE).y;

    DrawRectangleGradientV(0, 0, static_cast<int>(sw), static_cast<int>(hor) + 2, CEU_TOPO, CEU_BAIXO);
    for (int i = 0; i < 4; ++i)
        Nuvem(c, std::fmod(t * (4.0f + i) + i * 380.0f, W + 700.0f) - 350.0f, 30.0f + i * 20.0f, 0.8f + 0.12f * i);

    // morros ao longe e mais perto
    struct Morro { float x, rx, ry; };
    DrawRectangle(0, static_cast<int>(hor - 18 * k), static_cast<int>(sw), static_cast<int>(26 * k), MORRO_LONGE);
    const Morro longe[] = {{-420, 520, 120}, {-120, 460, 110}, {420, 380, 88}, {880, 470, 112}, {1350, 420, 100}, {1750, 520, 118}};
    for (const auto& m : longe) {
        const Vector2 p = c.p(m.x, HORIZONTE + 24);
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), m.rx * k, m.ry * k, MORRO_LONGE);
    }
    const Morro perto[] = {{-200, 400, 70}, {140, 330, 66}, {700, 360, 54}, {1180, 340, 72}, {1600, 420, 64}};
    for (const auto& m : perto) {
        const Vector2 p = c.p(m.x, HORIZONTE + 30);
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), m.rx * k, m.ry * k, MORRO_PERTO);
    }

    // chão de grama
    DrawRectangleGradientV(0, static_cast<int>(hor), static_cast<int>(sw), static_cast<int>(sh - hor) + 2,
                           GRAMA_TOPO, GRAMA_BAIXO);

    // viaduto à esquerda, como na câmera da rodovia
    DrawLineEx(c.p(-320, 262), c.p(430, 196), 22 * k, Color{150, 156, 164, 255});
    DrawLineEx(c.p(-320, 250), c.p(430, 184), 4 * k, Color{228, 232, 236, 255});
    for (float x = -260; x < 430; x += 110) {
        const float yv = 262 + (196 - 262) * ((x + 320) / 750.0f);
        const Vector2 topo = c.p(x - 7, yv + 11);
        DrawRectangleV(topo, {14 * k, (300 - (yv + 11)) * k}, Color{120, 126, 134, 255});
    }

    // palmeiras e moitas ao lado da pista
    Palmeira(c, 1240, 262, 120);
    Palmeira(c, 1196, 252, 86);
    Palmeira(c, 60, 300, 100);
    const float moitas[][3] = {{160, 330, 26}, {240, 345, 20}, {1100, 340, 24}, {1160, 360, 30}, {1030, 320, 18}};
    for (const auto& m : moitas) {
        const Vector2 p = c.p(m[0], m[1]);
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), m[2] * 1.5f * k, m[2] * k, Color{44, 124, 60, 255});
    }

    // pista: acostamento, asfalto, bordas, guard-rail e faixas
    FaixaDaPista(c, 0.0f, 1.7f, 0.0f, 1.10f, Color{122, 128, 134, 255});
    FaixaDaPista(c, 0.0f, 1.7f, 0.0f, 1.0f, ASFALTO);
    FaixaDaPista(c, 0.0f, 1.7f, -0.965f, 0.022f, FAIXA);
    FaixaDaPista(c, 0.0f, 1.7f, 0.965f, 0.022f, FAIXA);
    FaixaDaPista(c, 0.0f, 1.7f, -1.13f, 0.035f, Color{206, 211, 218, 255});
    FaixaDaPista(c, 0.0f, 1.7f, 1.13f, 0.035f, Color{206, 211, 218, 255});
    for (int i = 1; i < 19; ++i) {
        const float s0 = i / 14.0f, s1 = (i + 0.45f) / 14.0f;
        FaixaDaPista(c, s0 * s0, s1 * s1, -0.333f, 0.012f, FAIXA);
        FaixaDaPista(c, s0 * s0, s1 * s1, 0.333f, 0.012f, FAIXA);
    }

    // os de trás primeiro (quem está mais longe tem menor f)
    const Veiculo* ordem[std::size(VEICULOS)];
    for (size_t i = 0; i < std::size(VEICULOS); ++i) ordem[i] = &VEICULOS[i];
    std::sort(ordem, ordem + std::size(VEICULOS), [t](const Veiculo* a, const Veiculo* b) {
        return std::fmod(t * a->vel + a->fase, 1.0f) > std::fmod(t * b->vel + b->fase, 1.0f);
    });
    for (const Veiculo* v : ordem) DesenharVeiculo(c, *v, t);
}

void Rosto(float cx, float cy, float r, int humor, bool festa = false) {
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), r, Color{255, 206, 56, 255});
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r, Color{235, 165, 20, 255});
    Color tinta{90, 60, 20, 255};
    DrawCircle(static_cast<int>(cx - r * 0.35f), static_cast<int>(cy - r * 0.2f), r * 0.09f, tinta);
    DrawCircle(static_cast<int>(cx + r * 0.35f), static_cast<int>(cy - r * 0.2f), r * 0.09f, tinta);
    if (humor > 0)
        DrawRing({cx, cy + r * 0.02f}, r * 0.40f, r * 0.50f, 25, 155, 24, tinta);
    else if (humor < 0) {
        DrawRing({cx, cy + r * 0.72f}, r * 0.30f, r * 0.40f, 205, 335, 24, tinta);
        DrawCircle(static_cast<int>(cx + r * 0.78f), static_cast<int>(cy - r * 0.45f), r * 0.14f,
                   Color{110, 180, 250, 255});
    } else
        DrawRectangle(static_cast<int>(cx - r * 0.3f), static_cast<int>(cy + r * 0.38f),
                      static_cast<int>(r * 0.6f), static_cast<int>(r * 0.1f), tinta);
    if (festa) {
        Tri({cx + r * 0.05f, cy - r * 1.05f}, {cx + r * 0.75f, cy - r * 0.45f},
            {cx + r * 0.15f, cy - r * 0.62f}, Color{240, 110, 120, 255});
        Tri({cx + r * 0.05f, cy - r * 1.05f}, {cx - r * 0.2f, cy - r * 0.55f},
            {cx + r * 0.15f, cy - r * 0.62f}, Color{255, 200, 205, 255});
        DrawCircle(static_cast<int>(cx + r * 0.05f), static_cast<int>(cy - r * 1.05f), r * 0.1f,
                   Color{200, 60, 60, 255});
    }
}

// Painel branco translúcido: dá leitura ao texto por cima do cenário.
void Painel(Rectangle r, float alfa = 0.9f, float arred = 0.25f) {
    DrawRectangleRounded({r.x + 3, r.y + 6, r.width, r.height}, arred, 12, Fade(BLACK, 0.16f));
    DrawRectangleRounded(r, arred, 12, Fade(WHITE, alfa));
}

// Texto numa pílula branca, centralizado em (cx, cy).
void Faixa(const std::string& t, float cx, float cy, float size, Color cor = NAVY) {
    float w = LarguraTexto(t, size) + 56;
    Painel({cx - w / 2, cy - size * 0.95f, w, size * 1.9f}, 0.88f, 0.5f);
    Texto(t, cx, cy - size * 0.55f, size, cor);
}

// Título "Desafio Ecotrack": placa roxa que se ajusta ao texto, com faixas de pista nas pontas.
void Banner(float cx, float cy, float e) {
    float s1 = 84 * e;
    float w1 = LarguraTexto("Desafio", s1), w2 = LarguraTexto("Ecotrack", s1);
    float gap = 22 * e, pad = 52 * e;
    float larg = w1 + gap + w2 + 2 * pad, alt = 118 * e;
    Rectangle pill{cx - larg / 2, cy - alt / 2, larg, alt};
    DrawRectangleRounded({pill.x + 4 * e, pill.y + 8 * e, larg, alt}, 0.3f, 24, Fade(BLACK, 0.20f));
    DrawRectangleRounded(pill, 0.3f, 24, ROXO);
    DrawRectangleRoundedLinesEx({pill.x + 6 * e, pill.y + 6 * e, larg - 12 * e, alt - 12 * e}, 0.3f, 24, 2.5f * e,
                                Fade(WHITE, 0.55f));
    float x = pill.x + pad, y = cy - s1 * 0.55f;
    Texto("Desafio", x, y, s1, WHITE, -1);
    Texto("Ecotrack", x + w1 + gap, y, s1, Color{255, 214, 92, 255}, -1);
    // três tracinhos de faixa de pista em cada ponta
    for (int i = 0; i < 3; ++i) {
        float dx = (12 + i * 20) * e, h = (5 - i) * e;
        DrawRectangleRounded({pill.x - 18 * e - dx, cy - h / 2, 14 * e, h}, 0.5f, 4, Fade(WHITE, 0.95f - i * 0.2f));
        DrawRectangleRounded({pill.x + larg + 4 * e + dx, cy - h / 2, 14 * e, h}, 0.5f, 4,
                             Fade(WHITE, 0.95f - i * 0.2f));
    }
}

// Selo no canto, como o da câmera: placa branca com a logo.
void LogoPlaca(const Texture2D& logo, bool temLogo) {
    float e = temLogo ? 56.0f / logo.height : 1.0f;
    float lw = temLogo ? logo.width * e : LarguraTexto("ECOTRACK", 40);
    Rectangle p{24, 14, lw + 36, 76};
    DrawRectangleRounded({p.x + 3, p.y + 6, p.width, p.height}, 0.2f, 10, Fade(BLACK, 0.18f));
    DrawRectangleRounded(p, 0.2f, 10, WHITE);
    DrawRectangleRoundedLinesEx(p, 0.2f, 10, 4, ROXO);
    if (temLogo) DrawTextureEx(logo, {p.x + 18, p.y + 10}, 0, e, WHITE);
    else Texto("ECOTRACK", p.x + 18, p.y + 16, 40, NAVY, -1);
}

void Visto(float cx, float cy, float s, Color c) {
    DrawLineEx({cx - s * 0.5f, cy}, {cx - s * 0.15f, cy + s * 0.4f}, s * 0.18f, c);
    DrawLineEx({cx - s * 0.15f, cy + s * 0.4f}, {cx + s * 0.55f, cy - s * 0.5f}, s * 0.18f, c);
}

void Xis(float cx, float cy, float s, Color c) {
    DrawLineEx({cx - s * 0.4f, cy - s * 0.4f}, {cx + s * 0.4f, cy + s * 0.4f}, s * 0.16f, c);
    DrawLineEx({cx + s * 0.4f, cy - s * 0.4f}, {cx - s * 0.4f, cy + s * 0.4f}, s * 0.16f, c);
}

// ------------------------------------------------------------------ foto da rodada
struct Foto {
    Texture2D tex{};
    bool tem = false;
    std::string chave;
    int altura = 0;
    unsigned semente = 1;
};

void CarregarFoto(Foto& f, const Rodada& r) {
    std::string chave = r.imagem + ":" + std::to_string(r.alturaCm);
    if (chave == f.chave) return;
    if (f.tem) UnloadTexture(f.tex);
    f.tem = false;
    f.chave = chave;
    f.altura = r.alturaCm;
    f.semente = static_cast<unsigned>(std::hash<std::string>{}(chave));
    std::string p = CaminhoAsset(r.imagem);
    if (FileExists(p.c_str())) {
        Texture2D t = LoadTexture(p.c_str());
        if (t.id != 0) {
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
            f.tex = t;
            f.tem = true;
        }
    }
}

// Desenho de teste quando a imagem não existe: o comprimento das folhas segue a altura em cm.
void GramaDeTeste(const Foto& f, Rectangle d) {
    DrawRectangleRec(d, Color{52, 108, 46, 255});
    std::mt19937 g(f.semente);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    float base = d.y + d.height;
    for (int i = 0; i < 170; ++i) {
        float x = d.x + 4 + u(g) * (d.width - 8);
        float len = std::min(d.height - 6, (f.altura / 50.0f) * d.height * (0.55f + 0.45f * u(g)));
        float lean = (u(g) - 0.5f) * 0.35f * len;
        float xf = std::clamp(x + lean, d.x + 2, d.x + d.width - 2);
        Color c{static_cast<unsigned char>(40 + u(g) * 60), static_cast<unsigned char>(130 + u(g) * 90),
                static_cast<unsigned char>(30 + u(g) * 50), 255};
        DrawLineEx({x, base}, {xf, base - len}, 2.5f + u(g) * 2.0f, c);
    }
    Texto("imagem de teste", d.x + d.width / 2, d.y + 8, 20, Color{255, 255, 255, 200});
}

void DesenharFoto(const Foto& f, Rectangle moldura) {
    DrawRectangleRounded({moldura.x + 3, moldura.y + 6, moldura.width, moldura.height}, 0.06f, 10,
                         Fade(BLACK, 0.10f));
    DrawRectangleRounded(moldura, 0.06f, 10, WHITE);
    Rectangle d{moldura.x + 8, moldura.y + 8, moldura.width - 16, moldura.height - 16};
    if (!f.tem) { GramaDeTeste(f, d); return; }
    float ta = d.width / d.height;
    float ia = static_cast<float>(f.tex.width) / static_cast<float>(f.tex.height);
    Rectangle src{0, 0, static_cast<float>(f.tex.width), static_cast<float>(f.tex.height)};
    if (ia > ta) { src.width = src.height * ta; src.x = (f.tex.width - src.width) / 2; }
    else { src.height = src.width / ta; src.y = (f.tex.height - src.height) / 2; }
    DrawTexturePro(f.tex, src, d, {0, 0}, 0, WHITE);
}

// ------------------------------------------------------------------ confete
struct Confete { float x, y, vx, vy, rot, vrot, tam; Color cor; };
std::vector<Confete> gConfete;

void SpawnConfete(float x0, float x1, int n) {
    static std::mt19937 g(12345);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const Color cores[] = {{255, 80, 80, 255},  {255, 160, 40, 255}, {255, 215, 60, 255},
                           {60, 200, 120, 255}, {70, 170, 255, 255}, {200, 90, 255, 255},
                           {255, 90, 190, 255}};
    for (int i = 0; i < n && gConfete.size() < 700; ++i)
        gConfete.push_back({x0 + u(g) * (x1 - x0), -20 - u(g) * 160, (u(g) - 0.5f) * 120, 140 + u(g) * 220,
                            u(g) * 360, (u(g) - 0.5f) * 600, 8 + u(g) * 10, cores[g() % 7]});
}

void AtualizarConfete(float dt) {
    for (auto& c : gConfete) { c.x += c.vx * dt; c.y += c.vy * dt; c.rot += c.vrot * dt; }
    gConfete.erase(std::remove_if(gConfete.begin(), gConfete.end(), [](const Confete& c) { return c.y > H + 30; }),
                   gConfete.end());
}

void DesenharConfete() {
    for (const auto& c : gConfete)
        DrawRectanglePro({c.x, c.y, c.tam, c.tam * 0.5f}, {c.tam / 2, c.tam / 4}, c.rot, c.cor);
}

// ------------------------------------------------------------------ cartão do jogador
void Legenda(Rectangle r, const char* a, const char* b, const char* c, float alfa) {
    const char* nomes[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        float cx = r.x + r.width * (2 * i + 1) / 6.0f;
        DrawCircle(static_cast<int>(cx), static_cast<int>(r.y + r.height - 62), 12, Fade(ASFALTO_ESC, alfa));
        DrawCircle(static_cast<int>(cx), static_cast<int>(r.y + r.height - 62), 9, Fade(CorBotao(i), alfa));
        Texto(nomes[i], cx, r.y + r.height - 44, 22, Fade(NAVY, alfa));
    }
}

// Lâmpada de semáforo: carcaça escura, vidro colorido e um brilho.
void Lampada(Vector2 ctr, Color cor, bool acesa) {
    DrawCircleV(ctr, 78, ASFALTO_ESC);
    DrawCircleV(ctr, 66, acesa ? cor : Color{86, 94, 108, 255});
    if (acesa) {
        DrawCircleLines(static_cast<int>(ctr.x), static_cast<int>(ctr.y), 72, Fade(cor, 0.55f));
        DrawEllipse(static_cast<int>(ctr.x - 20), static_cast<int>(ctr.y - 26), 20, 11, Fade(WHITE, 0.40f));
    }
}

void Cartao(const Jogo& jogo, int j, Rectangle r) {
    const bool rev = jogo.fase() == Fase::Revelacao;
    const bool altura = jogo.modo() == Modo::Altura;
    // Nos dois modos os jogadores jogam ao mesmo tempo, cada um com o seu controle.
    const bool vez = jogo.fase() == Fase::Jogando;
    const Resposta& rp = jogo.resposta(j);
    const bool mascarada = jogo.fase() == Fase::Jogando && rp.travada;
    const bool perdeu = rev && rp.perdeuVez;  // o outro jogador apertou primeiro
    const bool empate = rev && jogo.empatouRodada();  // Altura: mesma distância, ninguém pontua

    Color fill = CARTAO, borda = CARTAO_BORDA, cab = j == 0 ? ROXO : TEAL;
    if (perdeu || empate) { fill = Color{226, 230, 238, 255}; borda = Color{150, 160, 175, 255}; cab = Color{118, 126, 142, 255}; }
    else if (rev) {
        if (jogo.ganhouRodada(j)) { fill = Color{222, 248, 214, 255}; borda = VERDE_ESC; cab = VERDE_ESC; }
        else { fill = Color{253, 224, 220, 255}; borda = VERMELHO; cab = Color{192, 40, 44, 255}; }
    }
    DrawRectangleRounded({r.x + 5, r.y + 10, r.width, r.height}, 0.08f, 12, Fade(BLACK, 0.20f));
    DrawRectangleRounded(r, 0.08f, 12, fill);
    // cabeçalho colorido, como uma placa
    DrawRectangleRounded({r.x, r.y, r.width, 76}, 0.4f, 12, cab);
    DrawRectangleRec({r.x, r.y + 40, r.width, 36}, cab);
    DrawRectangleRoundedLinesEx(r, 0.08f, 12, vez ? 6.0f : 3.0f, vez ? cab : borda);

    float cx = r.x + r.width / 2;
    Texto(TextFormat("JOGADOR %d", j + 1), cx, r.y + 17, 42, WHITE);

    // lâmpada + rótulo
    Vector2 ctr{cx, r.y + 172};
    if (mascarada) {
        Lampada(ctr, Color{150, 175, 205, 255}, true);
        Visto(ctr.x, ctr.y, 80, WHITE);
        Texto("Pronto!", cx, r.y + 258, 40, NAVY);
    } else if (!altura && !rp.respondeuCrit) {
        Lampada(ctr, WHITE, false);
        Texto("?", cx, ctr.y - 36, 72, Color{214, 222, 234, 255});
        const char* msg = perdeu ? "perdeu a vez" : vez ? "aperte rápido!" : "aguardando";
        Texto(msg, cx, r.y + 258, 36, Fade(NAVY, perdeu ? 0.75f : 0.55f));
    } else {
        Crit c = rp.critExibida(jogo.modo());
        Lampada(ctr, CorCrit(c), true);
        Texto(nomeCrit(c), cx, r.y + 258, 40, c == Crit::Alerta ? Color{176, 124, 0, 255} : CorCrit(c));
    }

    // visor de LED (como os painéis de mensagem da rodovia)
    Rectangle pill{r.x + 40, r.y + 316, r.width - 80, 86};
    DrawRectangleRounded(pill, 0.3f, 16, Color{20, 24, 32, 255});
    DrawRectangleRoundedLinesEx(pill, 0.3f, 16, 3, Color{74, 82, 96, 255});
    std::string txt;
    if (mascarada) txt = "?? cm";
    else if (altura) txt = TextFormat("%dcm", rp.cm());
    else txt = rev ? "-" : "?";
    Texto(txt, cx, pill.y + 9, 64, perdeu ? Color{110, 116, 128, 255} : LED);

    // rodapé
    if (vez && altura) Legenda(r, "dezena", "unidade", "confirmar", 1.0f);
    else if (vez) Legenda(r, "OK", "Alerta", "Crítico", 1.0f);
    else if (perdeu) {
        // sem ícone: o jogador não respondeu
    } else if (empate) {
        Texto("empate: sem ponto", cx, r.y + r.height - 56, 28, Color{90, 100, 120, 255});
    } else if (rev) {
        if (jogo.ganhouRodada(j)) Visto(cx, r.y + r.height - 52, 56, VERDE_ESC);
        else Xis(cx, r.y + r.height - 52, 56, Color{170, 20, 25, 255});
    }

    // carinhas da revelação
    if (rev) {
        float fx = j == 0 ? r.x + 8 : r.x + r.width - 8;
        Rosto(fx, r.y + 10, 42, perdeu || empate ? 0 : jogo.ganhouRodada(j) ? 1 : -1);
    }
}

// ------------------------------------------------------------------ telas
void TelaInicio(const Texture2D& logo, bool temLogo) {
    LogoPlaca(logo, temLogo);
    Banner(W / 2.0f, 200, 1.0f);
    Faixa("Você conhece a grama só de olhar?", W / 2.0f, 330, 42, ROXO_ESC);

    const char* titulo[2] = {"INICIAR", "INICIAR"};
    const char* sub[2] = {"Altura em cm", "Criticidade"};
    for (int i = 0; i < 2; ++i) {
        Rectangle p{210.0f + i * 460.0f, 420, 400, 120};
        DrawRectangleRounded({p.x + 4, p.y + 9, p.width, p.height}, 0.5f, 16, Fade(BLACK, 0.25f));
        DrawRectangleRounded(p, 0.5f, 16, BOTAO_INICIAR);
        DrawRectangleRoundedLinesEx(p, 0.5f, 16, 3, Fade(WHITE, 0.85f));
        DrawCircle(static_cast<int>(p.x + 62), static_cast<int>(p.y + 60), 28, ASFALTO_ESC);
        DrawCircle(static_cast<int>(p.x + 62), static_cast<int>(p.y + 60), 22, CorBotao(i));
        DrawEllipse(static_cast<int>(p.x + 55), static_cast<int>(p.y + 51), 9, 5, Fade(WHITE, 0.45f));
        Texto(titulo[i], p.x + 240, p.y + 20, 48, WHITE);
        Texto(sub[i], p.x + 240, p.y + 72, 30, Color{255, 214, 92, 255});
    }
    Faixa("aperte o botão verde ou o amarelo", W / 2.0f, 590, 24);
    Faixa("teclado: jogador 1 = 1 2 3   |   jogador 2 = 8 9 0", W / 2.0f, 648, 20, Color{70, 80, 104, 255});
}

void TelaJogo(const Jogo& jogo, const Foto& foto) {
    Banner(W / 2.0f, 62, 0.62f);

    // placar e rodada
    Painel({W / 2.0f - 110, 110, 220, 64}, 0.92f, 0.5f);
    Texto(TextFormat("%d - %d", jogo.placar(0), jogo.placar(1)), W / 2.0f, 114, 50, NAVY);
    std::string rod = TextFormat("Rodada %d/%d", jogo.rodadaAtual() + 1, jogo.totalRodadas());
    float rw = LarguraTexto(rod, 26) + 40;
    DrawRectangleRounded({28, 16, rw, 42}, 0.5f, 16, ROXO);
    Texto(rod, 28 + rw / 2, 23, 26, WHITE);

    Cartao(jogo, 0, {72, 178, 382, 500});
    Cartao(jogo, 1, {826, 178, 382, 500});
    DesenharFoto(foto, {492, 180, 296, 496});

    // etiqueta de câmera sobre a foto
    {
        std::string cam = TextFormat("CAM %02d", jogo.rodadaAtual() + 1);
        float cw = LarguraTexto(cam, 20) + 44;
        Rectangle e{512, 632, cw, 30};
        DrawRectangleRounded(e, 0.5f, 12, Fade(Color{20, 24, 32, 255}, 0.82f));
        DrawCircle(static_cast<int>(e.x + 18), static_cast<int>(e.y + 15), 6, VERMELHO);
        Texto(cam, e.x + cw - 12, e.y + 4, 20, WHITE, 1);
    }

    if (jogo.fase() == Fase::Revelacao) {
        float a = std::min(1.0f, jogo.tempoFase() / 0.35f);
        const Rodada& r = jogo.rodada();
        Rectangle box{526, 310, 228, 270};
        DrawRectangleRounded(box, 0.22f, 12, Fade(WHITE, 0.86f * a));
        DrawCircle(640, 372, 46 * (0.6f + 0.4f * a), Fade(CorCrit(r.crit()), a));
        Texto(nomeCrit(r.crit()), 640, 436, 40, Fade(CorCrit(r.crit()), a));
        Texto(TextFormat("%dcm", r.alturaCm), 640, 490, 60, Fade(NAVY, a));
        Faixa("aperte qualquer botão para continuar", W / 2.0f, 694, 22);
    } else if (jogo.modo() == Modo::Criticidade) {
        Faixa("Quem apertar primeiro responde. O outro perde a vez!", W / 2.0f, 694, 24);
    } else {
        Faixa("Montem o número e confirmem: os dois respondem!", W / 2.0f, 694, 24);
    }
}

void TelaVencedor(const Jogo& jogo, const Texture2D& logo, bool temLogo) {
    LogoPlaca(logo, temLogo);
    Banner(W / 2.0f, 200, 1.0f);
    Painel({130, 276, 1020, 372}, 0.9f, 0.1f);
    int v = jogo.vencedor();
    if (v >= 0) {
        Rosto(400, 470, 84, 1, true);
        Texto("Vencedor:", 800, 318, 62, ROXO);
        Texto(TextFormat("Jogador %d", v + 1), 800, 390, 140, NAVY);
    } else {
        Rosto(400, 470, 84, 0);
        Texto("Empate!", 800, 360, 150, NAVY);
    }
    Texto(TextFormat("Placar final: %d - %d", jogo.placar(0), jogo.placar(1)), W / 2.0f, 580, 38, NAVY);
    Faixa("aperte qualquer botão para jogar de novo", W / 2.0f, 690, 24);
}

void Status(bool ok, const std::string& t) {
    float w = LarguraTexto(t, 20);
    Rectangle p{W - 24 - w - 44, 16, w + 44, 40};
    DrawRectangleRounded({p.x + 2, p.y + 4, p.width, p.height}, 0.5f, 16, Fade(BLACK, 0.16f));
    DrawRectangleRounded(p, 0.5f, 16, Fade(WHITE, 0.92f));
    DrawCircle(static_cast<int>(p.x + 20), static_cast<int>(p.y + 20), 7, ok ? VERDE : Color{150, 160, 175, 255});
    Texto(t, p.x + p.width - 14, p.y + 9, 20, NAVY, 1);
}

}  // namespace

int main(int argc, char** argv) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(W, H, "Desafio EcoTrack");
    SetWindowMinSize(480, 270);
    SetTargetFPS(60);
    SetExitKey(KEY_ESCAPE);
    CarregarFonte();

    Texture2D logo{};
    bool temLogo = false;
    std::string pl = CaminhoAsset("logo.png");
    if (FileExists(pl.c_str())) {
        logo = LoadTexture(pl.c_str());
        GenTextureMipmaps(&logo);
        SetTextureFilter(logo, TEXTURE_FILTER_TRILINEAR);
        temLogo = logo.id != 0;
    }

    Jogo jogo;
    jogo.definirRodadas(CarregarRodadas());
    Foto foto;

    const std::string nomePorta = LerPorta(argc, argv);
    PortaSerial porta;
    float tentativa = 0.0f;
    if (!nomePorta.empty() && !porta.abrir(nomePorta))
        std::printf("Não consegui abrir a porta '%s'. Usando o teclado.\n", nomePorta.c_str());

    // Sem porta serial e sem --teclado: o ESP32 chega por Bluetooth (Windows).
    const bool soTeclado = argc > 1 && std::string(argv[1]) == "--teclado";
    ReceptorBLE ble;
    const bool usarBle = nomePorta.empty() && !soTeclado && ReceptorBLE::suportado();
    if (usarBle) ble.iniciar();

    int ultimoEvento = 0;
    Fase faseAnterior = Fase::Inicio;
    float emissor = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // --- entrada: teclado e ESP32
        // teclado: jogador 1 = 1 · 2 · 3, jogador 2 = 8 · 9 · 0
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) jogo.apertar(Botao::B1);
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) jogo.apertar(Botao::B2);
        if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) jogo.apertar(Botao::B3);
        if (IsKeyPressed(KEY_EIGHT) || IsKeyPressed(KEY_KP_8)) jogo.apertar(Botao::B4);
        if (IsKeyPressed(KEY_NINE) || IsKeyPressed(KEY_KP_9)) jogo.apertar(Botao::B5);
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) jogo.apertar(Botao::B6);
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (!nomePorta.empty() && !porta.aberta()) {  // tenta reconectar a cada 2 s
            tentativa += dt;
            if (tentativa > 2.0f) { tentativa = 0; porta.abrir(nomePorta); }
        }
        std::string linha;
        auto tratar = [&jogo](const std::string& l) {
            if (l == "BTN1") jogo.apertar(Botao::B1);
            else if (l == "BTN2") jogo.apertar(Botao::B2);
            else if (l == "BTN3") jogo.apertar(Botao::B3);
            else if (l == "BTN4") jogo.apertar(Botao::B4);
            else if (l == "BTN5") jogo.apertar(Botao::B5);
            else if (l == "BTN6") jogo.apertar(Botao::B6);
        };
        while (porta.lerLinha(linha)) tratar(linha);
        while (usarBle && ble.lerLinha(linha)) tratar(linha);

        jogo.atualizar(dt);

        // --- reações visuais
        if (jogo.fase() == Fase::Jogando || jogo.fase() == Fase::Revelacao)
            CarregarFoto(foto, jogo.rodada());
        if (jogo.eventoRevelacao() != ultimoEvento) {
            ultimoEvento = jogo.eventoRevelacao();
            bool g0 = jogo.ganhouRodada(0), g1 = jogo.ganhouRodada(1);
            if (g0 && g1) SpawnConfete(0, W, 120);
            else if (g0) SpawnConfete(72, 454, 140);
            else if (g1) SpawnConfete(826, 1208, 140);
        }
        if (jogo.fase() == Fase::Vencedor) {
            if (faseAnterior != Fase::Vencedor) SpawnConfete(0, W, 260);
            emissor += dt;
            if (emissor > 0.15f && jogo.tempoFase() < 6.0f && jogo.vencedor() >= 0) {
                emissor = 0;
                SpawnConfete(0, W, 14);
            }
        }
        faseAnterior = jogo.fase();
        AtualizarConfete(dt);

        // --- desenho: o conteúdo (1280x720 lógicos) escala e centraliza na janela, o fundo ocupa tudo
        const float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
        const float k = std::min(sw / W, sh / H);
        BeginDrawing();
        FundoDecorado(sw, sh, k);
        rlPushMatrix();
        rlTranslatef(std::floor((sw - W * k) / 2), std::floor((sh - H * k) / 2), 0);
        rlScalef(k, k, 1);
        switch (jogo.fase()) {
            case Fase::Inicio: TelaInicio(logo, temLogo); break;
            case Fase::Jogando:
            case Fase::Revelacao: TelaJogo(jogo, foto); break;
            case Fase::Vencedor: TelaVencedor(jogo, logo, temLogo); break;
        }
        DesenharConfete();
        if (usarBle) {
            const int n = ble.conectados();
            Status(n > 0, n == 2   ? "Bluetooth: 2 controles"
                          : n == 1 ? "Bluetooth: 1 de 2 controles"
                                   : "Bluetooth: procurando...");
        }
        else if (porta.aberta()) Status(true, "ESP32: " + nomePorta);
        else Status(false, "modo teclado");
        rlPopMatrix();
        EndDrawing();
    }

    ble.parar();
    if (foto.tem) UnloadTexture(foto.tex);
    if (temLogo) UnloadTexture(logo);
    CloseWindow();
    return 0;
}
