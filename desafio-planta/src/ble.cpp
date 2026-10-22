#include "ble.hpp"

#ifdef _WIN32
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;

namespace {
// Os mesmos UUIDs estão em ../ota_boot_j1 e ../ota_boot_j2
const guid UUID_SERVICO{0x6e400001, 0xb5a3, 0xf393, {0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e}};
const guid UUID_CARACTERISTICA{0x6e400003, 0xb5a3, 0xf393, {0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e}};

// Um controle (ESP32) por jogador: NEXT2026-J1 manda BTN1-3 e NEXT2026-J2 manda BTN4-6.
constexpr int CONTROLES = 2;

using namespace std::chrono_literals;
}  // namespace

struct ReceptorBLE::Impl {
    // Uma thread por controle. Cada uma procura um ESP32 que a outra ainda não pegou.
    std::thread threads[CONTROLES];
    std::atomic<bool> parar{false};
    std::atomic<int> conectados{0};
    std::mutex trava;
    std::deque<std::string> fila;
    std::mutex travaEnderecos;
    std::set<uint64_t> emUso;  // endereços já com uma thread

    void empilhar(std::string texto) {
        while (!texto.empty() && (texto.back() == '\n' || texto.back() == '\r' || texto.back() == ' '))
            texto.pop_back();
        if (texto.empty()) return;
        std::lock_guard<std::mutex> g(trava);
        fila.push_back(std::move(texto));
    }

    void dormir(std::chrono::milliseconds ms) {
        auto fim = std::chrono::steady_clock::now() + ms;
        while (!parar && std::chrono::steady_clock::now() < fim) std::this_thread::sleep_for(50ms);
    }

    // Escuta anúncios do nosso serviço por até 'limite'. Devolve o endereço de um ESP32 que
    // nenhuma outra thread pegou (já reservado para esta) ou 0.
    uint64_t procurar(std::chrono::milliseconds limite, BluetoothAddressType& tipoEndereco) {
        BluetoothLEAdvertisementWatcher watcher;
        watcher.ScanningMode(BluetoothLEScanningMode::Active);
        watcher.AdvertisementFilter().Advertisement().ServiceUuids().Append(UUID_SERVICO);

        std::atomic<uint64_t> achado{0};
        std::mutex travaAchado;
        bool encerrado = false;
        auto token = watcher.Received([&](auto&&, BluetoothLEAdvertisementReceivedEventArgs const& a) {
            std::lock_guard<std::mutex> g(travaAchado);
            if (encerrado || achado != 0) return;
            uint64_t endereco = a.BluetoothAddress();
            std::lock_guard<std::mutex> ge(travaEnderecos);
            if (!emUso.insert(endereco).second) return;  // é o controle da outra thread
            tipoEndereco = a.BluetoothAddressType();
            achado = endereco;
        });
        watcher.Start();
        auto fim = std::chrono::steady_clock::now() + limite;
        while (!parar && achado == 0 && std::chrono::steady_clock::now() < fim) std::this_thread::sleep_for(50ms);
        watcher.Stop();
        watcher.Received(token);
        // Espera um Received que ainda esteja rodando e impede que um atrasado reserve outra placa.
        std::lock_guard<std::mutex> g(travaAchado);
        encerrado = true;
        return achado;
    }

    void liberar(uint64_t endereco) {
        std::lock_guard<std::mutex> g(travaEnderecos);
        emUso.erase(endereco);
    }

    // Conecta e fica até a conexão cair. Devolve quando é para procurar de novo.
    void sessao(uint64_t endereco, BluetoothAddressType tipoEndereco) {
        auto dispositivo = BluetoothLEDevice::FromBluetoothAddressAsync(endereco, tipoEndereco).get();
        if (!dispositivo) return;

        // Sem isto o Windows derruba a conexao ociosa antes de terminar a descoberta.
        auto sessao = GattSession::FromDeviceIdAsync(dispositivo.BluetoothDeviceId()).get();
        sessao.MaintainConnection(true);

        // Lista todos os servicos e filtra aqui: a busca ja filtrada por UUID falha com este ESP32.
        auto servicos = dispositivo.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
        if (servicos.Status() != GattCommunicationStatus::Success) {
            sessao.Close();
            dispositivo.Close();
            return;
        }
        GattDeviceService servico{nullptr};
        for (auto const& s : servicos.Services())
            if (s.Uuid() == UUID_SERVICO) servico = s;
        if (!servico) {
            sessao.Close();
            dispositivo.Close();
            return;
        }
        auto carac = servico.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).get();
        GattCharacteristic caracteristica{nullptr};
        if (carac.Status() == GattCommunicationStatus::Success)
            for (auto const& c : carac.Characteristics())
                if (c.Uuid() == UUID_CARACTERISTICA) caracteristica = c;
        if (!caracteristica) {
            servico.Close();
            sessao.Close();
            dispositivo.Close();
            return;
        }

