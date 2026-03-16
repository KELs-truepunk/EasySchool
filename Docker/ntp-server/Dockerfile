FROM alpine:3.18

RUN apk add --no-cache chrony tzdata

# Настройка часового пояса Лондон
RUN cp /usr/share/zoneinfo/Europe/London /etc/localtime && \
    echo "Europe/London" > /etc/timezone

# Создаём необходимые директории
RUN mkdir -p /var/lib/chrony /var/run/chrony

# Конфигурация ТОЛЬКО для локального времени (без fudge)
RUN echo "# Автономный NTP сервер" > /etc/chrony/chrony.conf && \
    echo "server 127.127.1.0" >> /etc/chrony/chrony.conf && \
    echo "allow all" >> /etc/chrony/chrony.conf && \
    echo "local stratum 10" >> /etc/chrony/chrony.conf && \
    echo "driftfile /var/lib/chrony/chrony.drift" >> /etc/chrony/chrony.conf && \
    echo "pidfile /var/run/chrony/chronyd.pid" >> /etc/chrony/chrony.conf && \
    echo "makestep 1.0 3" >> /etc/chrony/chrony.conf

# Создаём пустой drift-файл
RUN touch /var/lib/chrony/chrony.drift

EXPOSE 123/udp

CMD ["/usr/sbin/chronyd", "-d", "-s", "-f", "/etc/chrony/chrony.conf"]
