#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Пин CE и CSN для NRF24L01+
#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte address[] = "1Node";  // Адрес канала связи

const int buttonPin = 2;  // Пин кнопки

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT_PULLUP);  // Внутренний подтягивающий резистор

  radio.begin();
  radio.openWritingPipe(address);  // Открываем канал для передачи
  radio.setPALevel(RF24_PA_LOW);   // Уровень мощности (можно RF24_PA_HIGH)
  radio.stopListening();            // Переходим в режим передачи
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {  // Кнопка нажата (LOW из-за INPUT_PULLUP)
    const char msg[] = "1";
    radio.write(&msg, sizeof(msg));  // Отправляем "1"
    Serial.println("Отправлено: 1");

    delay(300);  // Антидребезг
  }
}
