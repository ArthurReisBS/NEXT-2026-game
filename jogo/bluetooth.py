# Recebe os botoes dos ESP32 por Bluetooth Low Energy.
# Cada ESP32 anuncia o servico UUID_SERVICO; o jogo conecta em todos que encontrar
# e cada notificacao da caracteristica e uma mensagem como "G1" ou "E2".
import asyncio
import queue
import threading

from bleak import BleakClient, BleakScanner

# Os mesmos UUIDs estao em ../ota_boot/ota_boot.ino
UUID_SERVICO = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
UUID_CARACTERISTICA = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

TEMPO_BUSCA = 3.0  # s de cada varredura
PAUSA_ENTRE_BUSCAS = 1.0  # s


class ReceptorBluetooth:
    def __init__(self):
        self.mensagens = queue.Queue()
        self._conectados = set()
        self._travas = threading.Lock()
        self._parar = threading.Event()
        self._thread = threading.Thread(target=lambda: asyncio.run(self._principal()), daemon=True)

    @property
    def quantidade_conectados(self):
        with self._travas:
            return len(self._conectados)

    def iniciar(self):
        self._thread.start()

    def parar(self):
        self._parar.set()
        self._thread.join(timeout=5)

    def _ao_receber(self, _caracteristica, dados):
        # Roda na thread do Bluetooth: so enfileira, quem mexe na tela e o loop do jogo.
        self.mensagens.put(bytes(dados).decode(errors="ignore").strip().upper())

    async def _principal(self):
        tarefas = {}
        while not self._parar.is_set():
            try:
                achados = await BleakScanner.discover(
                    timeout=TEMPO_BUSCA, return_adv=True, service_uuids=[UUID_SERVICO]
                )
            except Exception as erro:  # adaptador desligado, sem permissao...
                print(f"Bluetooth indisponivel: {erro}")
                await asyncio.sleep(2)
                continue

            for endereco, (dispositivo, anuncio) in achados.items():
                if UUID_SERVICO not in [u.lower() for u in anuncio.service_uuids]:
                    continue
                if endereco not in tarefas or tarefas[endereco].done():
                    tarefas[endereco] = asyncio.create_task(self._manter(dispositivo))
            await asyncio.sleep(PAUSA_ENTRE_BUSCAS)

        for tarefa in tarefas.values():
            tarefa.cancel()
        await asyncio.gather(*tarefas.values(), return_exceptions=True)

    async def _manter(self, dispositivo):
        # Uma conexao por ESP32; quando cai, a tarefa termina e a proxima busca recria.
        caiu = asyncio.Event()
        try:
            async with BleakClient(dispositivo, disconnected_callback=lambda _: caiu.set()) as cliente:
                await cliente.start_notify(UUID_CARACTERISTICA, self._ao_receber)
                with self._travas:
                    self._conectados.add(dispositivo.address)
                print(f"Conectado: {dispositivo.name or dispositivo.address}")
                while not caiu.is_set() and not self._parar.is_set():
                    await asyncio.sleep(0.2)
        except Exception as erro:
            print(f"Falha ao conectar em {dispositivo.address}: {erro}")
        finally:
            with self._travas:
                self._conectados.discard(dispositivo.address)
