#!/bin/bash

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Функции для вывода
print_ok() { echo -e "${GREEN}✅ $1${NC}"; }
print_error() { echo -e "${RED}❌ $1${NC}"; }
print_info() { echo -e "${BLUE}📌 $1${NC}"; }
print_step() { echo -e "${YELLOW}=== $1 ===${NC}"; }

# Переменные
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

JSON_FILE="Json_Files/data.json"
MOUNT_POINT="mnt"
TEST_PASSED=0
TEST_FAILED=0

# Функция проверки команды
check_command() {
    if [ $? -eq 0 ]; then
        print_ok "$1"
        ((TEST_PASSED++))
    else
        print_error "$1"
        ((TEST_FAILED++))
    fi
}

# Функция проверки содержимого файла
check_content() {
    local file=$1
    local expected=$2
    local description=$3
    
    if [ -f "$file" ]; then
        local content=$(sudo cat "$file" 2>/dev/null)
        if [ "$content" = "$expected" ]; then
            print_ok "$description (ожидалось: $expected, получено: $content)"
            ((TEST_PASSED++))
        else
            print_error "$description (ожидалось: $expected, получено: $content)"
            ((TEST_FAILED++))
        fi
    else
        print_error "$description (файл не найден: $file)"
        ((TEST_FAILED++))
    fi
}

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                 JSONFS ФУНКЦИОНАЛЬНЫЙ ТЕСТ                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ========== ШАГ 1: Подготовка ==========
print_step "ШАГ 1: Подготовка и очистка"

sudo pkill -f "bin/jsonfs" 2>/dev/null
sudo fusermount -u "$MOUNT_POINT" 2>/dev/null
sudo umount "$MOUNT_POINT" 2>/dev/null
rm -rf "$MOUNT_POINT"
mkdir -p "$MOUNT_POINT"
mkdir -p Json_Files
sudo chown -R $(whoami):$(whoami) Json_Files 2>/dev/null
chmod 755 Json_Files 2>/dev/null

print_ok "Очистка выполнена"
print_ok "Точка монтирования создана: $MOUNT_POINT"

# ========== ШАГ 2: Создание тестового JSON ==========
print_step "ШАГ 2: Создание тестового JSON файла"

cat > "$JSON_FILE" << 'EOF'
{
  "string": "Hello World",
  "number": 42,
  "boolean": true,
  "null_value": null
}
EOF

if [ -f "$JSON_FILE" ]; then
    print_ok "JSON файл создан: $JSON_FILE"
    echo "  Содержимое:"
    cat "$JSON_FILE" | sed 's/^/    /'
else
    print_error "Не удалось создать JSON файл"
    exit 1
fi

# ========== ШАГ 3: Запуск JSONFS ==========
print_step "ШАГ 3: Запуск JSONFS"

sudo ./bin/jsonfs "$JSON_FILE" "$MOUNT_POINT" > /tmp/jsonfs_test.log 2>&1 &
JSONFS_PID=$!
sleep 3

if mountpoint -q "$MOUNT_POINT"; then
    print_ok "JSONFS успешно запущен (PID: $JSONFS_PID)"
    print_ok "Точка монтирования: $MOUNT_POINT"
else
    print_error "Не удалось запустить JSONFS"
    cat /tmp/jsonfs_test.log
    exit 1
fi

# ========== ШАГ 4: Проверка корневой директории ==========
print_step "ШАГ 4: Проверка корневой директории"

sudo ls -la "$MOUNT_POINT/" > /dev/null 2>&1
check_command "Чтение корневой директории"

# ========== ШАГ 5: Чтение существующих файлов ==========
print_step "ШАГ 5: Чтение существующих файлов"

check_content "$MOUNT_POINT/string" "Hello World" "Чтение строки"
check_content "$MOUNT_POINT/number" "42" "Чтение числа"
check_content "$MOUNT_POINT/boolean" "true" "Чтение булева значения"
check_content "$MOUNT_POINT/null_value" "null" "Чтение null значения"

# ========== ШАГ 6: Создание файлов разных типов ==========
print_step "ШАГ 6: Создание файлов разных типов"

echo "New String" | sudo tee "$MOUNT_POINT/new_string" > /dev/null
check_command "Создание строки"

