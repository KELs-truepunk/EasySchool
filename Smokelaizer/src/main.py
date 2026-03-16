import os
import datetime
from flask import Flask, request, jsonify
from werkzeug.utils import secure_filename, send_from_directory
import logging

# Настройка логирования
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# Конфигурация
UPLOAD_FOLDER = 'uploads'  # Папка для сохранения фото
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'gif', 'bmp'}
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # Максимальный размер файла 16MB

# Создаем папку для загрузок, если её нет
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH


def allowed_file(filename):
    """Проверяем, что файл имеет допустимое расширение"""
    return '.' in filename and \
        filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def generate_filename(original_filename):
    """Генерируем уникальное имя файла с timestamp"""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    if original_filename and '.' in original_filename:
        extension = original_filename.rsplit('.', 1)[1].lower()
    else:
        extension = 'jpg'
    return f"photo_{timestamp}.{extension}"


@app.route('/')
def index():
    """Главная страница - информация о сервере"""
    return '''
    <!DOCTYPE html>
    <html>
    <head>
        <title>Сервер для приема фото</title>
        <style>
        </style>
    </head>
    <body>
        <div class="container">
            <h1>📷 Сервер для приема фотографий</h1>
            <div class="info">
                <p>Сервер запущен и готов принимать фотографии!</p>
                <p><strong>Папка для сохранения:</strong> <code>{}</code></p>
                <p><strong>IP адрес сервера:</strong> <code>{}</code></p>
                <p><strong>Порт:</strong> <code>{}</code></p>
            </div>

            <h2>📤 Доступные эндпоинты:</h2>

            <div class="endpoint">
                <h3>1. Загрузка фото (POST)</h3>
                <p><code>POST /upload</code></p>
                <p>Принимает файл с ключом <code>file</code> в multipart/form-data</p>
            </div>

            <div class="endpoint">
                <h3>2. Проверка состояния (GET)</h3>
                <p><code>GET /status</code></p>
                <p>Возвращает статус сервера и статистику</p>
            </div>

            <div class="endpoint">
                <h3>3. Просмотр всех фото (GET)</h3>
                <p><code>GET /photos</code></p>
                <p>Список всех загруженных фотографий</p>
            </div>

            <h2>📋 Пример отправки с помощью curl:</h2>
            <pre><code>curl -X POST -F "file=@photo.jpg" http://{}:{}/upload</code></pre>

            <p style="margin-top: 30px; color: #666;">
                Сервер разработан для проектов по робототехнике
            </p>
        </div>
    </body>
    </html>
    '''.format(
        os.path.abspath(UPLOAD_FOLDER),
        get_local_ip(),
        5000,
        get_local_ip(),
        5000
    )


@app.route('/upload', methods=['POST'])
def upload_file():
    """Эндпоинт для загрузки фотографий"""
    try:
        # Проверяем, есть ли файл в запросе
        if 'file' not in request.files:
            logger.warning('Запрос без файла')
            return jsonify({
                'success': False,
                'error': 'Файл не найден в запросе. Используйте ключ "file".'
            }), 400

        file = request.files['file']

        # Если пользователь не выбрал файл
        if file.filename == '':
            logger.warning('Пустое имя файла')
            return jsonify({
                'success': False,
                'error': 'Файл не выбран'
            }), 400

        # Проверяем расширение файла
        if not allowed_file(file.filename):
            logger.warning(f'Недопустимое расширение файла: {file.filename}')
            return jsonify({
                'success': False,
                'error': f'Недопустимый тип файла. Разрешены: {", ".join(ALLOWED_EXTENSIONS)}'
            }), 400

        # Генерируем уникальное имя файла
        original_filename = secure_filename(file.filename)
        new_filename = generate_filename(original_filename)
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], new_filename)

        # Сохраняем файл
        file.save(filepath)

        # Получаем дополнительную информацию из запроса (если есть)
        metadata = {
            'timestamp': datetime.datetime.now().isoformat(),
            'original_name': original_filename,
            'saved_name': new_filename,
            'size': os.path.getsize(filepath),
            'camera_info': request.form.get('camera_info', 'Не указано'),
            'robot_id': request.form.get('robot_id', 'Не указано')
        }

        logger.info(f'Файл сохранен: {new_filename} ({metadata["size"]} байт)')

        # Возвращаем успешный ответ
        return jsonify({
            'success': True,
            'message': 'Фото успешно загружено',
            'filename': new_filename,
            'filepath': filepath,
            'metadata': metadata,
            'download_url': f'/download/{new_filename}'
        }), 200

    except Exception as e:
        logger.error(f'Ошибка при загрузке файла: {str(e)}')
        return jsonify({
            'success': False,
            'error': f'Внутренняя ошибка сервера: {str(e)}'
        }), 500


