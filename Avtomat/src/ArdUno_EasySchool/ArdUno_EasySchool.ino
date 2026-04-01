#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();
const int relayPin = 3; // Пин для реле
э
void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Выключаем (большинство реле работают по LOW)

  mySwitch.enableReceive(0); // Приемник на D2
  Serial.println("Приемник готов...");
}

void loop() {
  if (mySwitch.available()) {
    long value = mySwitch.getReceivedValue();

    if (value == 1) {
      Serial.println("Команда получена! Переключаю реле...");
      
      // Логика: если было выключено — включить, и наоборот
      digitalWrite(relayPin, !digitalRead(relayPin)); 
      
      delay(500); // Пауза, чтобы реле не "дребезжало"
    }
    mySwitch.resetAvailable();
  }
}