echo "12345" | sudo tee "$MOUNT_POINT/new_number" > /dev/null
check_command "Создание числа"

echo "false" | sudo tee "$MOUNT_POINT/new_boolean" > /dev/null
check_command "Создание булева значения (false)"

echo "null" | sudo tee "$MOUNT_POINT/new_null" > /dev/null
check_command "Создание null значения"

echo "3.14159" | sudo tee "$MOUNT_POINT/new_float" > /dev/null
check_command "Создание числа с плавающей точкой"

# ========== ШАГ 7: Проверка созданных файлов ==========
print_step "ШАГ 7: Проверка созданных файлов"

check_content "$MOUNT_POINT/new_string" "New String" "Проверка созданной строки"
check_content "$MOUNT_POINT/new_number" "12345" "Проверка созданного числа"
check_content "$MOUNT_POINT/new_boolean" "false" "Проверка булева значения"
check_content "$MOUNT_POINT/new_float" "3.14159" "Проверка числа с плавающей точкой"

# ========== ШАГ 8: Создание вложенной структуры ==========
print_step "ШАГ 8: Создание вложенной структуры (директории)"

sudo mkdir "$MOUNT_POINT/user"
check_command "Создание директории user"

echo "Alice" | sudo tee "$MOUNT_POINT/user/name" > /dev/null
check_command "Создание файла в вложенной директории"

echo "25" | sudo tee "$MOUNT_POINT/user/age" > /dev/null
check_command "Создание числа в вложенной директории"

sudo mkdir "$MOUNT_POINT/user/address"
check_command "Создание поддиректории address"

echo "Main St" | sudo tee "$MOUNT_POINT/user/address/street" > /dev/null
check_command "Создание файла в поддиректории"

echo "12345" | sudo tee "$MOUNT_POINT/user/address/zip" > /dev/null
check_command "Создание числа в поддиректории"

# ========== ШАГ 9: Создание массива ==========
print_step "ШАГ 9: Создание массива (через индексы)"

sudo mkdir "$MOUNT_POINT/colors"
check_command "Создание директории для массива"

echo "red" | sudo tee "$MOUNT_POINT/colors/0" > /dev/null
echo "green" | sudo tee "$MOUNT_POINT/colors/1" > /dev/null
echo "blue" | sudo tee "$MOUNT_POINT/colors/2" > /dev/null
check_command "Создание элементов массива (red, green, blue)"

# ========== ШАГ 10: Сохранение изменений ==========
print_step "ШАГ 10: Сохранение изменений"

echo "save" | sudo tee "$MOUNT_POINT/.save" > /dev/null
check_command "Сохранение через .save"

MODIFIED=$(sudo cat "$MOUNT_POINT/.modified" 2>/dev/null | tr -d '\n')
if [ "$MODIFIED" = "0" ]; then
    print_ok "Статус изменений = 0 (сохранено)"
    ((TEST_PASSED++))
else
    print_error "Статус изменений = $MODIFIED (ожидалось 0)"
    ((TEST_FAILED++))
fi

# ========== ШАГ 11: Проверка JSON после сохранения ==========
print_step "ШАГ 11: Проверка JSON файла после сохранения"

if [ -f "$JSON_FILE" ]; then
    print_ok "JSON файл существует"
    HAS_STRING=$(grep -c '"new_string"' "$JSON_FILE")
    HAS_NUMBER=$(grep -c '"new_number"' "$JSON_FILE")
    HAS_USER=$(grep -c '"user"' "$JSON_FILE")
    HAS_COLORS=$(grep -c '"colors"' "$JSON_FILE")
    
    if [ $HAS_STRING -gt 0 ] && [ $HAS_NUMBER -gt 0 ]; then
        print_ok "Новые поля найдены в JSON"
        ((TEST_PASSED++))
    else
        print_error "Новые поля не найдены в JSON"
        ((TEST_FAILED++))
    fi
fi

# ========== ШАГ 12: Обновление существующих файлов ==========
print_step "ШАГ 12: Обновление существующих файлов"

echo "Updated String" | sudo tee "$MOUNT_POINT/string" > /dev/null
check_command "Обновление строки"

echo "999" | sudo tee "$MOUNT_POINT/number" > /dev/null
check_command "Обновление числа"

