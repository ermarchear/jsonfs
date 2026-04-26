#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h> 
#include "../include/jsonfs.h"

extern struct jsonfs_data* GET_DATA;

// Инициализация
void* fs_init(struct fuse_conn_info *conn) {
    return GET_DATA;
}

// Уничтожение
void fs_destroy(void *priv) {
    struct jsonfs_data *data = (struct jsonfs_data*)priv;
    if (data->is_modified) {
        save_json(data);
    }
    if (data->json_root) json_object_put(data->json_root);
    free(data->json_filename);
    free_file_time_list(data->ft_list);
    free(data);
}

// Получение атрибутов
int fs_getattr(const char *path, struct stat *st) {
    memset(st, 0, sizeof(struct stat));
    
    struct jsonfs_data *data = GET_DATA;
    
    // Служебные файлы
    if (strcmp(path, "/.save") == 0) {
        st->st_mode = S_IFREG | 0222;
        st->st_nlink = 1;
        st->st_size = 1;
        st->st_uid = data->uid;
        st->st_gid = data->gid;
        return 0;
    }
    if (strcmp(path, "/.status") == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 10;
        st->st_uid = data->uid;
        st->st_gid = data->gid;
        return 0;
    }
    
    struct json_object *node = find_by_path(path, data->json_root);
    if (!node) return -ENOENT;
    
    enum json_type type = json_object_get_type(node);
    
    st->st_uid = data->uid;
    st->st_gid = data->gid;
    
    // Временные метки
    struct file_time *ft = find_file_time(path, data->ft_list);
    if (ft) {
        st->st_atime = ft->atime;
        st->st_mtime = ft->mtime;
        st->st_ctime = ft->ctime;
    } else {
        time_t now = time(NULL);
        st->st_atime = now;
        st->st_mtime = now;
        st->st_ctime = now;
    }
    
    if (type == json_type_object || type == json_type_array) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
    } else {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        
        if (type == json_type_string) {
            st->st_size = strlen(json_object_get_string(node));
        }
        else if (type == json_type_int) {
            char *tmp = number_to_string(json_object_get_int(node));
            st->st_size = strlen(tmp);
            free(tmp);
        }
        else if (type == json_type_double) {
            char *tmp = number_to_string(json_object_get_double(node));
            st->st_size = strlen(tmp);
            free(tmp);
        }
        else if (type == json_type_boolean) {
            st->st_size = json_object_get_boolean(node) ? 4 : 5;
        }
        else if (type == json_type_null) {
            st->st_size = 4;
        }
    }
    return 0;
}

// Открытие директории
int fs_opendir(const char *path, struct fuse_file_info *fi) {
    struct jsonfs_data *data = GET_DATA;
    struct json_object *node = find_by_path(path, data->json_root);
    if (!node) return -ENOENT;
    
    fi->fh = (uintptr_t)node;
    return 0;
}

// Чтение директории
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t off, struct fuse_file_info *fi) {
    (void) off;
    
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    // Добавляем служебные файлы в корень
    if (strcmp(path, "/") == 0) {
        filler(buf, ".save", NULL, 0);
        filler(buf, ".status", NULL, 0);
    }
    
    struct json_object *node = (struct json_object*)(uintptr_t)fi->fh;
    enum json_type type = json_object_get_type(node);
    
    if (type == json_type_object) {
        json_object_object_foreach(node, key, val) {
            (void) val;
            filler(buf, key, NULL, 0);
        }
    }
    else if (type == json_type_array) {
        int len = json_object_array_length(node);
        for (int i = 0; i < len; i++) {
            char idx[16];
            snprintf(idx, sizeof(idx), "%d", i);
            filler(buf, idx, NULL, 0);
        }
    }
    else {
        return -ENOTDIR;
    }
    return 0;
}

// Открытие файла
int fs_open(const char *path, struct fuse_file_info *fi) {
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/.save") == 0) {
        if ((fi->flags & 3) == O_RDONLY) return -EACCES;
        fi->fh = 0;
        return 0;
    }
    
    struct json_object *node = find_by_path(path, data->json_root);
    if (!node) return -ENOENT;
    
    enum json_type type = json_object_get_type(node);
    if (type == json_type_object || type == json_type_array) {
        return -EISDIR;
    }
    
    fi->fh = (uintptr_t)node;
    update_atime(path, data);
    return 0;
}

