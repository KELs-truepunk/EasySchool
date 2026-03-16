/*
 * ESP32-CAM Клиент для отправки фото на сервер
 * Подключается к Wi-Fi, делает фото и отправляет на сервер
 */

// ========== НАСТРОЙКИ ==========
// Настройки Wi-Fi

/*
 * ПРОСТОЙ ESP32-CAM клиент
 * Минимальный код для отправки фото на сервер
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Настройки
const char* ssid = "RTK-308(2.4)";      // Имя вашей Wi-Fi сети
const char* password = "77725495066";  // Пароль от Wi-Fi

// Настройки сервера
const char* serverUrl = "http://192.168.0.38:5000/upload";  // Адрес вашего сервера

// Конфигурация камеры
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const int ledFlash = 4;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32-CAM Photo Uploader");
  Serial.println("========================");
  
  // Отключаем brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Настройка LED
  pinMode(ledFlash, OUTPUT);
  digitalWrite(ledFlash, LOW);
  
  // Подключаем Wi-Fi
  Serial.print("Подключение к Wi-Fi ");
  Serial.print(ssid);
  Serial.println(" ...");
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi подключен!");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nОшибка подключения к Wi-Fi!");
    while(1) {
      digitalWrite(ledFlash, HIGH);
      delay(100);
      digitalWrite(ledFlash, LOW);
      delay(1000);
    }
  }
  
  // Настройка камеры
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA; // 640x480
  config.jpeg_quality = 10;
  config.fb_count = 1;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while(1) delay(1000);
  }
  
  Serial.println("Камера готова!");
  Serial.println("Отправьте 'c' для съемки");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') {
      takeAndSendPhoto();
    }
  }
  delay(100);
}

void takeAndSendPhoto() {
  Serial.println("\n>>> Съемка фото <<<");
  
  // Включаем вспышку
  //digitalWrite(ledFlash, HIGH);
  delay(200);
  
  // Захватываем фото
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Ошибка захвата фото");
    digitalWrite(ledFlash, LOW);
    return;
  }
  
  // Выключаем вспышку
  digitalWrite(ledFlash, LOW);
  
  Serial.print("Размер фото: ");
  Serial.print(fb->len);
  Serial.println(" байт");
  
  // Отправляем на сервер
  bool result = uploadPhoto(fb);
  
  // Освобождаем память
  esp_camera_fb_return(fb);
  
  if (result) {
    Serial.println(">>> Фото отправлено успешно! <<<");
  } else {
    Serial.println(">>> Ошибка отправки фото <<<");
  }
}
bool uploadPhoto(camera_fb_t *fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi не подключен");
    return false;
  }
  
  HTTPClient http;
  
  Serial.print("Отправка на сервер: ");
  Serial.println(serverUrl);
  
  // Создаем multipart форму
  String boundary = "----------esp32cam";
  String header = "multipart/form-data; boundary=" + boundary;
  
  // Формируем тело запроса
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"photo.jpg\"\r\n";
  bodyStart += "Content-Type: image/jpeg\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  
  // Вычисляем размер
  int totalSize = bodyStart.length() + fb->len + bodyEnd.length();
  
  // Подготавливаем запрос
  http.begin(serverUrl);
  http.addHeader("Content-Type", header);
  http.addHeader("Content-Length", String(totalSize));
  
  // Создаем буфер для всех данных
  uint8_t* fullData = new uint8_t[totalSize];
  if (!fullData) {
    Serial.println("Ошибка выделения памяти");
    return false;
  }
  
  // Копируем все части в один буфер
  size_t offset = 0;
  
  // Копируем начало
  memcpy(fullData + offset, bodyStart.c_str(), bodyStart.length());
  offset += bodyStart.length();
  
  // Копируем фото
  memcpy(fullData + offset, fb->buf, fb->len);
  offset += fb->len;
  
  // Копируем конец
  memcpy(fullData + offset, bodyEnd.c_str(), bodyEnd.length());
  
  // Отправляем
  int httpCode = http.POST(fullData, totalSize);
  
  // Очищаем память
  delete[] fullData;
  
  // Проверяем результат
  Serial.print("HTTP код: ");
  Serial.println(httpCode);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("Ответ сервера: ");
    Serial.println(response);
  } else {
    Serial.print("Ошибка: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
  
  return (httpCode == 200);
}
