#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

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
    fprintf(stderr, "Использование: %s <json_файл> <точка_монтирования> [опции]\n", progname);
    fprintf(stderr, "\nСпециальные файлы в точке монтирования:\n");
    fprintf(stderr, "  .save      - Записать что угодно для сохранения изменений\n");
    fprintf(stderr, "  .modified  - Проверить наличие изменений (1=да, 0=нет)\n");
    fprintf(stderr, "  .sync      - Синхронизация с диском\n");
    fprintf(stderr, "  .help      - Показать эту справку\n");
    fprintf(stderr, "\nОпределение типов:\n");
    fprintf(stderr, "  true/false -> логический тип\n");
    fprintf(stderr, "  123       -> целое число\n");
    fprintf(stderr, "  12.34     -> число с плавающей точкой\n");
    fprintf(stderr, "  null      -> нулевое значение\n");
    fprintf(stderr, "  текст     -> строка (по умолчанию)\n");
    fprintf(stderr, "\nПример:\n");
    fprintf(stderr, "  sudo %s data.json /mnt/jsonfs\n", progname);
    fprintf(stderr, "  echo save > /mnt/jsonfs/.save\n");
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
    
    state->json_path = realpath(argv[1], NULL);
    if (!state->json_path) {
        perror("realpath");
        free(state);
        return 1;
    }
    
    state->root = NULL;
    state->modified = 0;
    state->last_save_time = time(NULL);
    
    printf("JSONFS v2.0 запуск\n");
    printf("JSON файл: %s\n", state->json_path);
    printf("Точка монтирования: %s\n", argv[2]);
    printf("Определение типов: ВКЛ\n");
    printf("Изменения НЕ будут сохраняться автоматически. Используйте файл .save для сохранения.\n");
    printf("Swap файлы игнорируются.\n");
    
    char *fuse_argv[30];
    int fuse_argc = 0;
    
    fuse_argv[fuse_argc++] = argv[0];
    fuse_argv[fuse_argc++] = argv[2];
    fuse_argv[fuse_argc++] = "-s";  // Однопоточный режим
    
    // Добавляем foreground режим для интерактивной работы
    int foreground = 1;
for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
        foreground = 0;
    }
    fuse_argv[fuse_argc++] = argv[i];
}

if (foreground) {
    fuse_argv[fuse_argc++] = "-f";
}
    
    int ret = fuse_main(fuse_argc, fuse_argv, &jsonfs_ops, state);
    
    if (ret != 0) {
        fprintf(stderr, "FUSE завершился с ошибкой: %d\n", ret);
        if (state) {
            if (state->json_path) free(state->json_path);
            if (state->root) json_decref(state->root);
            free(state);
        }
    }
    
    return ret;
}