#!/bin/bash
mnt_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../mnt" && pwd)" #Отредактировать для выбора пути монтирования системы!
./bin/jsonfs Json_Files/test.json "$mnt_dir" &
sleep 2
echo "JSONFS запущен. Используйте stop.sh или 'sudo fusermount -u mnt' для остановки программы"