// Чтение файла
int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/.status") == 0) {
        const char *status = data->is_modified ? "UNSAVED\n" : "SAVED\n";
        size_t len = strlen(status);
        
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, status + offset, size);
            return size;
        }
        return 0;
    }
    
    struct json_object *node = (struct json_object*)(uintptr_t)fi->fh;
    enum json_type type = json_object_get_type(node);
    
    char *content = NULL;
    int need_free = 0;
    size_t len = 0;
    
    if (type == json_type_string) {
        content = (char*)json_object_get_string(node);
        len = strlen(content);
    }
    else if (type == json_type_int) {
        content = number_to_string(json_object_get_int(node));
        len = strlen(content);
        need_free = 1;
    }
    else if (type == json_type_double) {
        content = number_to_string(json_object_get_double(node));
        len = strlen(content);
        need_free = 1;
    }
    else if (type == json_type_boolean) {
        content = json_object_get_boolean(node) ? "true" : "false";
        len = strlen(content);
    }
    else if (type == json_type_null) {
        content = "null";
        len = 4;
    }
    else {
        return -EINVAL;
    }
    
    if (offset < len) {
        if (offset + size > len) size = len - offset;
        memcpy(buf, content + offset, size);
    } else {
        size = 0;
    }
    
    if (need_free) free(content);
    return size;
}

// Запись в файл
int fs_write(const char *path, const char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/.save") == 0) {
        if (save_json(data) == 0) return size;
        return -EIO;
    }
    
    struct json_object *node = (struct json_object*)(uintptr_t)fi->fh;
    enum json_type type = json_object_get_type(node);
    
    char *old_str = NULL;
    int need_free_old = 0;
    
    if (type == json_type_string) {
        old_str = (char*)json_object_get_string(node);
    } else if (type == json_type_int) {
        old_str = number_to_string(json_object_get_int(node));
        need_free_old = 1;
    } else if (type == json_type_double) {
        old_str = number_to_string(json_object_get_double(node));
        need_free_old = 1;
    } else if (type == json_type_boolean) {
        old_str = (char*)(json_object_get_boolean(node) ? "true" : "false");
    } else if (type == json_type_null) {
        old_str = "null";
    } else {
        return -EINVAL;
    }
    
    char *old_copy = strdup(old_str);
    if (need_free_old) free(old_str);
    if (!old_copy) return -ENOMEM;
    
    size_t old_len = strlen(old_copy);
    size_t new_len = (offset + size > old_len) ? offset + size : old_len;
    
    char *new_str = calloc(1, new_len + 1);
    if (!new_str) {
        free(old_copy);
        return -ENOMEM;
    }
    memcpy(new_str, old_copy, old_len);
    memcpy(new_str + offset, buf, size);
    new_str[new_len] = '\0';
    
    struct json_object *new_node = NULL;
    char *endptr;
    double num = strtod(new_str, &endptr);
    
    if (*endptr == '\0' && strlen(new_str) > 0) {
        if ((int)num == num && num == (int)num) {
            new_node = json_object_new_int((int)num);
        } else {
            new_node = json_object_new_double(num);
        }
    }
    else if (strcmp(new_str, "true") == 0) {
        new_node = json_object_new_boolean(1);
    }
    else if (strcmp(new_str, "false") == 0) {
        new_node = json_object_new_boolean(0);
    }
    else if (strcmp(new_str, "null") == 0) {
        new_node = json_object_new_null();
    }
    else {
        new_node = json_object_new_string(new_str);
    }
    
    if (!new_node) {
        free(old_copy);
        free(new_str);
        return -ENOMEM;
    }
    
    char *parent_path = NULL, *name = NULL;
    if (split_path(path, &parent_path, &name) == 0) {
        struct json_object *parent = find_by_path(parent_path, data->json_root);
        if (parent && json_object_get_type(parent) == json_type_object) {
            json_object_object_del(parent, name);
            json_object_object_add(parent, name, new_node);
        } else {
            json_object_put(new_node);
        }
        free(parent_path);
        free(name);
    } else {
        json_object_put(new_node);
    }
    
    free(old_copy);
    free(new_str);
    
    data->is_modified = 1;
    update_mtime(path, data);
    
    return size;
}

// Создание файла
int fs_mknod(const char *path, mode_t mode, dev_t dev) {
    (void) mode;
    (void) dev;
    
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/.save") == 0 || strcmp(path, "/.status") == 0) {
        return -EPERM;
    }
    
    int ret = ensure_parent_dirs(path, data);
    if (ret < 0) return ret;
    
    char *parent_path = NULL, *name = NULL;
    if (split_path(path, &parent_path, &name) < 0) {
        return -EINVAL;
    }
    
    struct json_object *parent = find_by_path(parent_path, data->json_root);
    if (!parent) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    if (json_object_get_type(parent) != json_type_object) {
        free(parent_path);
        free(name);
        return -ENOTDIR;
    }
    
    if (json_object_object_get(parent, name)) {
        free(parent_path);
        free(name);
        return -EEXIST;
    }
    
    struct json_object *new_node = json_object_new_int(0);
    json_object_object_add(parent, name, new_node);
    
    free(parent_path);
    free(name);
    
    data->is_modified = 1;
    update_ctime(path, data);
    
    return 0;
}

