// ble.hpp - recebe os botões do ESP32 por Bluetooth Low Energy, sem bloquear o jogo.
// Procura o ESP32 pelo serviço UUID_SERVICO, conecta, assina as notificações e reconecta
// sozinho se cair. Cada notificação é uma linha de texto: "BTN1", "BTN2" ou "BTN3".
// Só tem implementação no Windows (WinRT); nos outros sistemas suportado() é false e
// o jogo usa a serial USB ou o teclado.
// Este header NÃO inclui windows.h de propósito: ele conflita com o raylib.h.
#pragma once
#include <memory>
#include <string>

class ReceptorBLE {
public:
    ReceptorBLE();
    ~ReceptorBLE();
    ReceptorBLE(const ReceptorBLE&) = delete;
    ReceptorBLE& operator=(const ReceptorBLE&) = delete;

    static bool suportado();
    void iniciar();  // começa a procurar em uma thread própria
    void parar();
    bool conectado() const;

    // Devolve true e preenche 'linha' quando chegou um aperto. Nunca bloqueia.
    bool lerLinha(std::string& linha);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
