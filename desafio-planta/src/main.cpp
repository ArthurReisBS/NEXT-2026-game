// Desafio EcoTrack - interface dos dois minigames.
// Recebe os botões do ESP32 pela USB (linhas BTN1..BTN6) ou do teclado.
#include <algorithm>
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

// Paleta tirada do PDF.
const Color FUNDO{234, 245, 253, 255};
const Color FUNDO_ONDA{220, 236, 252, 255};
const Color FUNDO_ONDA2{205, 226, 250, 255};
const Color NAVY{10, 38, 92, 255};
const Color AZUL{28, 93, 196, 255};
const Color AZUL_TXT{50, 108, 190, 255};
const Color CARTAO{204, 227, 251, 255};
const Color CARTAO_BORDA{111, 163, 221, 255};
const Color BOTAO_INICIAR{2, 24, 88, 255};
const Color VERDE{63, 174, 73, 255};
const Color VERDE_ESC{82, 150, 12, 255};
const Color AMARELO{217, 177, 32, 255};
const Color VERMELHO{233, 42, 46, 255};
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

// assets/rodadas.csv: "arquivo.jpg;altura_em_cm" (arquivo dentro de assets/imagens/).
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
        if (arq.empty() || alt.empty() || alt[0] < '0' || alt[0] > '9') continue;
        v.push_back({"imagens/" + arq, std::atoi(alt.c_str())});
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

void Folha(float x, float y, float rh, float rv, Color c) {
    DrawEllipse(static_cast<int>(x), static_cast<int>(y), rh, rv, c);
}

// Fundo no tamanho da janela: as manchas e plantinhas ficam coladas nos cantos de verdade,
// mesmo quando a janela não é 16:9. (sw, sh = janela; k = escala do conteúdo)
void Plantas() {
    Folha(46, 690, 20, 52, AZUL);
    Folha(100, 706, 20, 40, AZUL_TXT);
}

