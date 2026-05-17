#!/bin/bash

echo "=== JSONFS Тест ==="

# Переходим в директорию проекта
cd "$(dirname "$0")"

# Создаем папки проекта
mkdir -p Json_Files
mkdir -p mnt

# Создаем тестовый JSON напрямую (без лишних операций)
echo '{
  "test": "value",
  "number": 123,
  "nested": {
    "key": "data"
  }
}' | sudo tee Json_Files/test.json > /dev/null

sudo chmod 644 Json_Files/test.json

# Получаем абсолютные пути
JSON_PATH="$(pwd)/Json_Files/test.json"
MOUNT_POINT="$(pwd)/mnt"

echo "✅ JSON файл: $JSON_PATH"
echo "✅ Точка монтирования: $MOUNT_POINT"

# Проверяем существует ли JSON файл
if [ ! -f "$JSON_PATH" ]; then
    echo "❌ JSON файл не создан!"
    exit 1
fi

echo "Содержимое JSON:"
cat "$JSON_PATH"
echo ""

# Очистка
sudo pkill -f "bin/jsonfs" 2>/dev/null || true
sudo fusermount -u "$MOUNT_POINT" 2>/dev/null || sudo umount "$MOUNT_POINT" 2>/dev/null || true
sleep 1

# Запускаем JSONFS
echo "🚀 Запуск JSONFS..."
sudo ./bin/jsonfs "$JSON_PATH" "$MOUNT_POINT" &
PID=$!

# Ждем монтирования
sleep 3

# Проверяем монтирование
if mountpoint -q "$MOUNT_POINT"; then
    echo "✅ JSONFS смонтирован"
    
    echo ""
    echo "=== Тестирование операций ==="
    
    # Создание файлов
    echo "📝 Создание файлов..."
    echo "hello" | sudo tee "$MOUNT_POINT/file1" > /dev/null
    echo "12345" | sudo tee "$MOUNT_POINT/number" > /dev/null
    echo "true" | sudo tee "$MOUNT_POINT/flag" > /dev/null
    sudo mkdir "$MOUNT_POINT/dir" 2>/dev/null
    echo "inside" | sudo tee "$MOUNT_POINT/dir/file" > /dev/null
    
    # Сохранение
    echo "save" | sudo tee "$MOUNT_POINT/.save" > /dev/null
    
    # Чтение
    echo "📖 Чтение файлов:"
    echo "  file1: $(sudo cat "$MOUNT_POINT/file1" 2>/dev/null)"
    echo "  number: $(sudo cat "$MOUNT_POINT/number" 2>/dev/null)"
    echo "  flag: $(sudo cat "$MOUNT_POINT/flag" 2>/dev/null)"
    echo "  modified: $(sudo cat "$MOUNT_POINT/.modified" 2>/dev/null)"
    
    # Список
    echo "📁 Содержимое mnt:"
    sudo ls -la "$MOUNT_POINT/"
    
    # Удаление
    echo "🗑️ Удаление файлов..."
    sudo rm "$MOUNT_POINT/file1" 2>/dev/null
    sudo rm -rf "$MOUNT_POINT/dir" 2>/dev/null
    
    # Финальное сохранение
    echo "save" | sudo tee "$MOUNT_POINT/.save" > /dev/null
    
    echo "✅ Тесты выполнены"
    
else
    echo "❌ Ошибка монтирования"
    echo "Проверьте процесс:"
    ps aux | grep jsonfs
    exit 1
fi

echo ""
echo "=== Завершение ==="

# Сначала размонтируем
echo "🔗 Размонтирование..."
sudo fusermount -u "$MOUNT_POINT" 2>/dev/null
sleep 1

# Останавливаем процесс JSONFS
echo "🛑 Остановка JSONFS..."
sudo kill $PID 2>/dev/null
sleep 1

# Если всё еще висит, принудительно
if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    echo "⚠️ Принудительное завершение..."
    sudo pkill -9 -f "bin/jsonfs"
    sleep 1
    sudo umount -l "$MOUNT_POINT" 2>/dev/null
fi

echo ""
echo "=== Результат ==="
echo "Содержимое JSON после теста:"
cat "$JSON_PATH" | python3 -m json.tool 2>/dev/null || cat "$JSON_PATH"
echo ""
echo "✅ Тест завершен"