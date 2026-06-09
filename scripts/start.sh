#!/bin/bash
./bin/jsonfs Json_Files/test.json /tmp/mnt & #Отредактировать для выбора пути монтирования системы!
sleep 2
echo "JSONFS запущен. Используйте 'sudo fusermount -u mnt' для остановки"