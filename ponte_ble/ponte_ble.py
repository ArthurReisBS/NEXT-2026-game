# Ponte BLE -> serial virtual, para rodar o jogo com Bluetooth no Linux.
# Conecta nas duas placas (NEXT2026-J1 e NEXT2026-J2), recebe as notificacoes
# BTN1..BTN6 e escreve cada uma como linha numa porta serial virtual (pty).
# O jogo abre essa porta como se fosse a USB:
#
#   python ponte_ble.py
#   ./build/desafio_planta /tmp/next2026-ble
import asyncio
import os
import sys
import tty

from bleak import BleakClient, BleakScanner

# Os mesmos UUIDs estao em ota_boot_j1, ota_boot_j2 e desafio-planta/src/ble.cpp.
UUID_SERVICO = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
UUID_CARACTERISTICA = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

PLACAS = ("NEXT2026-J1", "NEXT2026-J2")

CAMINHO_PORTA = "/tmp/next2026-ble"


def criar_porta():
    lado_ponte, lado_jogo = os.openpty()
    tty.setraw(lado_jogo)
    # O lado_jogo fica aberto aqui tambem: sem isso, escrever no lado_ponte com o jogo
    # fechado da erro.
    if os.path.islink(CAMINHO_PORTA):
        os.remove(CAMINHO_PORTA)
    os.symlink(os.ttyname(lado_jogo), CAMINHO_PORTA)
    return lado_ponte, lado_jogo


async def soltar(endereco):
    # Se uma conexao falha no meio, o BlueZ pode terminar de conectar sozinho e
    # ficar com o ESP32 sem ninguem escutando. Conectado, o ESP32 para de
    # anunciar e nunca mais aparece no scan, entao a conexao e derrubada aqui.
    processo = await asyncio.create_subprocess_exec(
        "bluetoothctl", "disconnect", endereco,
        stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL,
    )
    await processo.wait()


async def procurar(faltando):
    # Um scan so para as placas que ainda nao estao conectadas: {nome: dispositivo}.
    achados = await BleakScanner.discover(timeout=5.0, return_adv=True)
    placas = {}
    for dispositivo, anuncio in achados.values():
        nome = anuncio.local_name or dispositivo.name
        if UUID_SERVICO in anuncio.service_uuids and nome in faltando:
            placas[nome] = dispositivo
    return placas


async def conectar(nome, dispositivo, lado_ponte, conectadas):
    desconectou = asyncio.Event()

    def ao_receber(_, dados: bytearray):
        mensagem = dados.decode(errors="ignore").strip()
        print(f"{nome}: {mensagem}")
        os.write(lado_ponte, (mensagem + "\n").encode())

    cliente = BleakClient(dispositivo, disconnected_callback=lambda _: desconectou.set())
    await cliente.connect()
    await cliente.start_notify(UUID_CARACTERISTICA, ao_receber)
    conectadas.add(nome)
    print(f"Conectado em {nome} ({dispositivo.address}).")

    async def esperar_queda():
        await desconectou.wait()
        conectadas.discard(nome)
        print(f"{nome} desconectou.")

    return asyncio.create_task(esperar_queda())


async def rodar(lado_ponte):
    conectadas = set()
    enderecos = {}  # ultimo endereco visto de cada placa
    tarefas = set()
    while True:
        faltando = [nome for nome in PLACAS if nome not in conectadas]
        if not faltando:
            await asyncio.sleep(1)
            continue

        print(f"Procurando {', '.join(faltando)} por Bluetooth...")
        try:
            placas = await procurar(faltando)
        except Exception as erro:
            # Ex.: o PC suspendeu e o adaptador sumiu por um instante.
            print(f"Falha no scan: {erro}")
            await asyncio.sleep(2)
            continue

        # Uma conexao por vez: o BlueZ recusa ("InProgress") duas ao mesmo tempo.
        for nome in faltando:
            dispositivo = placas.get(nome)
            if dispositivo is None:
                if nome in enderecos:
                    await soltar(enderecos[nome])
                continue
            enderecos[nome] = dispositivo.address
            try:
                tarefa = await conectar(nome, dispositivo, lado_ponte, conectadas)
                tarefas.add(tarefa)
                tarefa.add_done_callback(tarefas.discard)
            except Exception as erro:
                print(f"Falha ao conectar em {nome}: {erro}")
                await soltar(dispositivo.address)


def main():
    lado_ponte, lado_jogo = criar_porta()
    print(f"Porta virtual: {CAMINHO_PORTA} -> {os.ttyname(lado_jogo)}")
    print(f"Rode o jogo com: ./build/desafio_planta {CAMINHO_PORTA}")
    try:
        asyncio.run(rodar(lado_ponte))
    except KeyboardInterrupt:
        pass
    finally:
        if os.path.islink(CAMINHO_PORTA):
            os.remove(CAMINHO_PORTA)
        os.close(lado_ponte)
        os.close(lado_jogo)
    return 0


if __name__ == "__main__":
    sys.exit(main())