// Создание директории
int fs_mkdir(const char *path, mode_t mode) {
    (void) mode;
    
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/") == 0) return -EEXIST;
    
    int ret = ensure_parent_dirs(path, data);
    if (ret < 0) return ret;
    
    char *parent_path = NULL, *name = NULL;
    if (split_path(path, &parent_path, &name) < 0) {
        return -EINVAL;
    }
    
    struct json_object *parent = find_by_path(parent_path, data->json_root);
    if (!parent) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    if (json_object_get_type(parent) != json_type_object) {
        free(parent_path);
        free(name);
        return -ENOTDIR;
    }
    
    if (json_object_object_get(parent, name)) {
        free(parent_path);
        free(name);
        return -EEXIST;
    }
    
    struct json_object *new_dir = json_object_new_object();
    json_object_object_add(parent, name, new_dir);
    
    free(parent_path);
    free(name);
    
    data->is_modified = 1;
    update_ctime(path, data);
    
    return 0;
}

// Удаление файла
int fs_unlink(const char *path) {
    struct jsonfs_data *data = GET_DATA;
    
    char *parent_path = NULL, *name = NULL;
    if (split_path(path, &parent_path, &name) < 0) {
        return -EINVAL;
    }
    
    struct json_object *parent = find_by_path(parent_path, data->json_root);
    if (!parent) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    if (json_object_get_type(parent) != json_type_object) {
        free(parent_path);
        free(name);
        return -ENOTDIR;
    }
    
    struct json_object *node = json_object_object_get(parent, name);
    if (!node) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    if (json_object_get_type(node) == json_type_object) {
        free(parent_path);
        free(name);
        return -EISDIR;
    }
    
    json_object_object_del(parent, name);
    
    free(parent_path);
    free(name);
    
    data->is_modified = 1;
    
    return 0;
}

// Удаление директории
int fs_rmdir(const char *path) {
    struct jsonfs_data *data = GET_DATA;
    
    if (strcmp(path, "/") == 0) return -EBUSY;
    
    char *parent_path = NULL, *name = NULL;
    if (split_path(path, &parent_path, &name) < 0) {
        return -EINVAL;
    }
    
    struct json_object *parent = find_by_path(parent_path, data->json_root);
    if (!parent) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    struct json_object *dir = json_object_object_get(parent, name);
    if (!dir) {
        free(parent_path);
        free(name);
        return -ENOENT;
    }
    
    if (json_object_get_type(dir) != json_type_object) {
        free(parent_path);
        free(name);
        return -ENOTDIR;
    }
    
    if (json_object_object_length(dir) > 0) {
        free(parent_path);
        free(name);
        return -ENOTEMPTY;
    }
    
    json_object_object_del(parent, name);
    
    free(parent_path);
    free(name);
    
    data->is_modified = 1;
    
    return 0;
}

// Переименование
int fs_rename(const char *old_path, const char *new_path) {
    struct jsonfs_data *data = GET_DATA;
    
    struct json_object *old_node = find_by_path(old_path, data->json_root);
    if (!old_node) return -ENOENT;
    
    char *old_parent_path = NULL, *old_name = NULL;
    char *new_parent_path = NULL, *new_name = NULL;
    
    if (split_path(old_path, &old_parent_path, &old_name) < 0) {
        return -EINVAL;
    }
    
    if (split_path(new_path, &new_parent_path, &new_name) < 0) {
        free(old_parent_path);
        free(old_name);
        return -EINVAL;
    }
    
    struct json_object *old_parent = find_by_path(old_parent_path, data->json_root);
    struct json_object *new_parent = find_by_path(new_parent_path, data->json_root);
    
    if (!old_parent || !new_parent) {
        free(old_parent_path);
        free(old_name);
        free(new_parent_path);
        free(new_name);
        return -ENOENT;
    }
    
    json_object_get(old_node);
    json_object_object_del(old_parent, old_name);
    json_object_object_add(new_parent, new_name, old_node);
    
    free(old_parent_path);
    free(old_name);
    free(new_parent_path);
    free(new_name);
    
    data->is_modified = 1;
    
    return 0;
}

// Изменение временных меток
int fs_utimens(const char *path, const struct timespec ts[2]) {
    struct jsonfs_data *data = GET_DATA;
    
    struct file_time *ft = find_file_time(path, data->ft_list);
    if (ft) {
        ft->atime = ts[0].tv_sec;
        ft->mtime = ts[1].tv_sec;
        ft->ctime = ts[1].tv_sec;
    }
    
    return 0;
}