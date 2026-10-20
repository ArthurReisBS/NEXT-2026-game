// serial.hpp - leitura da porta serial (USB do ESP32), sem bloquear o jogo.
// Este header NÃO inclui windows.h de propósito: ele conflita com o raylib.h.
#pragma once
#include <string>

class PortaSerial {
public:
    PortaSerial() = default;
    ~PortaSerial() { fechar(); }
    PortaSerial(const PortaSerial&) = delete;
    PortaSerial& operator=(const PortaSerial&) = delete;

    // nome: "COM5" no Windows, "/dev/ttyUSB0" no Linux, "/dev/cu.usbserial-XXXX" no macOS.
    bool abrir(const std::string& nome, int baud = 115200);
    void fechar();
    bool aberta() const { return aberta_; }

    // Devolve true e preenche 'linha' quando chegou uma linha completa. Nunca bloqueia.
    bool lerLinha(std::string& linha);

private:
    bool aberta_ = false;
    std::string buffer_;
    void* handle_ = nullptr;  // HANDLE (Windows)
    int fd_ = -1;             // descritor (Linux/macOS)
};
