FROM python:3.12-slim

WORKDIR /app
COPY requierements.txt .
RUN pip install --no-cache-dir -r requierements.txt

COPY . .

EXPOSE 8090

CMD ["python", "main.py"]
