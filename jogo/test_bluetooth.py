# Testa o receptor Bluetooth e a logica do jogo sem hardware: o bleak e trocado por um BLE falso.
# Rodar: python -m unittest test_bluetooth -v   (dentro de jogo/)
import asyncio
import os
import time
import unittest
from types import SimpleNamespace
from unittest import mock

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")

import bluetooth
from jogo import Jogo


class AnuncioFalso:
    def __init__(self, service_uuids):
        self.service_uuids = service_uuids


def dispositivo(endereco, nome):
    return SimpleNamespace(address=endereco, name=nome)


class MundoBLE:
    """Os 'ESP32' visiveis e as conexoes abertas, para o teste controlar o que o bleak ve."""

    def __init__(self):
        self.visiveis = {}  # endereco -> (dispositivo, uuids anunciados)
        self.clientes = {}  # endereco -> ClienteFalso conectado

    def scanner(self):
        mundo = self

        class ScannerFalso:
            @staticmethod
            async def discover(timeout, return_adv, service_uuids):
                await asyncio.sleep(0.05)
                return {e: (d, AnuncioFalso(u)) for e, (d, u) in mundo.visiveis.items()}

        return ScannerFalso

    def cliente(self):
        mundo = self

        class ClienteFalso:
            def __init__(self, disp, disconnected_callback):
                self.disp = disp
                self.ao_cair = disconnected_callback
                self.ao_notificar = None

            async def __aenter__(self):
                mundo.clientes[self.disp.address] = self
                return self

            async def __aexit__(self, *args):
                mundo.clientes.pop(self.disp.address, None)

            async def start_notify(self, uuid, callback):
                assert uuid == bluetooth.UUID_CARACTERISTICA
                self.ao_notificar = callback

            def cair(self):
                self.ao_cair(self)

        return ClienteFalso


def esperar(condicao, limite=3.0):
    fim = time.time() + limite
    while time.time() < fim:
        if condicao():
            return True
        time.sleep(0.02)
    return False


class TesteReceptor(unittest.TestCase):
    def setUp(self):
        self.mundo = MundoBLE()
        for alvo, falso in (("BleakScanner", self.mundo.scanner()), ("BleakClient", self.mundo.cliente())):
            patcher = mock.patch.object(bluetooth, alvo, falso)
            patcher.start()
            self.addCleanup(patcher.stop)
        patcher = mock.patch.object(bluetooth, "PAUSA_ENTRE_BUSCAS", 0.05)
        patcher.start()
        self.addCleanup(patcher.stop)

        self.receptor = bluetooth.ReceptorBluetooth()
        self.receptor.iniciar()
        self.addCleanup(self.receptor.parar)

    def notificar(self, endereco, texto):
        self.mundo.clientes[endereco].ao_notificar(None, bytearray(texto.encode()))

    def test_conecta_e_recebe_dois_controles(self):
        self.mundo.visiveis["AA"] = (dispositivo("AA", "NEXT2026-J1"), [bluetooth.UUID_SERVICO])
        self.mundo.visiveis["BB"] = (dispositivo("BB", "NEXT2026-J2"), [bluetooth.UUID_SERVICO])
        self.assertTrue(esperar(lambda: self.receptor.quantidade_conectados == 2))

        self.notificar("AA", "G1")
        self.notificar("BB", "y2\n")
        self.assertTrue(esperar(lambda: self.receptor.mensagens.qsize() == 2))
        self.assertEqual(sorted([self.receptor.mensagens.get(), self.receptor.mensagens.get()]), ["G1", "Y2"])

    def test_ignora_dispositivo_sem_o_servico(self):
        self.mundo.visiveis["CC"] = (dispositivo("CC", "Fone"), ["0000feb9-0000-1000-8000-00805f9b34fb"])
        time.sleep(0.5)
        self.assertEqual(self.receptor.quantidade_conectados, 0)
        self.assertEqual(self.mundo.clientes, {})

    def test_reconecta_depois_que_o_controle_cai(self):
        self.mundo.visiveis["AA"] = (dispositivo("AA", "NEXT2026-J1"), [bluetooth.UUID_SERVICO])
        self.assertTrue(esperar(lambda: self.receptor.quantidade_conectados == 1))
        primeiro = self.mundo.clientes["AA"]

        primeiro.cair()
        self.assertTrue(esperar(lambda: self.mundo.clientes.get("AA") not in (None, primeiro)))
        self.assertTrue(esperar(lambda: self.receptor.quantidade_conectados == 1))
        self.notificar("AA", "E1")
        self.assertEqual(self.receptor.mensagens.get(timeout=1), "E1")


class TesteJogo(unittest.TestCase):
    def test_sequencia_de_botoes(self):
        jogo = Jogo()
        for mensagem in ["G1", "G1", "Y1", "G2", "E1", "Y2", "Y2", "E2", "X9", "G", ""]:
            jogo.aplicar(mensagem)
        self.assertEqual(jogo.jogadores["1"].ultimo_confirmado, 12)
        self.assertEqual(jogo.jogadores["2"].ultimo_confirmado, 21)
        self.assertEqual(jogo.jogadores["1"].valor, 0)


if __name__ == "__main__":
    unittest.main()
