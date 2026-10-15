// Firmware do ESP32: lê 3 botões arcade e avisa o computador pela USB (serial).
// Cada aperto vira uma linha de texto: "BTN1", "BTN2" ou "BTN3".
//
// Ligação: um terminal do microswitch (NO) em cada GPIO, o outro (COM) no GND.
// Não precisa de resistor: usamos o pull-up interno (INPUT_PULLUP).

const uint8_t PINOS[3] = {25, 26, 27};     // BTN1 verde, BTN2 amarelo, BTN3 vermelho
const unsigned long DEBOUNCE_MS = 30;

bool leituraAnterior[3] = {HIGH, HIGH, HIGH};
bool estavel[3] = {HIGH, HIGH, HIGH};
unsigned long ultimaMudanca[3] = {0, 0, 0};

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 3; i++) {
    pinMode(PINOS[i], INPUT_PULLUP);
  }
  delay(200);
  Serial.println("READY");
}

void loop() {
  unsigned long agora = millis();
  for (int i = 0; i < 3; i++) {
    bool leitura = digitalRead(PINOS[i]);

    if (leitura != leituraAnterior[i]) {
      leituraAnterior[i] = leitura;
      ultimaMudanca[i] = agora;
    }

    if ((agora - ultimaMudanca[i]) > DEBOUNCE_MS && leitura != estavel[i]) {
      estavel[i] = leitura;
      if (leitura == LOW) {          // apertou (com pull-up, apertado = LOW)
        Serial.print("BTN");
        Serial.println(i + 1);
      }
    }
  }
}
