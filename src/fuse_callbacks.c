#define FUSE_USE_VERSION 35

#include <jansson.h>
#include <fuse.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/common.h"
#include "../include/handlers.h"
#include "../include/file_time.h"
#include "../include/json_operations.h"


int jsonfs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    int res_getattr;
    (void) fi;

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    memset(st, 0, sizeof(struct stat));

    if (is_special_file(path)) {
        res_getattr = getattr_special_file(path, st, pd);
    }
    else {
        res_getattr = getattr_json_file(path, st, pd);
    }

    return res_getattr;
}

int jsonfs_mknod(const char *path, mode_t mode, dev_t dev) {
    int res_mk;

    if (strstr(path, ".sw")) { return -EPERM; }

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);
    
    res_mk = make_file(path, mode, pd);
    if (!res_mk) { pd->is_saved = 0; }

    return res_mk;
}

int jsonfs_mkdir(const char *path, mode_t mode) {
    int res_mk;

    if (strstr(path, ".sw")) { return -EPERM; }

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);
    
    res_mk = make_file(path, mode, pd);
    if (!res_mk) { pd->is_saved = 0; }

    return res_mk;
}

int jsonfs_unlink(const char *path) {
    int res_rm;

    if (strstr(path, ".sw")) { return -EPERM; }

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    res_rm = rm_file(path, S_IFREG, pd);
    if (!res_rm) { pd->is_saved = 0; }
    
    return res_rm;
}

int jsonfs_rmdir(const char *path) {
    int res_rm;

    if (strstr(path, ".sw")) { return -EPERM; }

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    res_rm = rm_file(path, S_IFDIR, pd);
    if (!res_rm) { pd->is_saved = 0; }
    
    return res_rm;
}

int jsonfs_rename(const char *old_path, const char *new_path, unsigned int flags) {
    int res_rename;
    (void) flags;

    if (strstr(old_path, ".sw") || strstr(new_path, ".sw")) { return -EPERM; }

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    res_rename = rename_file(old_path, new_path, pd);
    if (!res_rename) { pd->is_saved = 0; }

    return res_rename;
}

int jsonfs_truncate(const char *path, off_t len, struct fuse_file_info *fi) {
    int res_trunc;
    (void) fi;

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    res_trunc = trunc_json_file(path, len, pd);

    return res_trunc;
}

int jsonfs_open(const char *path, struct fuse_file_info *fi) {
    if ((fi->flags & O_TRUNC) == O_TRUNC) {
        struct fuse_context *ctx = fuse_get_context();
        struct jsonfs_private_data *pd = ctx->private_data;
        CHECK_POINTER(pd, -ENOMEM);

        trunc_json_file(path, 0, pd);
    }

    return 0;
}

int jsonfs_read(const char *path, char *buffer, size_t size,
                off_t offset, struct fuse_file_info *fi) {
    int res_read;
    (void) fi;

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    if (is_special_file(path)) {
        res_read = read_special_file(path, buffer, size, offset, pd);
    }
    else {
        res_read = read_json_file(path, buffer, size, offset, pd);
    }

    return res_read;
}

int jsonfs_write(const char *path, const char *buffer, size_t size,
                 off_t offset, struct fuse_file_info *fi) {
    int res_write; 
    (void) fi;

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    if (is_special_file(path)) {
        res_write = write_special_file(path, buffer, size, offset, pd);
        if (res_write >= 0) { pd->is_saved = 1; }
    }
    else {
        res_write = write_json_file(path, buffer, size, offset, pd);
        if (res_write >= 0) { pd->is_saved = 0; }
    }

    return res_write;
}

int jsonfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi,
                   enum fuse_readdir_flags flags) {
    json_t *node = NULL;
    const char *key = NULL;
    json_t *value = NULL;

    (void) offset;
    (void) fi;
    (void) flags;

    struct fuse_context *ctx = fuse_get_context();
    struct jsonfs_private_data *pd = ctx->private_data;
    CHECK_POINTER(pd, -ENOMEM);

    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);
    if (strcmp("/", path) == 0) {
        filler(buffer, ".status", NULL, 0, 0);
        filler(buffer, ".save", NULL, 0, 0);
    }

    node = find_json_node(path, pd->root);
    CHECK_POINTER(node, -ENOENT);

    if (!json_is_object(node)) {
        return -ENOTDIR;
    }

    json_object_foreach(node, key, value) {
        filler(buffer, key, NULL, 0, 0);
    }

    return 0;
}

void jsonfs_destroy(void *userdata) {
    if (!userdata) { return; }

    struct jsonfs_private_data *pd = (struct jsonfs_private_data *)userdata;
    destroy_private_data(pd);
}

int jsonfs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    // Упрощённая версия
    (void) path;
    (void) tv;
    (void) fi;
    return 0;
}