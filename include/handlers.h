#ifndef HANDLERS_H
#define HANDLERS_H

#include <sys/stat.h>
#include <jansson.h>
#include "file_time.h"

struct jsonfs_private_data {
    json_t *root;
    char *path_to_json_file;
    struct file_time *ft;
    time_t mount_time;
    uid_t uid;
    gid_t gid;
    int is_saved;
};

// Объявления функций
int getattr_json_file(const char *path, struct stat *st, struct jsonfs_private_data *pd);
int getattr_special_file(const char *path, struct stat *st, struct jsonfs_private_data *pd);
int make_file(const char *path, mode_t mode, struct jsonfs_private_data *pd);
int rm_file(const char *path, int file_type, struct jsonfs_private_data *pd);
int rename_file(const char *old_path, const char *new_path, struct jsonfs_private_data *pd);
int trunc_json_file(const char *path, off_t offset, struct jsonfs_private_data *pd);
int read_json_file(const char *path, char *buffer, size_t size, off_t offset, struct jsonfs_private_data *pd);
int read_special_file(const char *path, char *buffer, size_t size, off_t offset, struct jsonfs_private_data *pd);
int write_json_file(const char *path, const char *buffer, size_t size, off_t offset, struct jsonfs_private_data *pd);
int write_special_file(const char *path, const char *buffer, size_t size, off_t offset, struct jsonfs_private_data *pd);
void destroy_private_data(struct jsonfs_private_data *pd);

#endif