@app.route('/status', methods=['GET'])
def status():
    """Возвращает статус сервера и статистику"""
    files = os.listdir(UPLOAD_FOLDER)
    image_files = [f for f in files if allowed_file(f)]

    total_size = sum(os.path.getsize(os.path.join(UPLOAD_FOLDER, f)) for f in image_files)

    return jsonify({
        'status': 'running',
        'server_time': datetime.datetime.now().isoformat(),
        'upload_folder': os.path.abspath(UPLOAD_FOLDER),
        'total_photos': len(image_files),
        'total_size_mb': round(total_size / (1024 * 1024), 2),
        'allowed_extensions': list(ALLOWED_EXTENSIONS),
        'ip_address': get_local_ip(),
        'port': 5000
    })


@app.route('/photos', methods=['GET'])
def list_photos():
    """Возвращает список всех загруженных фото"""
    files = os.listdir(UPLOAD_FOLDER)
    image_files = [f for f in files if allowed_file(f)]

    # Сортируем по дате изменения (сначала новые)
    image_files.sort(key=lambda x: os.path.getmtime(os.path.join(UPLOAD_FOLDER, x)), reverse=True)

    photos_info = []
    for filename in image_files:
        filepath = os.path.join(UPLOAD_FOLDER, filename)
        stats = os.stat(filepath)
        photos_info.append({
            'filename': filename,
            'size_kb': round(stats.st_size / 1024, 2),
            'created': datetime.datetime.fromtimestamp(stats.st_ctime).isoformat(),
            'modified': datetime.datetime.fromtimestamp(stats.st_mtime).isoformat(),
            'download_url': f'/download/{filename}',
            'view_url': f'/view/{filename}'
        })

    return jsonify({
        'count': len(photos_info),
        'photos': photos_info
    })


@app.route('/download/<filename>', methods=['GET'])
def download_file(filename):
    """Скачивание файла"""
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename, as_attachment=True)


@app.route('/view/<filename>', methods=['GET'])
def view_file(filename):
    """Просмотр файла в браузере"""
    from flask import send_from_directory
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)


def get_local_ip():
    """Получаем локальный IP адрес компьютера"""
    import socket
    try:
        # Создаем временное соединение, чтобы узнать IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def find_available_port(start_port=5000, max_attempts=100):
    """Находит свободный порт"""
    import socket
    for port in range(start_port, start_port + max_attempts):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.bind(('', port))
                return port
        except OSError:
            continue
    return start_port


if __name__ == '__main__':
    # Находим свободный порт
    port = find_available_port(5000)

    # Получаем IP адрес
    ip = get_local_ip()

    print("=" * 60)
    print("Запуск сервера")
    print("=" * 60)
    print(f"Папка для сохранения фото с ESP32-CAM: {os.path.abspath(UPLOAD_FOLDER)}")
    print(f"Локальный адрес: http://localhost:{port}")
    print(f"Сетевой адрес: http://{ip}:{port}")
    print("=" * 60)
    print("Для отправки фото используйте POST запрос на /upload")
    print("Для проверки статуса: GET /status")
    print("Для списка фото: GET /photos")
    print("=" * 60)

    # Запускаем сервер
    app.run(host='0.0.0.0', port=port, debug=True)
