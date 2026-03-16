#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h> 

int rele = 2; 
int LED = 13;

// Настройки
const char *ssid = "TP-Link_B815";    //Wi-Fi
const char *pass = "66065401"; //Пароль

const char *mqtt_server = "192.168.0.102";   //брокер
const int mqtt_port = 1883;                  //порт
const char *mqtt_user = "ZVONOK";      //имя пользователя для брокера
const char *mqtt_pass = "112233";          //пароль

const char* ntpServer = "192.168.0.102";//ntp-сервер

//настройка времени 
const long gmtOffset_sec = 0;      // GMT+0 (Зимнее время)
const int daylightOffset_sec = 3600; // GMT+1 (Летнее время, если актуально)

int Faza = 0; 
int chasi = 0; // Смещение часов (будет настраиваться через MQTT)

WiFiClient wclient; 
PubSubClient client(wclient, mqtt_server, mqtt_port);

// Флаги для защиты от повторных срабатываний
unsigned long lastRingTime = 0;
const unsigned long RING_COOLDOWN = 6000; // 6 секунд между звонками

void ring() {
    unsigned long currentTime = millis();
    
    // Защита от повторного срабатывания
    if (currentTime - lastRingTime < RING_COOLDOWN) {
        return;
    }
    
    lastRingTime = currentTime;
    
    Serial.println("RRRR");
    digitalWrite(rele, LOW);
    delay(5000);
    digitalWrite(rele, HIGH);
}

// Упрощенная проверка расписания
bool shouldRing(int day, int hour, int minute, int second) {
    if (second >= 3) return false; // Звоним только в первые 3 секунды минуты
    
    // Проверяем день недели (0-Вс, 1-Пн, ..., 6-Сб)
    if (day == 2 || day == 3 || day == 5) { // Вт, Ср, Пт
        // Проверяем расписание для этих дней
        if ((hour == 8 && minute == 0) ||
            (hour == 8 && minute == 40) ||
            (hour == 8 && minute == 50) ||
            (hour == 9 && minute == 30) ||
            (hour == 9 && minute == 40) ||
            (hour == 10 && minute == 20) ||
            (hour == 10 && minute == 40) ||
            (hour == 11 && minute == 20) ||
            (hour == 11 && minute == 30) ||
            (hour == 12 && minute == 10) ||
            (hour == 12 && minute == 20) ||
            (hour == 13 && minute == 0) ||
            (hour == 13 && minute == 10) ||
            (hour == 13 && minute == 50) ||
            (hour == 14 && minute == 0) ||
            (hour == 14 && minute == 40) ||
            (hour == 14 && minute == 50) ||
            (hour == 15 && minute == 30) ||
            (hour == 15 && minute == 50) ||
            (hour == 16 && minute == 30) ||
            (hour == 16 && minute == 40) ||
            (hour == 17 && minute == 20) ||
            (hour == 17 && minute == 30) ||
            (hour == 18 && minute == 10) ||
            (hour == 18 && minute == 20) ||
            (hour == 19 && minute == 0)) {
            return true;
        }
    }
    
    if (day == 1 || day == 4) { // Пн, Чт
        // Проверяем расписание для этих дней
        if ((hour == 8 && minute == 0) ||
            (hour == 8 && minute == 30) ||
            (hour == 8 && minute == 35) ||
            (hour == 9 && minute == 10) ||
            (hour == 9 && minute == 20) ||
            (hour == 9 && minute == 55) ||
            (hour == 10 && minute == 5) ||
            (hour == 10 && minute == 40) ||
            (hour == 11 && minute == 0) ||
            (hour == 11 && minute == 35) ||
            (hour == 11 && minute == 45) ||
            (hour == 12 && minute == 20) ||
            (hour == 12 && minute == 30) ||
            (hour == 13 && minute == 5) ||
            (hour == 13 && minute == 15) ||
            (hour == 13 && minute == 50) ||
            (hour == 14 && minute == 0) ||
            (hour == 14 && minute == 30) ||
            (hour == 14 && minute == 35) ||
            (hour == 15 && minute == 10) ||
            (hour == 15 && minute == 20) ||
            (hour == 15 && minute == 55) ||
            (hour == 16 && minute == 5) ||
            (hour == 16 && minute == 40) ||
            (hour == 17 && minute == 0) ||
            (hour == 17 && minute == 35) ||
            (hour == 17 && minute == 45) ||
            (hour == 18 && minute == 20) ||
            (hour == 18 && minute == 25) ||
            (hour == 19 && minute == 0)) {
            return true;
        }
    }
    
    return false;
}

