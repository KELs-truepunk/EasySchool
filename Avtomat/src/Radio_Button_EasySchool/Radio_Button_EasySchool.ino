#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();
const int buttonPin = 2; // Пин кнопки (как в вашем коде)

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT_PULLUP);

  // Передатчик TU0 подключаем DATA к пину D10
  mySwitch.enableTransmit(10);
  
  Serial.println("Пульт готов к передаче...");
}

void loop() {
  // Кнопка нажата (LOW из-за INPUT_PULLUP)
  if (digitalRead(buttonPin) == LOW) {
    
    // Вместо строки "1" отправляем число 1
    // 24 — это стандартная длина протокола для этих модулей
    mySwitch.send(1, 24); 
    
    Serial.println("1");
    delay(300); // Антидребезг (как в вашем коде)
  }
}
