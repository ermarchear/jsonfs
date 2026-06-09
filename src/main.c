#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>  // Добавьте эту строку

static struct fuse_operations jsonfs_ops = {
    .init       = jsonfs_init,
    .destroy    = jsonfs_destroy,
    .getattr    = jsonfs_getattr,
    .readdir    = jsonfs_readdir,
    .open       = jsonfs_open,
    .read       = jsonfs_read,
    .write      = jsonfs_write,
    .create     = jsonfs_create,
    .unlink     = jsonfs_unlink,
    .mkdir      = jsonfs_mkdir,
    .rmdir      = jsonfs_rmdir,
    .rename     = jsonfs_rename,
    .truncate   = jsonfs_truncate,
    .utimens    = jsonfs_utimens,
    .fsync      = jsonfs_fsync,
};

void print_usage(const char *progname) {
    fprintf(stderr, "JSONFS - JSON Файловая система v2.0\n");
    fprintf(stderr, "Использование: %s <json_файл> <точка_монтирования>\n", progname);
    fprintf(stderr, "\nСпециальные файлы:\n");
    fprintf(stderr, "  .save      - Сохранить изменения\n");
    fprintf(stderr, "  .modified  - Статус изменений\n");
    fprintf(stderr, "  .help      - Справка\n");
    fprintf(stderr, "\nПример:\n");
    fprintf(stderr, "  sudo %s data.json mnt\n", progname);
    fprintf(stderr, "  echo save | sudo tee mnt/.save\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    jsonfs_state *state = malloc(sizeof(jsonfs_state));
    if (!state) {
        perror("malloc");
        return 1;
    }
    
    memset(state, 0, sizeof(jsonfs_state));
    
    // Проверяем существование файла
    struct stat st;
    if (stat(argv[1], &st) != 0) {
        // Файл не существует, используем путь как есть
        state->json_path = strdup(argv[1]);
        if (!state->json_path) {
            perror("strdup");
            free(state);
            return 1;
        }
    } else {
        state->json_path = realpath(argv[1], NULL);
        if (!state->json_path) {
            perror("realpath");
            free(state);
            return 1;
        }
    }
    
    state->root = NULL;
    state->modified = 0;
    state->last_save_time = time(NULL);
    
    printf("JSONFS v2.0 запущен\n");
    printf("JSON файл: %s\n", state->json_path);
    printf("Точка монтирования: %s\n", argv[2]);
    printf("Используйте 'fusermount -u %s' для размонтирования\n", argv[2]);
    
    char *fuse_argv[20];
    int fuse_argc = 0;
    
    fuse_argv[fuse_argc++] = argv[0];
    fuse_argv[fuse_argc++] = argv[2];
    fuse_argv[fuse_argc++] = "-s";  // Однопоточный режим
    
    int ret = fuse_main(fuse_argc, fuse_argv, &jsonfs_ops, state);
    
    if (ret != 0) {
        fprintf(stderr, "FUSE ошибка: %d\n", ret);
    }
    
    // Очистка (хотя fuse_main уже должен был вызвать destroy)
    if (state) {
        if (state->json_path) free(state->json_path);
        if (state->root) json_decref(state->root);
        free(state);
    }
    
    return ret;
}