void FundoDecorado(float sw, float sh, float k) {
    ClearBackground(FUNDO);
    DrawCircleV({0, 0}, 190 * k, FUNDO_ONDA);
    DrawCircleV({sw - 30 * k, 90 * k}, 280 * k, FUNDO_ONDA);
    DrawCircleV({160 * k, sh + 40 * k}, 250 * k, FUNDO_ONDA);
    DrawCircleV({sw - 180 * k, sh + 70 * k}, 280 * k, FUNDO_ONDA);
    DrawCircleV({120 * k, sh + 70 * k}, 170 * k, FUNDO_ONDA2);
    DrawCircleV({sw - 90 * k, sh + 80 * k}, 190 * k, FUNDO_ONDA2);
    // plantinhas nos cantos de baixo
    rlPushMatrix();
    rlTranslatef(0, sh - H * k, 0);
    rlScalef(k, k, 1);
    Plantas();
    rlPopMatrix();
    rlPushMatrix();
    rlTranslatef(sw - W * k, sh - H * k, 0);
    rlScalef(k, k, 1);
    Folha(1226, 686, 20, 54, AZUL);
    Folha(1266, 706, 16, 38, AZUL_TXT);
    rlPopMatrix();
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

// Título "Desafio Ecotrack": a pílula se ajusta ao texto, que sempre fica dentro dela.
void Banner(float cx, float cy, float e) {
    float s1 = 84 * e;
    float w1 = LarguraTexto("Desafio", s1), w2 = LarguraTexto("Ecotrack", s1);
    float gap = 22 * e, pad = 52 * e;
    float larg = w1 + gap + w2 + 2 * pad, alt = 118 * e;
    Rectangle pill{cx - larg / 2, cy - alt / 2, larg, alt};
    DrawRectangleRounded(pill, 0.5f, 24, Color{200, 225, 250, 255});
    DrawRectangleRoundedLinesEx(pill, 0.5f, 24, 2 * e, Color{150, 195, 245, 255});
    float x = pill.x + pad, y = cy - s1 * 0.55f;
    Texto("Desafio", x, y, s1, NAVY, -1);
    Texto("Ecotrack", x + w1 + gap, y, s1, AZUL, -1);
    // folhinhas e traços do PDF
    float fx = pill.x + larg - 70 * e;
    Folha(fx, pill.y - 8 * e, 14 * e, 30 * e, AZUL_TXT);
    Folha(fx + 26 * e, pill.y + 6 * e, 12 * e, 26 * e, AZUL);
    float l = pill.x - 28 * e, r = pill.x + larg + 28 * e;
    DrawLineEx({l, cy - 18 * e}, {l + 14 * e, cy - 6 * e}, 5 * e, AZUL_TXT);
    DrawLineEx({l - 6 * e, cy + 14 * e}, {l + 14 * e, cy + 20 * e}, 5 * e, AZUL_TXT);
    DrawLineEx({r, cy - 18 * e}, {r - 14 * e, cy - 6 * e}, 5 * e, AZUL_TXT);
    DrawLineEx({r + 6 * e, cy + 14 * e}, {r - 14 * e, cy + 20 * e}, 5 * e, AZUL_TXT);
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
        DrawCircle(static_cast<int>(cx), static_cast<int>(r.y + r.height - 62), 11, Fade(CorBotao(i), alfa));
        // contorno para o botão branco não sumir no cartão claro
        if (i == 2) DrawCircleLines(static_cast<int>(cx), static_cast<int>(r.y + r.height - 62), 11, Fade(CARTAO_BORDA, alfa));
        Texto(nomes[i], cx, r.y + r.height - 44, 22, Fade(NAVY, alfa));
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

    Color fill = CARTAO, borda = CARTAO_BORDA;
    if (perdeu) { fill = Color{222, 228, 238, 255}; borda = Color{150, 160, 175, 255}; }
    else if (rev) {
        if (jogo.ganhouRodada(j)) { fill = Color{190, 255, 125, 255}; borda = VERDE_ESC; }
        else { fill = Color{255, 140, 140, 255}; borda = VERMELHO; }
    }
    DrawRectangleRounded({r.x + 4, r.y + 8, r.width, r.height}, 0.08f, 12, Fade(BLACK, 0.10f));
    DrawRectangleRounded(r, 0.08f, 12, fill);
    DrawRectangleRoundedLinesEx(r, 0.08f, 12, vez ? 6.0f : 3.0f, vez ? AZUL : borda);

    float cx = r.x + r.width / 2;
    Texto(TextFormat("Jogador %d", j + 1), cx, r.y + 22, 46, NAVY);

    // círculo + rótulo
    Vector2 ctr{cx, r.y + 168};
    if (mascarada) {
        DrawCircleV(ctr, 70, Color{150, 175, 205, 255});
        Visto(ctr.x, ctr.y, 80, WHITE);
        Texto("Pronto!", cx, r.y + 252, 40, NAVY);
    } else if (!altura && !rp.respondeuCrit) {
        DrawCircleV(ctr, 70, Color{236, 244, 253, 255});
        DrawCircleLines(static_cast<int>(ctr.x), static_cast<int>(ctr.y), 70, CARTAO_BORDA);
        Texto("?", cx, ctr.y - 34, 70, CARTAO_BORDA);
        const char* msg = perdeu ? "perdeu a vez" : vez ? "aperte rápido!" : "aguardando";
        Texto(msg, cx, r.y + 252, 36, Fade(NAVY, perdeu ? 0.75f : 0.55f));
    } else {
        Crit c = rp.critExibida(jogo.modo());
        DrawCircleV(ctr, 70, CorCrit(c));
        DrawLineEx({ctr.x - 108, ctr.y - 30}, {ctr.x - 88, ctr.y - 40}, 6, CARTAO_BORDA);
        DrawLineEx({ctr.x - 112, ctr.y - 6}, {ctr.x - 90, ctr.y - 8}, 6, CARTAO_BORDA);
        DrawLineEx({ctr.x + 108, ctr.y - 30}, {ctr.x + 88, ctr.y - 40}, 6, CARTAO_BORDA);
        DrawLineEx({ctr.x + 112, ctr.y - 6}, {ctr.x + 90, ctr.y - 8}, 6, CARTAO_BORDA);
        Texto(nomeCrit(c), cx, r.y + 252, 40, CorCrit(c));
    }

    // pílula com a altura
    Rectangle pill{r.x + 40, r.y + 316, r.width - 80, 86};
    DrawRectangleRounded(pill, 0.5f, 16, Color{234, 245, 253, 255});
    std::string txt;
    if (mascarada) txt = "?? cm";
    else if (altura) txt = TextFormat("%dcm", rp.cm());
    else txt = rev ? "-" : "?";
    Texto(txt, cx, pill.y + 8, 64, NAVY);

    // rodapé
    if (vez && altura) Legenda(r, "dezena", "unidade", "confirmar", 1.0f);
    else if (vez) Legenda(r, "OK", "Alerta", "Crítico", 1.0f);
    else if (perdeu) {
        // sem ícone: o jogador não respondeu
    } else if (rev) {
        if (jogo.ganhouRodada(j)) Visto(cx, r.y + r.height - 52, 56, VERDE_ESC);
        else Xis(cx, r.y + r.height - 52, 56, Color{170, 20, 25, 255});
    } else if (!mascarada && jogo.fase() == Fase::Jogando) {
        Texto("aguarde sua vez", cx, r.y + r.height - 50, 26, Fade(NAVY, 0.6f));
    }

    // carinhas da revelação (como no PDF)
    if (rev) {
        float fx = j == 0 ? r.x + 8 : r.x + r.width - 8;
        Rosto(fx, r.y + 10, 42, perdeu ? 0 : jogo.ganhouRodada(j) ? 1 : -1);
    }
}

// ------------------------------------------------------------------ telas
void TelaInicio(const Texture2D& logo, bool temLogo) {
    if (temLogo) {
        float e = 64.0f / logo.height;
        DrawTextureEx(logo, {40, 22}, 0, e, WHITE);
    } else {
        Texto("ECOTRACK", 40, 30, 54, NAVY, -1);
    }
    Banner(W / 2.0f, 200, 1.0f);
    Texto("Você conhece a grama só de olhar?", W / 2.0f, 310, 44, AZUL_TXT);

    const char* titulo[2] = {"INICIAR", "INICIAR"};
    const char* sub[2] = {"Altura em cm", "Criticidade"};
    for (int i = 0; i < 2; ++i) {
        Rectangle p{210.0f + i * 460.0f, 420, 400, 120};
        DrawRectangleRounded({p.x + 3, p.y + 8, p.width, p.height}, 0.5f, 16, Fade(BLACK, 0.12f));
        DrawRectangleRounded(p, 0.5f, 16, BOTAO_INICIAR);
        DrawCircle(static_cast<int>(p.x + 62), static_cast<int>(p.y + 60), 24, CorBotao(i));
        DrawCircleLines(static_cast<int>(p.x + 62), static_cast<int>(p.y + 60), 24, WHITE);
        Texto(titulo[i], p.x + 240, p.y + 20, 48, WHITE);
        Texto(sub[i], p.x + 240, p.y + 72, 30, Color{190, 215, 250, 255});
    }
    Texto("aperte o botão verde ou o amarelo", W / 2.0f, 566, 26, Fade(NAVY, 0.6f));
}

void TelaJogo(const Jogo& jogo, const Foto& foto) {
    Banner(W / 2.0f, 62, 0.62f);
    Texto(TextFormat("%d - %d", jogo.placar(0), jogo.placar(1)), W / 2.0f, 122, 50, NAVY);
    Texto(TextFormat("Rodada %d/%d", jogo.rodadaAtual() + 1, jogo.totalRodadas()), 36, 24, 28, NAVY, -1);

    Cartao(jogo, 0, {72, 178, 382, 500});
    Cartao(jogo, 1, {826, 178, 382, 500});
    DesenharFoto(foto, {492, 180, 296, 496});

    if (jogo.fase() == Fase::Revelacao) {
        float a = std::min(1.0f, jogo.tempoFase() / 0.35f);
        const Rodada& r = jogo.rodada();
        Rectangle box{526, 310, 228, 270};
        DrawRectangleRounded(box, 0.22f, 12, Fade(WHITE, 0.82f * a));
        DrawCircle(640, 372, 46 * (0.6f + 0.4f * a), Fade(CorCrit(r.crit()), a));
        Texto(nomeCrit(r.crit()), 640, 436, 40, Fade(CorCrit(r.crit()), a));
        Texto(TextFormat("%dcm", r.alturaCm), 640, 490, 60, Fade(NAVY, a));
        Texto("aperte qualquer botão para continuar", W / 2.0f, 690, 24, Fade(NAVY, 0.55f));
    } else if (jogo.modo() == Modo::Criticidade) {
        Texto("Quem apertar primeiro responde. O outro perde a vez!", W / 2.0f, 683, 28, NAVY);
    } else {
        Texto("Quem confirmar primeiro encerra a rodada!", W / 2.0f, 683, 28, NAVY);
    }
}

void TelaVencedor(const Jogo& jogo, const Texture2D& logo, bool temLogo) {
    if (temLogo) DrawTextureEx(logo, {40, 22}, 0, 64.0f / logo.height, WHITE);
    else Texto("ECOTRACK", 40, 30, 54, NAVY, -1);
    Banner(W / 2.0f, 200, 1.0f);
    int v = jogo.vencedor();
    if (v >= 0) {
        Rosto(400, 470, 84, 1, true);
        Texto("Vencedor:", 800, 318, 62, AZUL_TXT);
        Texto(TextFormat("Jogador %d", v + 1), 800, 390, 140, NAVY);
    } else {
        Rosto(400, 470, 84, 0);
        Texto("Empate!", 800, 360, 150, NAVY);
    }
    Texto(TextFormat("Placar final: %d - %d", jogo.placar(0), jogo.placar(1)), W / 2.0f, 610, 38, NAVY);
    Texto("aperte qualquer botão para jogar de novo", W / 2.0f, 670, 24, Fade(NAVY, 0.55f));
}

void Status(bool ok, const std::string& t) {
    float w = LarguraTexto(t, 20);
    DrawCircle(static_cast<int>(W - 24 - w - 14), 32, 6, ok ? VERDE : Color{150, 160, 175, 255});
    Texto(t, W - 24, 22, 20, Fade(NAVY, 0.7f), 1);
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
                          : n == 1 ? "Bluetooth: 1 controle, procurando o outro..."
                                   : "Bluetooth: procurando... (teclado: 1,2,3 / 8,9,0)");
        }
        else if (porta.aberta()) Status(true, "ESP32: " + nomePorta);
        else Status(false, "teclado: J1 1,2,3 | J2 8,9,0");
        rlPopMatrix();
        EndDrawing();
    }

    ble.parar();
    if (foto.tem) UnloadTexture(foto.tex);
    if (temLogo) UnloadTexture(logo);
    CloseWindow();
    return 0;
}
