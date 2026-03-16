

#define PIN_MQ2 15
#define BUZZ 2

int value;

void setup() {
  Serial.begin(9600);
  pinMode(PIN_MQ2, OUTPUT);
  pinMode(BUZZ, OUTPUT);
}

void loop() {
  // записываем полученные данные с датчика
  value = analogRead(PIN_MQ2);

  // выводим информацию на монитор порта
  Serial.println("VALUE - " + String(value));
  Serial.println(" ");

  // включаем светодиод при превышении определенного значения
  if (value < 2300) {
    Serial.println('c');
    digitalWrite(BUZZ, HIGH);
    delay(1500);
    digitalWrite(BUZZ, LOW);
  } else {
    Serial.println("no c");
  }

  delay(500);
}
