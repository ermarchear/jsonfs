#!/bin/bash
sudo fusermount -u mnt
sudo umount mnt
sudo pkill -f "bin/jsonfs"
echo "JSONFS остановлен"