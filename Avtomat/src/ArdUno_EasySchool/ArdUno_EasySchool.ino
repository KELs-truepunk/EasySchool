#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Пин CE и CSN для NRF24L01+
#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte address[] = "1Node";  // Тот же адрес, что у передатчика

void setup() {
  Serial.begin(9600);

  radio.begin();
  radio.openReadingPipe(0, address);  // Открываем канал для приёма
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();  // Переходим в режим приёма
}

void loop() {
  if (radio.available()) {
    char receivedMsg[32] = {0};  // Буфер для приёма
    radio.read(&receivedMsg, sizeof(receivedMsg));

    if (strcmp(receivedMsg, "1") == 0) {
      Serial.println("Получено: 1");
    }
  }
}
