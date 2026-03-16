# Базовый образ
FROM eclipse-mosquitto:latest

# Переключаемся на root, чтобы менять права
USER root

# Копируем ваши папки внутрь образа
COPY mqtt-config /mosquitto/config
COPY mqtt-data   /mosquitto/data
COPY mqtt-log    /mosquitto/log

# Устанавливаем владельца mosquitto (UID 1883) на все скопированные файлы
RUN chown -R mosquitto:mosquitto /mosquitto/config /mosquitto/data /mosquitto/log

# Возвращаем пользователя mosquitto (как в оригинальном образе)
USER mosquitto

# (Опционально) Проверяем, что файлы на месте
RUN ls -la /mosquitto/config && ls -la /mosquitto/data && ls -la /mosquitto/log
