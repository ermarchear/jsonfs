#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include "../include/jsonfs.h"

// Глобальная переменная для доступа из операций
struct jsonfs_data *GET_DATA = NULL;

// Структура операций FUSE
static struct fuse_operations operations = {
    .init    = fs_init,
    .destroy = fs_destroy,
    .getattr = fs_getattr,
    .opendir = fs_opendir,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
    .mknod   = fs_mknod,
    .mkdir   = fs_mkdir,
    .unlink  = fs_unlink,
    .rmdir   = fs_rmdir,
    .rename  = fs_rename,
    .utimens = fs_utimens,
};

static void show_help() {
    fprintf(stderr, "Использование: jsonfs <json-файл> <точка_монтирования> [опции]\n");
    fprintf(stderr, "Пример: ./jsonfs data.json /mnt/json -f\n");
    fprintf(stderr, "\nСпециальные файлы:\n");
    fprintf(stderr, "  /.save  - запись 1 сюда сохраняет изменения\n");
    fprintf(stderr, "  /.status- показывает статус (сохранено/не сохранено)\n");
}

int main(int argc, char **argv) {
    // Защита от root
    if (getuid() == 0 || geteuid() == 0) {
        fprintf(stderr, "Ошибка: нельзя запускать от root\n");
        return 1;
    }
    
    // Проверка аргументов
    if (argc < 3) {
        show_help();
        return 1;
    }
    
    // Первый аргумент после программы — JSON-файл
    const char *json_filename = argv[1];
    
    // Второй аргумент — точка монтирования
    const char *mountpoint = argv[2];
    
    printf("JSON файл: %s\n", json_filename);
    printf("Точка монтирования: %s\n", mountpoint);
    
    // Проверка существования JSON-файла
    if (access(json_filename, R_OK) != 0) {
        fprintf(stderr, "Ошибка: файл %s не существует или недоступен для чтения\n", json_filename);
        return 1;
    }
    
    // Проверка существования папки монтирования
    if (access(mountpoint, F_OK) != 0) {
        fprintf(stderr, "Ошибка: папка монтирования %s не существует\n", mountpoint);
        fprintf(stderr, "Создайте её: mkdir -p %s\n", mountpoint);
        return 1;
    }
    
    // Загружаем JSON
    struct json_object *root = json_object_from_file(json_filename);
    if (!root) {
        fprintf(stderr, "Ошибка: не удалось прочитать JSON из %s\n", json_filename);
        return 1;
    }
    
    // Создаём состояние
    GET_DATA = malloc(sizeof(struct jsonfs_data));
    if (!GET_DATA) {
        perror("malloc");
        json_object_put(root);
        return 1;
    }
    
    GET_DATA->json_root = root;
    GET_DATA->json_filename = strdup(json_filename);
    GET_DATA->is_modified = 0;
    GET_DATA->ft_list = NULL;
    GET_DATA->uid = getuid();
    GET_DATA->gid = getgid();
    
    // Добавляем корневую временную метку
    GET_DATA->ft_list = add_file_time("/", NULL);
    
    // Формируем аргументы для FUSE (пропускаем JSON-файл, оставляем точку монтирования и опции)
    // Аргументы: программа, точка_монтирования, опции...
    int fuse_argc = argc - 1;  // убираем JSON-файл (argv[1])
    char **fuse_argv = malloc(fuse_argc * sizeof(char*));
    if (!fuse_argv) {
        fprintf(stderr, "Ошибка: не удалось выделить память\n");
        json_object_put(root);
        free(GET_DATA->json_filename);
        free(GET_DATA);
        return 1;
    }
    
    fuse_argv[0] = argv[0];  // имя программы
    fuse_argv[1] = argv[2];  // точка монтирования
    for (int i = 3; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];  // остальные опции
    }
    
    printf("Запуск FUSE...\n");
    
    // Запускаем FUSE
    int ret = fuse_main(fuse_argc, fuse_argv, &operations, GET_DATA);
    
    free(fuse_argv);
    return ret;
}