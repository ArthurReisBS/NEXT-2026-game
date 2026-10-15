# Painel com os botoes de todos os jogadores, para testar o jogo sem o ESP32.
# Cada clique publica no MQTT a mesma mensagem que o ESP32 mandaria (G1, Y2, E1...).
import os

import paho.mqtt.client as mqtt
import pygame

BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
TOPICO = os.environ.get("MQTT_TOPIC", "next2026/game/botoes")

JOGADORES = ["1", "2"]
# letra enviada, rotulo e cor de cada botao
BOTOES = [("G", "+1", (70, 170, 90)), ("Y", "+10", (220, 180, 50)), ("E", "Enter", (200, 70, 60))]

LARGURA, ALTURA = 480, 320
COR_FUNDO = (24, 24, 32)
COR_TEXTO = (20, 20, 20)
COR_INFO = (180, 180, 190)
TEMPO_DESTAQUE = 150  # ms que o botao fica claro depois do clique


def iniciar_mqtt():
    cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    cliente.connect_async(BROKER, 1883)
    cliente.loop_start()
    return cliente


def montar_botoes():
    # uma coluna por jogador, um botao por letra
    botoes = []
    for i, jogador in enumerate(JOGADORES):
        centro_x = LARGURA // 4 + i * LARGURA // 2
        for j, (letra, rotulo, cor) in enumerate(BOTOES):
            ret = pygame.Rect(0, 0, 160, 50)
            ret.center = (centro_x, 100 + j * 65)
            botoes.append({"ret": ret, "mensagem": letra + jogador, "rotulo": rotulo, "cor": cor})
    return botoes


def desenhar(tela, botoes, destaque, fonte_botao, fonte_info, conectado, ultima):
    tela.fill(COR_FUNDO)
    agora = pygame.time.get_ticks()

    for i, jogador in enumerate(JOGADORES):
        titulo = fonte_info.render(f"Jogador {jogador}", True, COR_INFO)
        tela.blit(titulo, titulo.get_rect(center=(LARGURA // 4 + i * LARGURA // 2, 45)))

    for botao in botoes:
        cor = botao["cor"]
        if destaque.get(botao["mensagem"], 0) > agora:
            cor = tuple(min(255, c + 60) for c in cor)
        pygame.draw.rect(tela, cor, botao["ret"], border_radius=8)
        texto = fonte_botao.render(botao["rotulo"], True, COR_TEXTO)
        tela.blit(texto, texto.get_rect(center=botao["ret"].center))

    status = "MQTT conectado" if conectado else "MQTT desconectado"
    if ultima:
        status += f"  |  enviado: {ultima}"
    rodape = fonte_info.render(status, True, COR_INFO)
    tela.blit(rodape, rodape.get_rect(center=(LARGURA // 2, ALTURA - 24)))

    pygame.display.flip()


def main():
    pygame.init()
    tela = pygame.display.set_mode((LARGURA, ALTURA))
    pygame.display.set_caption("NEXT 2026 - botoes")
    fonte_botao = pygame.font.Font(None, 36)
    fonte_info = pygame.font.Font(None, 26)
    relogio = pygame.time.Clock()

    cliente = iniciar_mqtt()
    botoes = montar_botoes()
    destaque = {}
    ultima = None

    rodando = True
    while rodando:
        for evento in pygame.event.get():
            if evento.type == pygame.QUIT:
                rodando = False
            elif evento.type == pygame.MOUSEBUTTONDOWN and evento.button == 1:
                for botao in botoes:
                    if botao["ret"].collidepoint(evento.pos):
                        cliente.publish(TOPICO, botao["mensagem"])
                        destaque[botao["mensagem"]] = pygame.time.get_ticks() + TEMPO_DESTAQUE
                        ultima = botao["mensagem"]

        desenhar(tela, botoes, destaque, fonte_botao, fonte_info, cliente.is_connected(), ultima)
        relogio.tick(60)

    cliente.loop_stop()
    cliente.disconnect()
    pygame.quit()


if __name__ == "__main__":
    main()
