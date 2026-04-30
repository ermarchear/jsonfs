#ifndef JSONFS_H
#define JSONFS_H

#include <fuse.h>
#include <sys/stat.h>
#include <jansson.h>
#include <time.h>

#include "common.h"
#include "handlers.h"
#include "file_time.h"
#include "json_operations.h"

// Структура для аргументов FUSE
struct private_args {
    int fuse_argc;
    char **fuse_argv;
};

struct fuse_operations get_fuse_op(void);
int get_fuse_args(int argc, char **argv, struct private_args *args);
struct jsonfs_private_data *init_private_data(json_t *json_root, const char *path);
void destroy_private_data(struct jsonfs_private_data *pd);

#endif