int zvonki() {
    struct tm timeinfo;
    
    // Получаем время с таймаутом
    if (!getLocalTime(&timeinfo, 100)) {
        Serial.println("Ошибка получения времени");
        return 1;
    }
    
    // Корректируем часы
    int time_wday = timeinfo.tm_wday;
    int time_hr = timeinfo.tm_hour + chasi;
    int time_min = timeinfo.tm_min;
    int time_sec = timeinfo.tm_sec;
    
    // Выводим отладочную информацию
    Serial.printf("День: %d, Время: %02d:%02d:%02d\n", 
                  time_wday, time_hr, time_min, time_sec);
    
    // Отправляем в MQTT БЕЗ использования String (чтобы избежать падения)
    char msg[60];
    snprintf(msg, sizeof(msg), 
             "День:%d,Время:%02d:%02d:%02d", 
             time_wday, time_hr, time_min, time_sec);
    
    if (client.connected()) {
        client.publish("test/timenow", msg);
    }
    
    // Проверяем нужно ли звонить
    if (Faza == 0) {
        if (shouldRing(time_wday, time_hr, time_min, time_sec)) {
            ring();
        }
    }
    
    return 0;
}

void callback(const MQTT::Publish &pub) {
    Serial.printf("Топик: %s, Данные: %s\n", 
                  pub.topic().c_str(), 
                  pub.payload_string().c_str());
    
    String payload = pub.payload_string();
    
    if (String(pub.topic()) == "test/button") {
        if (payload == "1") ring();
        if (payload == "On") Faza = 1;
        if (payload == "Off") Faza = 0;
    }
    
    if (String(pub.topic()) == "test/chasi") {
        chasi = payload.toInt();
        Serial.printf("Смещение часов установлено: %d\n", chasi);
    }
}

void setup() {
    Serial.begin(9600);
    delay(1000);
    
    pinMode(LED, OUTPUT);
    pinMode(rele, OUTPUT);
    digitalWrite(rele, HIGH); // Выключаем реле
    
    Serial.println("Инициализация...");
    
    // Подключаем WiFi
    WiFi.begin(ssid, pass);
    Serial.print("Подключение к WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi подключен!");
    
    // Настраиваем NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("NTP настроен");
    
    // Ждем синхронизации времени
    struct tm timeinfo;
    for (int i = 0; i < 20; i++) {
        if (getLocalTime(&timeinfo)) {
            Serial.println("Время синхронизировано");
            break;
        }
        delay(500);
        Serial.print(".");
    }
}

void loop() {
    // Переподключаемся к WiFi при необходимости
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, pass);
        delay(5000);
        return;
    }
    
    // Переподключаемся к MQTT при необходимости
    if (!client.connected()) {
        Serial.println("Подключение к MQTT...");
        if (client.connect(MQTT::Connect("Zvonok")
            .set_auth(mqtt_user, mqtt_pass))) {
            Serial.println("MQTT подключен");
            client.set_callback(callback);
            client.subscribe("test/button");
            client.subscribe("test/chasi");
        } else {
            Serial.println("Ошибка MQTT");
            delay(5000);
            return;
        }
    }
    
    // Основной цикл работы
    if (client.connected()) {
        digitalWrite(LED, HIGH);
        client.loop();
        zvonki();
        digitalWrite(LED, LOW);
    }
    
    delay(1000); // Проверяем раз в секунду
}
