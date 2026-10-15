#include "serial.hpp"

#ifdef _WIN32
#include <windows.h>

bool PortaSerial::abrir(const std::string& nome, int baud) {
    fechar();
    // Para COM10 ou maior o prefixo \\.\ é obrigatório; usar sempre é seguro.
    std::string caminho = "\\\\.\\" + nome;
    HANDLE h = CreateFileA(caminho.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return false; }
    dcb.BaudRate = static_cast<DWORD>(baud);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;  // evita resetar o ESP32 ao abrir
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(h, &dcb)) { CloseHandle(h); return false; }

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = MAXDWORD;  // leitura não bloqueante: devolve o que houver
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = 0;
    SetCommTimeouts(h, &to);

    handle_ = h;
    aberta_ = true;
    return true;
}

void PortaSerial::fechar() {
    if (handle_) CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
    aberta_ = false;
    buffer_.clear();
}

bool PortaSerial::lerLinha(std::string& linha) {
    if (!aberta_) return false;
    char tmp[256];
    DWORD lidos = 0;
    if (ReadFile(static_cast<HANDLE>(handle_), tmp, sizeof(tmp), &lidos, nullptr) && lidos > 0)
        buffer_.append(tmp, lidos);
    size_t pos = buffer_.find('\n');
    if (pos == std::string::npos) return false;
    linha = buffer_.substr(0, pos);
    buffer_.erase(0, pos + 1);
    while (!linha.empty() && (linha.back() == '\r' || linha.back() == ' ')) linha.pop_back();
    return true;
}

#else  // Linux e macOS
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static speed_t velocidade(int baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        default: return B115200;
    }
}

bool PortaSerial::abrir(const std::string& nome, int baud) {
    fechar();
    int fd = ::open(nome.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return false;

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) { ::close(fd); return false; }
    cfmakeraw(&tty);
    cfsetispeed(&tty, velocidade(baud));
    cfsetospeed(&tty, velocidade(baud));
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) { ::close(fd); return false; }

    fd_ = fd;
    aberta_ = true;
    return true;
}

void PortaSerial::fechar() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    aberta_ = false;
    buffer_.clear();
}

bool PortaSerial::lerLinha(std::string& linha) {
    if (!aberta_) return false;
    char tmp[256];
    ssize_t n = ::read(fd_, tmp, sizeof(tmp));
    if (n > 0) buffer_.append(tmp, static_cast<size_t>(n));
    size_t pos = buffer_.find('\n');
    if (pos == std::string::npos) return false;
    linha = buffer_.substr(0, pos);
    buffer_.erase(0, pos + 1);
    while (!linha.empty() && (linha.back() == '\r' || linha.back() == ' ')) linha.pop_back();
    return true;
}
#endif
