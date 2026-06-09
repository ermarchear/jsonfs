#ifndef JSONFS_H
#define JSONFS_H

#define FUSE_USE_VERSION 35

#include <fuse.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_PATH 4096
#define MAX_DATA 8192

typedef struct {
    char *json_path;
    json_t *root;
    int modified;
    time_t last_save_time;
} jsonfs_state;

// JSON operations
json_t* jsonfs_get_node(json_t *root, const char *path);
int jsonfs_set_value(json_t *root, const char *path, json_t *value);
int jsonfs_delete_path(json_t *root, const char *path);

// Save operations
int jsonfs_force_save(jsonfs_state *state);
int jsonfs_is_modified(jsonfs_state *state);

// FUSE callbacks
void* jsonfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
void jsonfs_destroy(void *private_data);
int jsonfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int jsonfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                   off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int jsonfs_open(const char *path, struct fuse_file_info *fi);
int jsonfs_read(const char *path, char *buf, size_t size, off_t offset, 
                struct fuse_file_info *fi);
int jsonfs_write(const char *path, const char *buf, size_t size, 
                 off_t offset, struct fuse_file_info *fi);
int jsonfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int jsonfs_unlink(const char *path);
int jsonfs_mkdir(const char *path, mode_t mode);
int jsonfs_rmdir(const char *path);
int jsonfs_rename(const char *from, const char *to, unsigned int flags);
int jsonfs_truncate(const char *path, off_t size, struct fuse_file_info *fi);
int jsonfs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
int jsonfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

#endif