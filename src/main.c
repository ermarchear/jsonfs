#define FUSE_USE_VERSION 35

#include <jansson.h>
#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/common.h"
#include "../include/handlers.h"
#include "../include/file_time.h"
#include "../include/json_operations.h"
#include "../include/jsonfs.h"

extern int jsonfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi);
extern int jsonfs_mknod(const char *path, mode_t mode, dev_t dev);
extern int jsonfs_mkdir(const char *path, mode_t mode);
extern int jsonfs_unlink(const char *path);
extern int jsonfs_rmdir(const char *path);
extern int jsonfs_rename(const char *old_path, const char *new_path, unsigned int flags);
extern int jsonfs_truncate(const char *path, off_t len, struct fuse_file_info *fi);
extern int jsonfs_open(const char *path, struct fuse_file_info *fi);
extern int jsonfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
extern int jsonfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
extern int jsonfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
extern void jsonfs_destroy(void *userdata);
extern int jsonfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);

struct fuse_operations get_fuse_op(void) {
    struct fuse_operations op = {
        .getattr = jsonfs_getattr,
        .mknod   = jsonfs_mknod,
        .mkdir   = jsonfs_mkdir,
        .unlink  = jsonfs_unlink,
        .rmdir   = jsonfs_rmdir,
        .rename  = jsonfs_rename,
        .truncate = jsonfs_truncate,
        .open    = jsonfs_open,
        .read    = jsonfs_read,
        .write   = jsonfs_write,
        .readdir = jsonfs_readdir,
        .destroy = jsonfs_destroy,
        .utimens = jsonfs_utimens
    };

    return op;
}

int get_fuse_args(int argc, char **argv, struct private_args *args) {
    CHECK_POINTER(argv, -1);
    CHECK_POINTER(args, -1);

    args->fuse_argc = argc - 1;
    args->fuse_argv = calloc(args->fuse_argc, sizeof(char *));
    if (!args->fuse_argv) {
        return -1;
    }

    args->fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        args->fuse_argv[i - 1] = argv[i];
    }

    return 0;
}

struct jsonfs_private_data *init_private_data(json_t *json_root, const char *path) {
    int count_byte;
    char cwd[MID_SIZE];
    char full_path[BIG_SIZE];
    time_t now = time(NULL);

    CHECK_POINTER(json_root, NULL);
    CHECK_POINTER(path, NULL);

    struct jsonfs_private_data *pd = calloc(1, sizeof(struct jsonfs_private_data));
    CHECK_POINTER(pd, NULL);

    pd->root = json_root;

    if (path[0] == '/') {
        count_byte = snprintf(full_path, sizeof(full_path), "%s", path);
        if (count_byte >= sizeof(full_path)) { goto handle_error; }
    }
    else {
        if (!getcwd(cwd, sizeof(cwd))) { goto handle_error; }

        count_byte = snprintf(full_path, sizeof(full_path), "%s/%s", cwd, path);
        if (count_byte >= sizeof(full_path)) { goto handle_error; }
    }

    pd->path_to_json_file = strdup(full_path);
    if (!pd->path_to_json_file) { goto handle_error; }

    pd->ft = add_node_to_list_ft("/", NULL, SET_ATIME | SET_MTIME | SET_CTIME);
    if (!pd->ft) { goto handle_error; }

    pd->mount_time = now;
    pd->uid = getuid();
    pd->gid = getgid();
    pd->is_saved = 1;

    return pd;
    
    handle_error:
        json_decref(pd->root);
        free(pd->path_to_json_file);
        free(pd);
        return NULL;
}

void destroy_private_data(struct jsonfs_private_data *pd) {
    struct file_time *next = NULL;
    struct file_time *curr = NULL;

    if (!pd) { return; }

    if (pd->root) {
        json_decref(pd->root);
    }

    free(pd->path_to_json_file);

    curr = pd->ft;
    while(curr) {
        next = curr->next_node;
        free_file_time(curr);
        curr = next;
    }

    free(pd);
}

int main(int argc, char **argv) {
    json_t *root = NULL;
    json_t *norm_root = NULL;
    struct jsonfs_private_data *pd = NULL;
    json_error_t json_error;
    struct private_args args;
    const char *json_file = NULL;
    int ret, res_get_args;

    if (argc < 3) { 
        fprintf(stderr, "Использование: %s <json-файл> <точка_монтирования> [опции]\n", argv[0]);
        fprintf(stderr, "Пример: %s data.json /mnt/json -f\n", argv[0]);
        return EXIT_FAILURE; 
    }

    memset(&json_error, 0, sizeof(json_error));
    memset(&args, 0, sizeof(args));

    json_file = argv[1];

    root = json_load_file(json_file, JSON_DECODE_ANY, &json_error);
    if (!root) { 
        fprintf(stderr, "Ошибка загрузки JSON: %s\n", json_error.text);
        goto handle_error; 
    }

    norm_root = normalize_json(root, 1);
    if (!norm_root) { goto handle_error; }
    json_decref(root);

    pd = init_private_data(norm_root, json_file);
    if (!pd) { goto handle_error; }

    struct fuse_operations op = get_fuse_op();

    res_get_args = get_fuse_args(argc, argv, &args);
    if (res_get_args == -1) { goto handle_error; }

    ret = fuse_main(args.fuse_argc, args.fuse_argv, &op, pd);
    free(args.fuse_argv);
    return ret;

    handle_error:
        if (pd) destroy_private_data(pd);
        if (root) json_decref(root);
        if (norm_root) json_decref(norm_root);
        fputs("jsonfs: не удалось инициализировать файловую систему\n", stderr);
        return EXIT_FAILURE;
}