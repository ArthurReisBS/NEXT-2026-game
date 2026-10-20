#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <NimBLEDevice.h>
#include "secrets.h"

enum ResultadoOTA {
  OTA_OK,
  OTA_SEM_WIFI,
  OTA_DOWNLOAD_FALHOU,
  OTA_SEM_ESPACO,
  OTA_GRAVACAO_FALHOU
};

const char *VERSAO_FIRMWARE = "3.1";

const char *URL_MANIFESTO = MANIFEST_URL;

const unsigned long DEBOUNCE_MS = 30UL;

// O jogo procura o ESP32 por este servico e recebe os botoes como notificacoes
// da caracteristica. Os mesmos UUIDs estao em jogo/jogo.py.
const char *UUID_SERVICO = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char *UUID_CARACTERISTICA = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

struct Botao {
  uint8_t pino;
  uint8_t numero;
  bool estadoEstavel;
  bool ultimaLeitura;
  unsigned long ultimaMudancaMs;
};

Botao botoes[] = {
  {25, 1, HIGH, HIGH, 0},  // BTN1 verde
  {26, 2, HIGH, HIGH, 0},  // BTN2 amarelo
  {27, 3, HIGH, HIGH, 0}   // BTN3 vermelho
};
const uint8_t QUANTIDADE_BOTOES = sizeof(botoes) / sizeof(botoes[0]);

NimBLECharacteristic *caracteristicaBotoes = nullptr;

bool conectarWiFi(unsigned long timeoutMs) {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.mode(WIFI_STA);
#ifdef WIFI_SSID
  // Gravacao pela USB: conecta e salva a rede na NVS da placa.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
  // Binario publicado: sem senha no codigo, usa a rede salva na NVS.
  WiFi.begin();
#endif
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi conectado com sucesso: ");
    Serial.println(WiFi.SSID());
    return true;
  }
  Serial.println("Erro: Nao foi possivel conectar ao Wi-Fi.");
  return false;
}

// Le o valor de texto de uma chave do manifesto: {"chave": "valor"}.
// Basta para o version.json, que so tem campos de texto simples.
String extrairCampo(const String &json, const String &chave) {
  int posChave = json.indexOf("\"" + chave + "\"");
  if (posChave < 0) return "";
  int doisPontos = json.indexOf(':', posChave);
  if (doisPontos < 0) return "";
  int inicio = json.indexOf('"', doisPontos);
  if (inicio < 0) return "";
  int fim = json.indexOf('"', inicio + 1);
  if (fim < 0) return "";
  return json.substring(inicio + 1, fim);
}

void mostrarProgressoOTA(size_t escritos, size_t total) {
  static int ultimaFatia = -1;
  if (total == 0) return;
  int fatia = (escritos * 100) / total / 10;
  if (fatia != ultimaFatia) {
    ultimaFatia = fatia;
    Serial.print("Gravado: ");
    Serial.print(fatia * 10);
    Serial.println("%");
  }
}

ResultadoOTA executarOTA(const String &url) {
  if (WiFi.status() != WL_CONNECTED) return OTA_SEM_WIFI;

  WiFiClientSecure cliente;
  cliente.setInsecure();

  HTTPClient http;
  if (!http.begin(cliente, url)) return OTA_DOWNLOAD_FALHOU;

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Codigo HTTP do binario: ");
    Serial.println(codigo);
    http.end();
    return OTA_DOWNLOAD_FALHOU;
  }

  int tamanho = http.getSize();
  if (tamanho <= 0) {
    http.end();
    return OTA_DOWNLOAD_FALHOU;
  }
  Serial.print("Tamanho do firmware: ");
  Serial.print(tamanho);
  Serial.println(" bytes");

  if (!Update.begin(tamanho)) {
    http.end();
    return OTA_SEM_ESPACO;
  }

  Update.onProgress(mostrarProgressoOTA);
  size_t gravados = Update.writeStream(http.getStream());
  http.end();

  if (gravados != (size_t)tamanho) {
    Update.abort();
    return OTA_GRAVACAO_FALHOU;
  }
  if (!Update.end(true) || !Update.isFinished()) {
    return OTA_GRAVACAO_FALHOU;
  }
  return OTA_OK;
}

void aplicarAtualizacao(const String &url) {
  Serial.println("Baixando e gravando o novo firmware...");
  ResultadoOTA resultado = executarOTA(url);

  switch (resultado) {
    case OTA_OK:
      Serial.println("Gravacao concluida. Reiniciando na versao nova...");
      delay(500);
      ESP.restart();
      break;
    case OTA_SEM_WIFI:
      Serial.println("Erro: A conexao Wi-Fi caiu durante a atualizacao.");
      break;
    case OTA_DOWNLOAD_FALHOU:
      Serial.println("Erro: Nao foi possivel baixar o arquivo .bin.");
      break;
    case OTA_SEM_ESPACO:
      Serial.println("Erro: Espaco insuficiente na particao OTA.");
      break;
    case OTA_GRAVACAO_FALHOU:
      Serial.print("Erro: Falha ao gravar o firmware. ");
      Serial.println(Update.errorString());
      break;
  }
}

void verificarAtualizacao() {
  Serial.println("Consultando o manifesto de versao...");

  WiFiClientSecure cliente;
  cliente.setInsecure();

  HTTPClient http;
  http.begin(cliente, URL_MANIFESTO);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Erro: O manifesto nao pode ser acessado. Codigo HTTP: ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  String versaoDisponivel = extrairCampo(payload, "version");
  String urlBinario = extrairCampo(payload, "url");

  if (versaoDisponivel == "" || urlBinario == "") {
    Serial.println("Erro: Falha ao interpretar o arquivo version.json.");
    return;
  }

  Serial.print("Versao instalada: ");
  Serial.println(VERSAO_FIRMWARE);
  Serial.print("Versao disponivel: ");
  Serial.println(versaoDisponivel);

  if (versaoDisponivel == VERSAO_FIRMWARE) {
    Serial.println("A versao instalada ja e a mais recente.");
    return;
  }

  Serial.println("Atualizacao encontrada!");
  Serial.print("URL do Firmware: ");
  Serial.println(urlBinario);
  aplicarAtualizacao(urlBinario);
}

void configurarBotoes() {
  // Botao entre o pino e o GND: solto = HIGH (pull-up interno), apertado = LOW.
  for (uint8_t i = 0; i < QUANTIDADE_BOTOES; i++) {
    pinMode(botoes[i].pino, INPUT_PULLUP);
  }
}

class CallbacksServidor : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *servidor, NimBLEConnInfo &info) override {
    Serial.println("Jogo conectado por Bluetooth.");
  }

  void onDisconnect(NimBLEServer *servidor, NimBLEConnInfo &info, int motivo) override {
    Serial.println("Jogo desconectado, anunciando de novo...");
    NimBLEDevice::startAdvertising();
  }
};

// Depois da checagem de OTA o Wi-Fi nao e mais usado: desliga para o radio ficar so no Bluetooth.
void desligarWiFi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

void iniciarBluetooth() {
  const String nome = "NEXT2026-botoes";
  NimBLEDevice::init(nome.c_str());

  NimBLEServer *servidor = NimBLEDevice::createServer();
  servidor->setCallbacks(new CallbacksServidor());

  NimBLEService *servico = servidor->createService(UUID_SERVICO);
  caracteristicaBotoes = servico->createCharacteristic(
    UUID_CARACTERISTICA, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  servico->start();

  // O UUID de 128 bits ocupa quase todo o anuncio, entao o nome vai na resposta de scan.
  NimBLEAdvertisementData anuncio;
  anuncio.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  anuncio.addServiceUUID(UUID_SERVICO);
  NimBLEAdvertisementData respostaScan;
  respostaScan.setName(nome.c_str());

  NimBLEAdvertising *anunciante = NimBLEDevice::getAdvertising();
  anunciante->setAdvertisementData(anuncio);
  anunciante->setScanResponseData(respostaScan);
  anunciante->start();

  Serial.print("Bluetooth anunciando como ");
  Serial.println(nome);
}

// Manda "BTN1", "BTN2" ou "BTN3", que e o que o jogo entende. A mesma linha sai pela
// USB, para o jogo tambem funcionar por serial (Linux/macOS).
void enviarBotao(uint8_t numero) {
  String mensagem = "BTN" + String(numero);
  Serial.println(mensagem);
  if (caracteristicaBotoes == nullptr || NimBLEDevice::getServer()->getConnectedCount() == 0) {
    Serial.println("(jogo nao conectado por Bluetooth, nao enviado)");
    return;
  }
  caracteristicaBotoes->setValue((const uint8_t *)mensagem.c_str(), mensagem.length());
  caracteristicaBotoes->notify();
}

// Para testar sem os botoes: digitar 1, 2 ou 3 no Serial Monitor aperta o botao correspondente.
void lerTestePelaSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '3') enviarBotao(c - '0');
  }
}

void lerBotoes(unsigned long agora) {
  for (uint8_t i = 0; i < QUANTIDADE_BOTOES; i++) {
    Botao &b = botoes[i];
    bool leitura = digitalRead(b.pino);
    if (leitura != b.ultimaLeitura) {
      b.ultimaLeitura = leitura;
      b.ultimaMudancaMs = agora;
    }
    if (agora - b.ultimaMudancaMs >= DEBOUNCE_MS && leitura != b.estadoEstavel) {
      b.estadoEstavel = leitura;
      if (b.estadoEstavel == LOW) {
        enviarBotao(b.numero);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Firmware ");
  Serial.print(VERSAO_FIRMWARE);
  Serial.println(" iniciado.");

  configurarBotoes();

  if (conectarWiFi(10000UL)) {
    verificarAtualizacao();
  }
  desligarWiFi();
  iniciarBluetooth();
}

void loop() {
  lerBotoes(millis());
  lerTestePelaSerial();
}
