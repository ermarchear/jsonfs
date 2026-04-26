#ifndef JSONFS_H
#define JSONFS_H

#include <fuse.h>
#include <json-c/json.h>
#include <sys/stat.h>
#include <time.h>


// Временные метки

struct file_time {
    char *path;
    time_t atime; // последний доступ
    time_t mtime; // последнее изменение
    time_t ctime; // изменение метаданных
    struct file_time *next;
};

// Данные ФС
struct jsonfs_data {
    struct json_object *json_root;
    char *json_filename;
    int is_modified;
    struct file_time *ft_list; // список временных меток
    uid_t uid;  
    gid_t gid;
};
// Функции для работы с JSON
struct json_object* find_by_path(const char *path, struct json_object *root);
char* number_to_string(double n);
int split_path(const char *path, char **parent_path, char **name);
int ensure_parent_dirs(const char *path, struct jsonfs_data *data);

// Функции для временных меток
struct file_time* find_file_time(const char *path, struct file_time *root);
struct file_time* add_file_time(const char *path, struct file_time *root);
void update_atime(const char *path, struct jsonfs_data *data);
void update_mtime(const char *path, struct jsonfs_data *data);
void update_ctime(const char *path, struct jsonfs_data *data);
void free_file_time_list(struct file_time *root);

// Функции сохранения
int save_json(struct jsonfs_data *data);
int auto_save(void *priv);

// Операции FUSE
void* fs_init(struct fuse_conn_info *conn);
void fs_destroy(void *priv);
int fs_getattr(const char *path, struct stat *st);
int fs_opendir(const char *path, struct fuse_file_info *fi);
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi);
int fs_open(const char *path, struct fuse_file_info *fi);
int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi);
int fs_write(const char *path, const char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi);
int fs_mknod(const char *path, mode_t mode, dev_t dev);
int fs_mkdir(const char *path, mode_t mode);
int fs_unlink(const char *path);
int fs_rmdir(const char *path);
int fs_rename(const char *old_path, const char *new_path);
int fs_utimens(const char *path, const struct timespec ts[2]);

#endif