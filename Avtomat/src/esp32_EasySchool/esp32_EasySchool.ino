#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>          

//Хардвейр часть
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 1602 I2C
RTC_DS1307 rtc;//RTC (HW-111)
  // Пины 
int rele = 2;
int LED = 13; //светодиод для индикации


//Настройка сети
//Wi-Fi
const char* ssid = "DECO222_Guest";
const char* pass = "66065401!!!";

const char* server = "192.168.0.102";

//MQTT
const int mqtt_port = 1883;
const char* mqtt_user = "ZVONOK";
const char* mqtt_pass = "112233";
//NTP
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;


int Faza = 0;
int chasi = 0;      // смещение часов

WiFiClient wclient;
PubSubClient client(wclient, server, mqtt_port);

unsigned long lastRingTime = 0;
const unsigned long RING_COOLDOWN = 6000;

unsigned long lastLcdUpdate = 0;
bool ringBlink = false;
unsigned long ringBlinkStop = 0;

// Для периодической синхронизации RTC с NTP (раз в час)
unsigned long lastRtcSync = 0;
const unsigned long RTC_SYNC_INTERVAL = 3600000; // 1 час

void ring() {
    unsigned long currentTime = millis();
    if (currentTime - lastRingTime < RING_COOLDOWN) return;
    lastRingTime = currentTime;

    Serial.println("RRRR");
    digitalWrite(rele, LOW);
    delay(5000);
    digitalWrite(rele, HIGH);

    ringBlink = true;
    ringBlinkStop = millis() + 2000;
}

bool shouldRing(int day, int hour, int minute, int second) {
    if (second >= 3) return false;

    if (day == 2 || day == 3 || day == 5) { // Вт, Ср, Пт
        if ((hour == 8 && minute == 0)  || (hour == 8 && minute == 40) ||
            (hour == 8 && minute == 50) || (hour == 9 && minute == 30) ||
            (hour == 9 && minute == 40) || (hour == 10 && minute == 20) ||
            (hour == 10 && minute == 40)|| (hour == 11 && minute == 20) ||
            (hour == 11 && minute == 30)|| (hour == 12 && minute == 10) ||
            (hour == 12 && minute == 20)|| (hour == 13 && minute == 0) ||
            (hour == 13 && minute == 10)|| (hour == 13 && minute == 50) ||
            (hour == 14 && minute == 0) || (hour == 14 && minute == 40) ||
            (hour == 14 && minute == 50)|| (hour == 15 && minute == 30) ||
            (hour == 15 && minute == 50)|| (hour == 16 && minute == 30) ||
            (hour == 16 && minute == 40)|| (hour == 17 && minute == 20) ||
            (hour == 17 && minute == 30)|| (hour == 18 && minute == 10) ||
            (hour == 18 && minute == 20)|| (hour == 19 && minute == 0))
            return true;
    }

    if (day == 1 || day == 4) { // Пн, Чт
        if ((hour == 8 && minute == 0)  || (hour == 8 && minute == 30) ||
            (hour == 8 && minute == 35) || (hour == 9 && minute == 10) ||
            (hour == 9 && minute == 20) || (hour == 9 && minute == 55) ||
            (hour == 10 && minute == 5) || (hour == 10 && minute == 40) ||
            (hour == 11 && minute == 0) || (hour == 11 && minute == 35) ||
            (hour == 11 && minute == 45)|| (hour == 12 && minute == 20) ||
            (hour == 12 && minute == 30)|| (hour == 13 && minute == 5) ||
            (hour == 13 && minute == 15)|| (hour == 13 && minute == 50) ||
            (hour == 14 && minute == 0) || (hour == 14 && minute == 30) ||
            (hour == 14 && minute == 35)|| (hour == 15 && minute == 10) ||
            (hour == 15 && minute == 20)|| (hour == 15 && minute == 55) ||
            (hour == 16 && minute == 5) || (hour == 16 && minute == 40) ||
            (hour == 17 && minute == 0) || (hour == 17 && minute == 35) ||
            (hour == 17 && minute == 45)|| (hour == 18 && minute == 20) ||
            (hour == 18 && minute == 25)|| (hour == 19 && minute == 0))
            return true;
    }
    return false;
}

void updateLCD(int hour, int minute, int second, int wday) {
    int dispHour = hour + chasi;
    if (dispHour >= 24) dispHour -= 24;
    if (dispHour < 0) dispHour += 24;

    lcd.setCursor(0, 0);
    char buf[17];
    snprintf(buf, 17, "%02d:%02d:%02d Ch%+d", dispHour, minute, second, chasi);
    lcd.print(buf);

    lcd.setCursor(0, 1);
    if (ringBlink && millis() < ringBlinkStop) {
        lcd.print("    RING!      ");
        if (millis() >= ringBlinkStop) ringBlink = false;
    } else {
        const char* fazaStr = (Faza == 0) ? "Zvon:On " : "Zvon:Off";
        const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        snprintf(buf, 17, "%s %s", fazaStr, days[wday % 7]);
        lcd.print(buf);
    }
}

