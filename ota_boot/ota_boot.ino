#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "secrets.h"

enum ResultadoOTA {
  OTA_OK,
  OTA_SEM_WIFI,
  OTA_DOWNLOAD_FALHOU,
  OTA_SEM_ESPACO,
  OTA_GRAVACAO_FALHOU
};

const char *VERSAO_FIRMWARE = "1.0";

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *URL_MANIFESTO = MANIFEST_URL;

bool conectarWiFi(unsigned long timeoutMs) {
  Serial.print("Conectando a rede Wi-Fi ");
  Serial.print(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado com sucesso!");
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

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Firmware ");
  Serial.print(VERSAO_FIRMWARE);
  Serial.println(" iniciado.");

  if (conectarWiFi(10000UL)) {
    verificarAtualizacao();
  }
}

void loop() {
}