        auto tokenValor = caracteristica.ValueChanged(
            [this](auto&&, GattValueChangedEventArgs const& a) {
                auto leitor = Windows::Storage::Streams::DataReader::FromBuffer(a.CharacteristicValue());
                std::string texto(leitor.UnconsumedBufferLength(), '\0');
                leitor.ReadBytes(array_view<uint8_t>(reinterpret_cast<uint8_t*>(texto.data()),
                                                     reinterpret_cast<uint8_t*>(texto.data()) + texto.size()));
                empilhar(texto);
            });

        std::atomic<bool> caiu{false};
        auto tokenStatus = dispositivo.ConnectionStatusChanged([&](BluetoothLEDevice const& d, auto&&) {
            if (d.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) caiu = true;
        });

        auto escrita = caracteristica
                           .WriteClientCharacteristicConfigurationDescriptorAsync(
                               GattClientCharacteristicConfigurationDescriptorValue::Notify)
                           .get();
        if (escrita == GattCommunicationStatus::Success) {
            ++conectados;
            std::printf("Bluetooth: ESP32 conectado (%d de %d).\n", conectados.load(), CONTROLES); std::fflush(stdout);
            while (!caiu && !parar) std::this_thread::sleep_for(100ms);
            --conectados;
            std::printf("Bluetooth: ESP32 desconectado.\n"); std::fflush(stdout);
        }

        caracteristica.ValueChanged(tokenValor);
        dispositivo.ConnectionStatusChanged(tokenStatus);
        servico.Close();
        sessao.Close();
        dispositivo.Close();
    }

    void executar() {
        init_apartment(apartment_type::multi_threaded);
        while (!parar) {
            uint64_t endereco = 0;
            try {
                BluetoothAddressType tipoEndereco = BluetoothAddressType::Public;
                endereco = procurar(3000ms, tipoEndereco);
                if (endereco != 0) sessao(endereco, tipoEndereco);
            } catch (hresult_error const& e) {
                // adaptador desligado, aparelho sumiu no meio da conexão...
                std::printf("Bluetooth: erro 0x%08x, tentando de novo.\n", static_cast<unsigned>(e.code()));
                std::fflush(stdout);
            }
            if (endereco != 0) liberar(endereco);
            dormir(1000ms);
        }
        uninit_apartment();
    }
};

ReceptorBLE::ReceptorBLE() : impl_(new Impl) {}
ReceptorBLE::~ReceptorBLE() { parar(); }
bool ReceptorBLE::suportado() { return true; }
void ReceptorBLE::iniciar() {
    for (auto& t : impl_->threads)
        if (!t.joinable()) t = std::thread([this] { impl_->executar(); });
}
void ReceptorBLE::parar() {
    impl_->parar = true;
    for (auto& t : impl_->threads)
        if (t.joinable()) t.join();
}
int ReceptorBLE::conectados() const { return impl_->conectados; }
bool ReceptorBLE::lerLinha(std::string& linha) {
    std::lock_guard<std::mutex> g(impl_->trava);
    if (impl_->fila.empty()) return false;
    linha = std::move(impl_->fila.front());
    impl_->fila.pop_front();
    return true;
}

#else  // Linux e macOS: sem Bluetooth nativo, use a serial USB ou o teclado

struct ReceptorBLE::Impl {};
ReceptorBLE::ReceptorBLE() : impl_(new Impl) {}
ReceptorBLE::~ReceptorBLE() = default;
bool ReceptorBLE::suportado() { return false; }
void ReceptorBLE::iniciar() {}
void ReceptorBLE::parar() {}
int ReceptorBLE::conectados() const { return 0; }
bool ReceptorBLE::lerLinha(std::string&) { return false; }
#endif
