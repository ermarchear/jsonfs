CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-unused-parameter -g $(shell pkg-config fuse3 --cflags 2>/dev/null || pkg-config fuse --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs 2>/dev/null || pkg-config fuse --libs) -ljansson

SRCDIR = src
INCDIR = include
OBJDIR = objects
BINDIR = bin
JSONDIR = Json_Files
MOUNTPOINT = mnt

$(shell mkdir -p $(OBJDIR) $(BINDIR) $(JSONDIR) $(MOUNTPOINT))

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jsonfs

.PHONY: all clean test create-test-json run mount umount start stop valgrind-test help

# Проверка и настройка FUSE (выполняется один раз при первой сборке)
# Отдельная команда для настройки (пользователь запускает если нужно)

all: $(TARGET)
	@echo "✅ Сборка завершена"

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/jsonfs.h
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	@echo "🧹 Очистка..."
	rm -rf $(OBJDIR)/*.o $(TARGET)
	@echo "✅ Очистка завершена"

setup:
	@if [ ! -w /dev/fuse ]; then \
		echo "=== Настройка FUSE ==="; \
		if ! getent group fuse > /dev/null 2>&1; then \
			sudo groupadd fuse; \
		fi; \
		if ! groups $(USER) | grep -q fuse; then \
			sudo usermod -aG fuse $(USER); \
		fi; \
		sudo chmod 666 /dev/fuse; \
		if [ -f /etc/fuse.conf ] && ! grep -q "^user_allow_other" /etc/fuse.conf 2>/dev/null; then \
			echo "user_allow_other" | sudo tee -a /etc/fuse.conf > /dev/null; \
		fi; \
		echo "=========================================="; \
		echo "Настройка завершена! Перезайдите в систему."; \
		echo "=========================================="; \
	else \
		echo "FUSE уже настроен"; \
	fi

# Создание тестового JSON файла
create-test-json:
	@mkdir -p $(JSONDIR)
	@echo '{"test": "value", "number": 123, "nested": {"key": "data"}}' > $(JSONDIR)/test.json
	@echo "✅ Создан test.json в $(JSONDIR)/"

# Монтирование
mount: $(TARGET) create-test-json
	@echo "🔗 Монтирование JSONFS в ./$(MOUNTPOINT)"
	@mkdir -p $(MOUNTPOINT)
	@sudo ./$(TARGET) $(JSONDIR)/test.json ./$(MOUNTPOINT)
	@echo "✅ Смонтировано. Используйте 'make umount' для размонтирования"

# Размонтирование
umount:
	@echo "🔗 Размонтирование ./$(MOUNTPOINT)"
	@-sudo fusermount -u ./$(MOUNTPOINT) 2>/dev/null || sudo umount ./$(MOUNTPOINT) 2>/dev/null
	@echo "✅ Размонтировано"

# Запуск в foreground
run: $(TARGET) create-test-json
	@mkdir -p $(MOUNTPOINT)
	@echo "🚀 Запуск JSONFS. Нажмите Ctrl+C для остановки."
	@echo "JSON файл: $(JSONDIR)/test.json"
	@echo "Точка монтирования: ./$(MOUNTPOINT)"
	@sudo ./$(TARGET) $(JSONDIR)/test.json ./$(MOUNTPOINT)

# Запуск в background
start: $(TARGET) create-test-json
	@mkdir -p $(MOUNTPOINT)
	@echo "🚀 Запуск JSONFS в фоне..."
	@sudo ./$(TARGET) $(JSONDIR)/test.json ./$(MOUNTPOINT) > /tmp/jsonfs.log 2>&1 &
	@echo $$! | sudo tee /tmp/jsonfs.pid > /dev/null
	@sleep 2
	@if mountpoint -q ./$(MOUNTPOINT); then \
		echo "✅ JSONFS смонтирован в ./$(MOUNTPOINT)"; \
		echo "PID: $$(cat /tmp/jsonfs.pid)"; \
		echo "Лог: /tmp/jsonfs.log"; \
	else \
		echo "❌ Ошибка монтирования"; \
		echo "Проверьте лог: cat /tmp/jsonfs.log"; \
	fi

# Остановка
stop: umount
	@-sudo kill $$(cat /tmp/jsonfs.pid) 2>/dev/null || true
	@-sudo rm -f /tmp/jsonfs.pid /tmp/jsonfs.log
	@echo "✅ Остановлено"

# Тестирование
test: $(TARGET) create-test-json
	@echo "=== Запуск теста JSONFS ==="
	@mkdir -p $(MOUNTPOINT)
	@echo "Запуск JSONFS в фоне..."
	@sudo ./$(TARGET) $(JSONDIR)/test.json ./$(MOUNTPOINT) > /tmp/jsonfs.log 2>&1 &
	@echo $$! | sudo tee /tmp/jsonfs.pid > /dev/null
	@sleep 2
	@if ! mountpoint -q ./$(MOUNTPOINT); then \
		echo "❌ Ошибка монтирования"; \
		echo "Лог ошибок:"; \
		cat /tmp/jsonfs.log; \
		exit 1; \
	fi
	@echo "✅ JSONFS смонтирован"
	@echo "📝 Создание тестовых файлов..."
	@echo "hello world" | sudo tee ./$(MOUNTPOINT)/file1 > /dev/null
	@echo "12345" | sudo tee ./$(MOUNTPOINT)/number > /dev/null
	@echo "true" | sudo tee ./$(MOUNTPOINT)/flag > /dev/null
	@sudo mkdir ./$(MOUNTPOINT)/dir 2>/dev/null
	@echo "inside" | sudo tee ./$(MOUNTPOINT)/dir/file > /dev/null
	@echo "📁 Содержимое директории:"
	@ls -la ./$(MOUNTPOINT)/
	@echo "📖 Чтение file1:"
	@cat ./$(MOUNTPOINT)/file1
	@echo "💾 Сохранение изменений..."
	@echo "save" | sudo tee ./$(MOUNTPOINT)/.save > /dev/null
	@echo "📊 Статус изменений:"
	@cat ./$(MOUNTPOINT)/.modified
	@echo "🗑️ Удаление файлов..."
	@sudo rm ./$(MOUNTPOINT)/file1
	@sudo rm -rf ./$(MOUNTPOINT)/dir
	@echo "💾 Финальное сохранение..."
	@echo "save" | sudo tee ./$(MOUNTPOINT)/.save > /dev/null
	@echo "🔗 Размонтирование..."
	@sudo fusermount -u ./$(MOUNTPOINT) 2>/dev/null || sudo umount ./$(MOUNTPOINT) 2>/dev/null
	@sleep 1
	@sudo kill $$(cat /tmp/jsonfs.pid) 2>/dev/null || true
	@sudo rm -f /tmp/jsonfs.pid /tmp/jsonfs.log
	@echo "✅ Тест завершен"
	@echo "📄 Содержимое JSON файла после теста:"
	@cat $(JSONDIR)/test.json

# Очистка тестов
clean-test:
	@echo "🧹 Очистка тестовых файлов..."
	@-sudo fusermount -u $(MOUNTPOINT) 2>/dev/null || sudo umount $(MOUNTPOINT) 2>/dev/null
	@rm -rf $(JSONDIR)/*.json $(MOUNTPOINT) 2>/dev/null || true
	@mkdir -p $(JSONDIR) $(MOUNTPOINT)
	@sudo rm -f /tmp/jsonfs.pid /tmp/jsonfs.log /tmp/valgrind.pid
	@echo "✅ Тестовые файлы очищены"

# Valgrind тест
valgrind-test: $(TARGET) create-test-json
	@echo "=== Запуск valgrind теста ==="
	@mkdir -p $(MOUNTPOINT)
	@sudo valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--log-file=/tmp/valgrind.log ./$(TARGET) $(JSONDIR)/test.json ./$(MOUNTPOINT) &
	@echo $$! | sudo tee /tmp/valgrind.pid > /dev/null
	@sleep 3
	@echo "📝 Выполнение операций..."
	@echo "test" | sudo tee ./$(MOUNTPOINT)/testfile > /dev/null 2>&1 || true
	@cat ./$(MOUNTPOINT)/testfile > /dev/null 2>&1 || true
	@echo "save" | sudo tee ./$(MOUNTPOINT)/.save > /dev/null 2>&1 || true
	@sleep 1
	@echo "🔗 Размонтирование..."
	@sudo fusermount -u ./$(MOUNTPOINT) 2>/dev/null || sudo umount ./$(MOUNTPOINT) 2>/dev/null
	@sleep 1
	@sudo kill $$(cat /tmp/valgrind.pid) 2>/dev/null || true
	@echo "📊 Результаты valgrind:"
	@sudo grep -E "definitely lost|indirectly lost|possibly lost|ERROR SUMMARY" /tmp/valgrind.log
	@sudo rm -f /tmp/valgrind.pid /tmp/valgrind.log
	@echo "✅ Valgrind тест завершен"

help:
	@echo "📖 Доступные команды:"
	@echo "make                  - Собрать проект"
	@echo "make clean            - Очистить сборку"
	@echo "make setup            - Настроить сборку"
	@echo "make test             - Запустить тестирование"
	@echo "make run              - Запустить JSONFS (интерактивно)"
	@echo "make start            - Запустить JSONFS в фоне"
	@echo "make stop             - Остановить JSONFS"
	@echo "make mount            - Смонтировать (интерактивно)"
	@echo "make umount           - Размонтировать"
	@echo "make valgrind-test    - Проверить утечки памяти"
	@echo "make clean-test       - Очистить тестовые файлы"
	@echo "make create-test-json - Создать тестовый JSON"