# Jogo controlado pelos botoes do ESP32 via MQTT.
# G soma 1, Y soma 10, E confirma (Enter).
# Sem o ESP32, as teclas G, Y e Enter do teclado fazem o mesmo.
import os
import queue

import paho.mqtt.client as mqtt
import pygame

BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
TOPICO = os.environ.get("MQTT_TOPIC", "next2026/game/botoes")

LARGURA, ALTURA = 480, 320
COR_FUNDO = (24, 24, 32)
COR_CAMPO = (245, 245, 245)
COR_TEXTO = (20, 20, 20)
COR_INFO = (180, 180, 190)

letras_recebidas = queue.Queue()


def ao_conectar(cliente, userdata, flags, reason_code, properties):
    cliente.subscribe(TOPICO)


def ao_receber(cliente, userdata, msg):
    # Roda na thread do MQTT: so enfileira, quem mexe na tela e o loop do jogo.
    letras_recebidas.put(msg.payload.decode().strip().upper())


def iniciar_mqtt():
    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cliente.on_connect = ao_conectar
    cliente.on_message = ao_receber
    cliente.connect_async(BROKER, 1883)
    cliente.loop_start()
    return cliente


class Jogo:
    def __init__(self):
        self.valor = 0
        self.ultimo_confirmado = None

    def aplicar(self, letra):
        if letra == "G":
            self.valor += 1
        elif letra == "Y":
            self.valor += 10
        elif letra == "E":
            self.ultimo_confirmado = self.valor
            self.valor = 0


def desenhar(tela, jogo, fonte_campo, fonte_info, conectado):
    tela.fill(COR_FUNDO)

    campo = pygame.Rect(0, 0, 320, 90)
    campo.center = (LARGURA // 2, ALTURA // 2 - 20)
    pygame.draw.rect(tela, COR_CAMPO, campo, border_radius=8)
    texto = fonte_campo.render(str(jogo.valor), True, COR_TEXTO)
    tela.blit(texto, texto.get_rect(midright=(campo.right - 20, campo.centery)))

    if jogo.ultimo_confirmado is not None:
        info = fonte_info.render(f"Confirmado: {jogo.ultimo_confirmado}", True, COR_INFO)
        tela.blit(info, info.get_rect(center=(LARGURA // 2, campo.bottom + 40)))

    status = "MQTT conectado" if conectado else "MQTT desconectado"
    rodape = fonte_info.render(f"{status}  |  G +1   Y +10   E enter", True, COR_INFO)
    tela.blit(rodape, rodape.get_rect(center=(LARGURA // 2, ALTURA - 24)))

    pygame.display.flip()


def main():
    pygame.init()
    tela = pygame.display.set_mode((LARGURA, ALTURA))
    pygame.display.set_caption("NEXT 2026")
    fonte_campo = pygame.font.Font(None, 72)
    fonte_info = pygame.font.Font(None, 26)
    relogio = pygame.time.Clock()

    cliente = iniciar_mqtt()
    jogo = Jogo()
    teclas = {pygame.K_g: "G", pygame.K_y: "Y", pygame.K_RETURN: "E", pygame.K_KP_ENTER: "E"}

    rodando = True
    while rodando:
        for evento in pygame.event.get():
            if evento.type == pygame.QUIT:
                rodando = False
            elif evento.type == pygame.KEYDOWN and evento.key in teclas:
                jogo.aplicar(teclas[evento.key])

        while not letras_recebidas.empty():
            jogo.aplicar(letras_recebidas.get())

        desenhar(tela, jogo, fonte_campo, fonte_info, cliente.is_connected())
        relogio.tick(60)

    cliente.loop_stop()
    cliente.disconnect()
    pygame.quit()


if __name__ == "__main__":
    main()
