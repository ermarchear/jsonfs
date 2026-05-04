#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <time.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

static int jsonfs_save(jsonfs_state *state) {
    if (!state || !state->json_path || !state->root) {
        printf("JSONFS: Save failed - invalid state\n");
        return -EINVAL;
    }
    
    printf("JSONFS: Attempting to save to %s\n", state->json_path);
    
    FILE *f = fopen(state->json_path, "w");
    if (!f) {
        printf("JSONFS: Cannot open file %s for writing: %s\n", 
               state->json_path, strerror(errno));
        return -EIO;
    }
    
    int dump_result = json_dumpf(state->root, f, JSON_INDENT(2));
    if (dump_result != 0) {
        printf("JSONFS: json_dumpf failed with code %d\n", dump_result);
        fclose(f);
        return -EIO;
    }
    
    fclose(f);
    printf("JSONFS: Successfully saved to %s\n", state->json_path);
    return 0;
}

int jsonfs_force_save(jsonfs_state *state) {
    if (!state) {
        return -EINVAL;
    }
    
    pthread_mutex_lock(&state->mutex);
    int result = jsonfs_save(state);
    if (result == 0) {
        state->modified = 0;
    }
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_is_modified(jsonfs_state *state) {
    if (!state) return 0;
    pthread_mutex_lock(&state->mutex);
    int modified = state->modified;
    pthread_mutex_unlock(&state->mutex);
    return modified;
}

void* jsonfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    cfg->kernel_cache = 0;
    cfg->use_ino = 1;
    
    pthread_mutex_init(&state->mutex, NULL);
    
    FILE *f = fopen(state->json_path, "r");
    if (f) {
        json_error_t error;
        state->root = json_loadf(f, 0, &error);
        fclose(f);
        
        if (!state->root) {
            fprintf(stderr, "JSON parse error: %s\n", error.text);
            state->root = json_object();
        }
    } else {
        state->root = json_object();
    }
    
    return state;
}

void jsonfs_destroy(void *private_data) {
    jsonfs_state *state = (jsonfs_state*)private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    if (state->modified) {
        fprintf(stderr, "\nWARNING: There are unsaved changes in %s\n", state->json_path);
        fprintf(stderr, "To save them, write to .save file before unmounting\n");
    }
    
    json_decref(state->root);
    pthread_mutex_unlock(&state->mutex);
    pthread_mutex_destroy(&state->mutex);
}

int jsonfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        pthread_mutex_unlock(&state->mutex);
        return 0;
    }
    
    if (strcmp(path, "/.save") == 0 || strcmp(path, "/.modified") == 0 || strcmp(path, "/.help") == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_size = 4096;
        stbuf->st_nlink = 1;
        pthread_mutex_unlock(&state->mutex);
        return 0;
    }
    
    json_t *node = jsonfs_get_node(state->root, path);
    
    if (!node) {
        pthread_mutex_unlock(&state->mutex);
        return -ENOENT;
    }
    
    if (json_is_object(node)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (json_is_string(node)) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_size = strlen(json_string_value(node));
        stbuf->st_nlink = 1;
    } else if (json_is_integer(node)) {
        stbuf->st_mode = S_IFREG | 0644;
        char buf[32];
        snprintf(buf, sizeof(buf), "%" JSON_INTEGER_FORMAT, json_integer_value(node));
        stbuf->st_size = strlen(buf);
        stbuf->st_nlink = 1;
    } else if (json_is_real(node)) {
        stbuf->st_mode = S_IFREG | 0644;
        char buf[64];
        snprintf(buf, sizeof(buf), "%f", json_real_value(node));
        stbuf->st_size = strlen(buf);
        stbuf->st_nlink = 1;
    } else if (json_is_boolean(node)) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_size = json_is_true(node) ? 4 : 5;
        stbuf->st_nlink = 1;
    } else if (json_is_null(node)) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_size = 4;
        stbuf->st_nlink = 1;
    } else if (json_is_array(node)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        pthread_mutex_unlock(&state->mutex);
        return -ENOENT;
    }
    
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = time(NULL);
    stbuf->st_mtime = time(NULL);
    stbuf->st_ctime = time(NULL);
    
    pthread_mutex_unlock(&state->mutex);
    return 0;
}

int jsonfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                   off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    if (strcmp(path, "/") == 0) {
        filler(buf, ".save", NULL, 0, 0);
        filler(buf, ".modified", NULL, 0, 0);
        filler(buf, ".help", NULL, 0, 0);
    }
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *dir = jsonfs_get_node(state->root, path);
    
    if (!dir || (!json_is_object(dir) && !json_is_array(dir))) {
        pthread_mutex_unlock(&state->mutex);
        return 0;
    }
    
    if (json_is_object(dir)) {
        const char *key;
        json_t *value;
        json_object_foreach(dir, key, value) {
            filler(buf, key, NULL, 0, 0);
        }
    } else if (json_is_array(dir)) {
        size_t i;
        char idx_str[32];
        for (i = 0; i < json_array_size(dir); i++) {
            snprintf(idx_str, sizeof(idx_str), "%zu", i);
            filler(buf, idx_str, NULL, 0, 0);
        }
    }
    
    pthread_mutex_unlock(&state->mutex);
    return 0;
}

int jsonfs_read(const char *path, char *buf, size_t size, off_t offset, 
                struct fuse_file_info *fi) {
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    if (strcmp(path, "/.help") == 0) {
        const char *help = 
            "JSONFS - JSON Filesystem\n"
            "========================\n"
            "\n"
            "Special files:\n"
            "  .save      - Write anything to this file to save changes\n"
            "  .modified  - Read this file to check if changes exist (1=yes, 0=no)\n"
            "  .help      - This help message\n"
            "\n"
            "To save changes:\n"
            "  $ echo save > /mnt/jsonfs/.save\n"
            "  $ cat /mnt/jsonfs/.modified  (check if saving needed)\n"
            "\n"
            "Note: Changes are NOT saved automatically!\n"
            "You must explicitly save them using .save file.\n";
        
        size_t len = strlen(help);
        if (offset < 0 || (size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, help + offset, size);
        return size;
    }
    
    if (strcmp(path, "/.modified") == 0) {
        char modified_str[64];
        int modified = jsonfs_is_modified(state);
        snprintf(modified_str, sizeof(modified_str), "%d\n", modified);
        
        size_t len = strlen(modified_str);
        if (offset < 0 || (size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, modified_str + offset, size);
        return size;
    }
    
    if (strcmp(path, "/.save") == 0) {
        const char *msg = "Write anything here to save changes\n";
        size_t len = strlen(msg);
        if (offset < 0 || (size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, msg + offset, size);
        return size;
    }
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *node = jsonfs_get_node(state->root, path);
    
    if (!node) {
        pthread_mutex_unlock(&state->mutex);
        return -ENOENT;
    }
    
    char str_value[MAX_DATA];
    
    if (json_is_string(node)) {
        snprintf(str_value, sizeof(str_value), "%s", json_string_value(node));
    } else if (json_is_integer(node)) {
        snprintf(str_value, sizeof(str_value), "%" JSON_INTEGER_FORMAT, json_integer_value(node));
    } else if (json_is_real(node)) {
        snprintf(str_value, sizeof(str_value), "%f", json_real_value(node));
    } else if (json_is_boolean(node)) {
        snprintf(str_value, sizeof(str_value), "%s", json_is_true(node) ? "true" : "false");
    } else if (json_is_null(node)) {
        snprintf(str_value, sizeof(str_value), "null");
    } else {
        pthread_mutex_unlock(&state->mutex);
        return -EISDIR;
    }
    
    size_t len = strlen(str_value);
    
    if ((off_t)offset >= (off_t)len) {
        pthread_mutex_unlock(&state->mutex);
        return 0;
    }
    
    if ((size_t)(offset + size) > len) {
        size = len - offset;
    }
    
    memcpy(buf, str_value + offset, size);
    
    pthread_mutex_unlock(&state->mutex);
    return size;
}

int jsonfs_write(const char *path, const char *buf, size_t size, 
                 off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    (void)offset;  // Для простоты игнорируем offset
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    // Специальная обработка для файла .save
    if (strcmp(path, "/.save") == 0) {
        int result = jsonfs_force_save(state);
        if (result == 0) {
            return (int)size;
        } else {
            return result;
        }
    }
    
    pthread_mutex_lock(&state->mutex);
    
    // Создаём строку из входных данных (убираем trailing newline)
    char full_data[MAX_DATA];
    size_t copy_size = size;
    if (copy_size >= MAX_DATA) {
        copy_size = MAX_DATA - 1;
    }
    memcpy(full_data, buf, copy_size);
    full_data[copy_size] = '\0';
    
    // Удаляем символ новой строки в конце
    size_t len = strlen(full_data);
    if (len > 0 && full_data[len-1] == '\n') {
        full_data[len-1] = '\0';
    }
    
    // Создаём новое строковое значение
    json_t *new_value = json_string(full_data);
    if (!new_value) {
        pthread_mutex_unlock(&state->mutex);
        return -ENOMEM;
    }
    
    // Устанавливаем значение (перезаписывает существующее)
    int result = jsonfs_set_value(state->root, path, new_value);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Written to %s = '%s'\n", path, full_data);
    } else {
        json_decref(new_value);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return (result == 0) ? (int)size : result;
}

int jsonfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)mode;
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *existing = jsonfs_get_node(state->root, path);
    if (existing) {
        pthread_mutex_unlock(&state->mutex);
        return -EEXIST;
    }
    
    json_t *value = json_string("");
    int result = jsonfs_set_value(state->root, path, value);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Created %s\n", path);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_unlink(const char *path) {
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    int result = jsonfs_delete_path(state->root, path);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Deleted %s\n", path);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_mkdir(const char *path, mode_t mode) {
    (void)mode;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *existing = jsonfs_get_node(state->root, path);
    if (existing) {
        pthread_mutex_unlock(&state->mutex);
        return -EEXIST;
    }
    
    json_t *dir = json_object();
    int result = jsonfs_set_value(state->root, path, dir);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Created directory %s\n", path);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_rmdir(const char *path) {
    return jsonfs_unlink(path);
}

int jsonfs_rename(const char *from, const char *to, unsigned int flags) {
    (void)flags;
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *node = jsonfs_get_node(state->root, from);
    if (!node) {
        pthread_mutex_unlock(&state->mutex);
        return -ENOENT;
    }
    
    json_incref(node);
    
    if (jsonfs_delete_path(state->root, from) != 0) {
        json_decref(node);
        pthread_mutex_unlock(&state->mutex);
        return -ENOENT;
    }
    
    int result = jsonfs_set_value(state->root, to, node);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Renamed %s to %s\n", from, to);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void)fi;
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    pthread_mutex_lock(&state->mutex);
    
    json_t *node = jsonfs_get_node(state->root, path);
    if (!node || !json_is_string(node)) {
        pthread_mutex_unlock(&state->mutex);
        return -EINVAL;
    }
    
    const char *old_str = json_string_value(node);
    size_t old_len = strlen(old_str);
    size_t size_cast = (size < 0) ? 0 : (size_t)size;
    
    if (size_cast >= old_len) {
        pthread_mutex_unlock(&state->mutex);
        return 0;
    }
    
    char new_str[MAX_DATA];
    strncpy(new_str, old_str, size_cast);
    new_str[size_cast] = '\0';
    
    json_t *new_node = json_string(new_str);
    if (!new_node) {
        pthread_mutex_unlock(&state->mutex);
        return -ENOMEM;
    }
    
    int result = jsonfs_set_value(state->root, path, new_node);
    
    if (result == 0) {
        state->modified = 1;
        printf("JSONFS: Truncated %s\n", path);
    }
    
    pthread_mutex_unlock(&state->mutex);
    return result;
}

int jsonfs_open(const char *path, struct fuse_file_info *fi) {
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    if (strcmp(path, "/.save") == 0 || 
        strcmp(path, "/.modified") == 0 || 
        strcmp(path, "/.help") == 0) {
        fi->direct_io = 1;
        return 0;
    }
    
    pthread_mutex_lock(&state->mutex);
    json_t *node = jsonfs_get_node(state->root, path);
    pthread_mutex_unlock(&state->mutex);
    
    if (!node) {
        return -ENOENT;
    }
    
    if (json_is_object(node) || json_is_array(node)) {
        return -EISDIR;
    }
    
    fi->direct_io = 1;
    return 0;
}

int jsonfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)path;
    (void)isdatasync;
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    return jsonfs_force_save(state);
}

int jsonfs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi) {
    (void)path;
    (void)ts;
    (void)fi;
    return 0;
}