echo "false" | sudo tee "$MOUNT_POINT/boolean" > /dev/null
check_command "Обновление булева значения"

echo "save" | sudo tee "$MOUNT_POINT/.save" > /dev/null
check_command "Сохранение после обновлений"

# ========== ШАГ 13: Проверка обновлений ==========
print_step "ШАГ 13: Проверка обновленных значений"

check_content "$MOUNT_POINT/string" "Updated String" "Проверка обновленной строки"
check_content "$MOUNT_POINT/number" "999" "Проверка обновленного числа"
check_content "$MOUNT_POINT/boolean" "false" "Проверка обновленного булева значения"

# ========== ШАГ 14: Удаление файлов ==========
print_step "ШАГ 14: Удаление файлов"

sudo rm "$MOUNT_POINT/new_string" 2>/dev/null
check_command "Удаление строки"

sudo rm "$MOUNT_POINT/new_number" 2>/dev/null
check_command "Удаление числа"

sudo rm "$MOUNT_POINT/new_boolean" 2>/dev/null
check_command "Удаление булева значения"

sudo rm "$MOUNT_POINT/new_float" 2>/dev/null
check_command "Удаление числа с плавающей точкой"

sudo rm "$MOUNT_POINT/new_null" 2>/dev/null
check_command "Удаление null значения"

# ========== ШАГ 15: Удаление директорий ==========
print_step "ШАГ 15: Удаление директорий"

sudo rm -rf "$MOUNT_POINT/user" 2>/dev/null
check_command "Удаление директории user"

sudo rm -rf "$MOUNT_POINT/colors" 2>/dev/null
check_command "Удаление директории colors"

echo "save" | sudo tee "$MOUNT_POINT/.save" > /dev/null
check_command "Сохранение после удалений"

# ========== ШАГ 16: Проверка финального JSON ==========
print_step "ШАГ 16: Проверка финального JSON"

if [ -f "$JSON_FILE" ]; then
    HAS_DELETED=$(grep -c '"new_string\|new_number\|user\|colors"' "$JSON_FILE")
    if [ $HAS_DELETED -eq 0 ]; then
        print_ok "Удаленные поля отсутствуют в JSON"
        ((TEST_PASSED++))
    else
        print_error "Удаленные поля все еще присутствуют в JSON"
        ((TEST_FAILED++))
    fi
    
    echo ""
    echo "  Финальное содержимое JSON:"
    cat "$JSON_FILE" | python3 -m json.tool 2>/dev/null | sed 's/^/    /' || cat "$JSON_FILE" | sed 's/^/    /'
fi

# ========== ШАГ 17: Проверка специальных файлов ==========
print_step "ШАГ 17: Проверка специальных файлов"

if sudo test -f "$MOUNT_POINT/.help"; then
    print_ok "Файл .help существует"
    ((TEST_PASSED++))
else
    print_error "Файл .help не существует"
    ((TEST_FAILED++))
fi

# ========== ШАГ 18: Размонтирование ==========
print_step "ШАГ 18: Размонтирование"

sudo fusermount -u "$MOUNT_POINT" 2>/dev/null
check_command "Размонтирование"

sudo pkill -f "bin/jsonfs" 2>/dev/null

# ========== ИТОГИ ==========
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                       РЕЗУЛЬТАТЫ ТЕСТА                       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

echo -e "${GREEN}✅ Пройдено: $TEST_PASSED${NC}"
echo -e "${RED}❌ Не пройдено: $TEST_FAILED${NC}"
echo ""

if [ $TEST_FAILED -eq 0 ]; then
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║              ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО! 🎉                 ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "JSONFS полностью готов к использованию!"
    echo ""
    echo "Пример использования:"
    echo "  sudo ./bin/jsonfs Json_Files/data.json mnt"
    echo "  echo 'hello' | sudo tee mnt/newfile"
    echo "  echo 'save' | sudo tee mnt/.save"
    echo "  sudo fusermount -u mnt"
else
    echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║              НЕКОТОРЫЕ ТЕСТЫ НЕ ПРОЙДЕНЫ                     ║${NC}"
    echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Проверьте логи и повторите попытку."
fi

# Очистка
rm -f /tmp/jsonfs_test.log

# Возвращаем код ошибки если есть падения
if [ $TEST_FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi