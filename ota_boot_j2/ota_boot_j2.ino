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

const char *VERSAO_FIRMWARE = "3.4";

const char *URL_MANIFESTO = MANIFEST_URL;

const unsigned long DEBOUNCE_MS = 30UL;

// O jogo procura o ESP32 por este servico e recebe os botoes como notificacoes
// da caracteristica. Os mesmos UUIDs estao em desafio-planta/src/ble.cpp e ponte_ble/ponte_ble.py.
// As duas placas (jogador 1 e 2) usam os mesmos UUIDs; o jogo separa pelos BTN1-3 / BTN4-6.
const char *UUID_SERVICO = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char *UUID_CARACTERISTICA = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

struct Botao {
  uint8_t pino;
  uint8_t numero;
  int8_t led;  // pino do LED do botao, -1 = sem LED
  bool estadoEstavel;
  bool ultimaLeitura;
  unsigned long ultimaMudancaMs;
};

// Placa do jogador 2: manda BTN4 a BTN6. A do jogador 1 fica em ota_boot_j1.
const char *NOME_BLUETOOTH = "NEXT2026-J2";

// O GPIO12 decide a tensao da flash no boot: o botao nele tem que ir para o GND,
// nunca para o 3,3 V. LED amarelo no GPIO13 porque o 34 e so de entrada.
Botao botoes[] = {
  {12, 4, 14, HIGH, HIGH, 0},  // BTN4 verde
  {27, 5, 13, HIGH, HIGH, 0},  // BTN5 amarelo
  {18, 6, -1, HIGH, HIGH, 0}   // BTN6 Enter (sem LED)
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
    // LED entre o pino e o GND: HIGH acende.
    if (botoes[i].led >= 0) {
      pinMode(botoes[i].led, OUTPUT);
      digitalWrite(botoes[i].led, LOW);
    }
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
  const String nome = NOME_BLUETOOTH;
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

// Manda "BTN1" a "BTN6" (1-3 = jogador 1, 4-6 = jogador 2), que e o que o jogo entende. A mesma linha sai pela
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

// Para testar sem os botoes: digitar 1 a 3 no Serial Monitor aperta o botao correspondente.
void lerTestePelaSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '3') enviarBotao(botoes[c - '1'].numero);
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
      // O LED fica aceso enquanto o botao estiver apertado.
      if (b.led >= 0) digitalWrite(b.led, b.estadoEstavel == LOW ? HIGH : LOW);
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