bool getCurrentTime(struct tm &timeinfo, DateTime &rtcNow) {
    // Пытаемся получить время через NTP (таймаут 2 секунды)
    if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 2000)) {
        // Успех: используем NTP
        Serial.println("Время получено через NTP");
        return true;
    } else {
        // Нет интернета или NTP недоступен – берём из RTC
        if (rtc.isrunning()) {
            rtcNow = rtc.now();
            timeinfo.tm_year = rtcNow.year() - 1900;
            timeinfo.tm_mon  = rtcNow.month() - 1;
            timeinfo.tm_mday = rtcNow.day();
            timeinfo.tm_hour = rtcNow.hour();
            timeinfo.tm_min  = rtcNow.minute();
            timeinfo.tm_sec  = rtcNow.second();
            timeinfo.tm_wday = rtcNow.dayOfTheWeek();
            Serial.println("Время получено из RTC (интернет отсутствует)");
            return true;
        } else {
            Serial.println("Ошибка: нет интернета и RTC не работает!");
            return false;
        }
    }
}


int zvonki() {
    struct tm timeinfo;
    DateTime rtcNow;  // не используется, если получили из NTP
    
    bool timeOk = getCurrentTime(timeinfo, rtcNow);
    if (!timeOk) return 1;

    // Применяем смещение часов (chasi)
    int time_wday = timeinfo.tm_wday;
    int time_hr = timeinfo.tm_hour + chasi;
    int time_min = timeinfo.tm_min;
    int time_sec = timeinfo.tm_sec;

    // Коррекция перехода через 24 часа
    if (time_hr >= 24) time_hr -= 24;
    if (time_hr < 0) time_hr += 24;

    Serial.printf("День: %d, Время: %02d:%02d:%02d\n", time_wday, time_hr, time_min, time_sec);

    // Отправка времени в MQTT (топик test/timenow)
    char msg[60];
    snprintf(msg, sizeof(msg), "День:%d,Время:%02d:%02d:%02d", time_wday, time_hr, time_min, time_sec);
    if (client.connected()) {
        client.publish("test/timenow", msg);
    }

    // Проверка расписания и звонок
    if (Faza == 0) {
        if (shouldRing(time_wday, time_hr, time_min, time_sec)) {
            ring();
        }
    }

    // Обновление LCD (раз в секунду)
    if (millis() - lastLcdUpdate >= 1000) {
        updateLCD(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_wday);
        lastLcdUpdate = millis();
    }

    // Если интернет есть и прошёл час – синхронизируем RTC с NTP
    if (WiFi.status() == WL_CONNECTED && (millis() - lastRtcSync > RTC_SYNC_INTERVAL)) {
        struct tm ntpTime;
        if (getLocalTime(&ntpTime, 2000)) {
            DateTime now(ntpTime.tm_year + 1900, ntpTime.tm_mon + 1, ntpTime.tm_mday,
                        ntpTime.tm_hour, ntpTime.tm_min, ntpTime.tm_sec);
            rtc.adjust(now);
            Serial.println("RTC синхронизирован с NTP");
        }
        lastRtcSync = millis();
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
    digitalWrite(rele, HIGH);

    // Инициализация LCD
    Wire.begin();  // SDA=21, SCL=22
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  ZVONOK RTC   ");
    lcd.setCursor(0, 1);
    lcd.print("   Starting    ");

    // Инициализация RTC HW-111
    if (!rtc.begin()) {
        Serial.println("Ошибка: RTC HW-111 не найден!");
        lcd.setCursor(0, 1);
        lcd.print(" RTC NOT FOUND ");
    } else {
        Serial.println("RTC HW-111 найден");
        if (!rtc.isrunning()) {
            Serial.println("RTC не работает (нет батарейки или она разряжена)");
            lcd.setCursor(0, 1);
            lcd.print(" RTC NO BATTERY");
        } else {
            Serial.println("RTC работает");
        }
    }

    // Подключение к Wi-Fi
    WiFi.begin(ssid, pass);
    Serial.print("Подключение к WiFi");
    lcd.setCursor(0, 1);
    lcd.print(" WiFi connect..");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi подключен!");

    // Настройка NTP (всегда, даже если нет интернета – просто установка параметров)
    configTime(gmtOffset_sec, daylightOffset_sec, server);
    Serial.println("NTP настроен");

    // Первичная синхронизация RTC с NTP (если интернет есть)
    if (WiFi.status() == WL_CONNECTED) {
        struct tm ntpTime;
        if (getLocalTime(&ntpTime, 3000)) {
            DateTime now(ntpTime.tm_year + 1900, ntpTime.tm_mon + 1, ntpTime.tm_mday,
                        ntpTime.tm_hour, ntpTime.tm_min, ntpTime.tm_sec);
            rtc.adjust(now);
            Serial.println("RTC первично синхронизирован с NTP");
        } else {
            Serial.println("Не удалось получить NTP-время при старте");
        }
    }

    // Подключение к MQTT
    client.set_callback(callback);
}

void loop() {
    // Переподключение Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, pass);
        delay(5000);
        return;
    }

    // Переподключение MQTT
    if (!client.connected()) {
        Serial.println("Подключение к MQTT...");
        if (client.connect(MQTT::Connect("Zvonok")
            .set_auth(mqtt_user, mqtt_pass))) {
            Serial.println("MQTT подключен");
            client.subscribe("test/button");
            client.subscribe("test/chasi");
        } else {
            Serial.println("Ошибка MQTT");
            delay(5000);
            return;
        }
    }

    // Основной цикл
    if (client.connected()) {
        digitalWrite(LED, HIGH);
        client.loop();
        zvonki();
        digitalWrite(LED, LOW);
    }

    delay(1000);
}
