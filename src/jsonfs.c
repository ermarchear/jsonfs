#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

// Проверка swap файлов (vim, emacs, etc)
static int is_swap_file(const char *path) {
    const char *name = strrchr(path, '/');
    if (name) name++; else name = path;
    
    // Vim swap files
    if (strstr(name, ".swp") != NULL) return 1;
    if (strstr(name, ".swx") != NULL) return 1;
    if (strstr(name, ".swo") != NULL) return 1;
    
    // Emacs backup files
    if (strstr(name, "~") != NULL) return 1;
    if (strstr(name, "#") != NULL) return 1;
    
    // Temporary files
    if (strstr(name, ".tmp") != NULL) return 1;
    if (strstr(name, ".bak") != NULL) return 1;
    
    return 0;
}

// Определение типа значения по строке
static json_t* parse_value_by_content(const char *str) {
    char *endptr;
    
    // Trim whitespace
    while (isspace(*str)) str++;
    
    // Проверяем null
    if (strcmp(str, "null") == 0) {
        return json_null();
    }
    // Проверяем boolean
    if (strcmp(str, "true") == 0) {
        return json_true();
    }
    if (strcmp(str, "false") == 0) {
        return json_false();
    }
    // Проверяем число (целое)
    long long int_val = strtoll(str, &endptr, 10);
    if (*endptr == '\0' || isspace(*endptr)) {
        return json_integer(int_val);
    }
    // Проверяем число с плавающей точкой
    double real_val = strtod(str, &endptr);
    if (*endptr == '\0' || isspace(*endptr)) {
        return json_real(real_val);
    }
    // По умолчанию строка
    return json_string(str);
}

// Сохранение JSON на диск
static int jsonfs_save(jsonfs_state *state) {
    if (!state || !state->json_path || !state->root) {
        return -EINVAL;
    }
    
    char temp_path[MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", state->json_path);
    
    FILE *f = fopen(temp_path, "w");
    if (!f) return -EIO;
    
    json_dumpf(state->root, f, JSON_INDENT(2) | JSON_ENSURE_ASCII);
    fclose(f);
    
    if (rename(temp_path, state->json_path) != 0) {
        unlink(temp_path);
        return -EIO;
    }
    
    state->modified = 0;
    state->last_save_time = time(NULL);
    
    return 0;
}

int jsonfs_force_save(jsonfs_state *state) {
    if (!state) return -EINVAL;
    return jsonfs_save(state);
}

int jsonfs_is_modified(jsonfs_state *state) {
    if (!state) return 0;
    return state->modified;
}

void* jsonfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    cfg->kernel_cache = 0;
    cfg->use_ino = 1;
    
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
    
    state->modified = 0;
    state->last_save_time = time(NULL);
    
    return state;
}

void jsonfs_destroy(void *private_data) {
    jsonfs_state *state = (jsonfs_state*)private_data;
    
    if (state->modified) {
        fprintf(stderr, "\nWARNING: There are unsaved changes in %s\n", state->json_path);
        fprintf(stderr, "To save them, write to .save file before unmounting\n");
    }
    
    json_decref(state->root);
}

int jsonfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    time_t now = time(NULL);
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_atime = now;
        stbuf->st_mtime = state->modified ? now : state->last_save_time;
        stbuf->st_ctime = now;
        return 0;
    }
    
    // Специальные файлы
    if (strcmp(path, "/.save") == 0 || strcmp(path, "/.modified") == 0 || 
        strcmp(path, "/.help") == 0 || strcmp(path, "/.sync") == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_size = 4096;
        stbuf->st_nlink = 1;
        stbuf->st_atime = now;
        stbuf->st_mtime = now;
        stbuf->st_ctime = now;
        return 0;
    }
    
    json_t *node = jsonfs_get_node(state->root, path);
    
    if (!node) {
        return -ENOENT;
    }
    
    if (json_is_object(node)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
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
        stbuf->st_size = 4096;
    } else {
        return -ENOENT;
    }
    
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = now;
    stbuf->st_mtime = state->modified ? now : state->last_save_time;
    stbuf->st_ctime = now;
    
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
        filler(buf, ".sync", NULL, 0, 0);
    }
    
    json_t *dir = jsonfs_get_node(state->root, path);
    
    if (!dir) {
        return 0;
    }
    
    if (json_is_object(dir)) {
        const char *key;
        json_t *value;
        json_object_foreach(dir, key, value) {
            // Пропускаем специальные файлы
            if (strcmp(path, "/") == 0) {
                if (key[0] == '.') continue;
            }
            // Пропускаем ключи со слешами
            if (strchr(key, '/') != NULL) continue;
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
    
    return 0;
}

int jsonfs_read(const char *path, char *buf, size_t size, off_t offset, 
                struct fuse_file_info *fi) {
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    // Специальные файлы
    if (strcmp(path, "/.help") == 0) {
        const char *help = 
            "JSONFS - JSON Filesystem\n"
            "========================\n"
            "\n"
            "Special files:\n"
            "  .save      - Write anything to save changes\n"
            "  .modified  - Check if changes exist (1=yes, 0=no)\n"
            "  .sync      - Sync with disk\n"
            "  .help      - This help\n"
            "\n"
            "Type detection:\n"
            "  true/false -> boolean\n"
            "  123       -> integer\n"
            "  12.34     -> float\n"
            "  null      -> null\n"
            "  text      -> string (default)\n"
            "\n"
            "To save changes:\n"
            "  $ echo save > /mnt/.save\n";
        
        size_t len = strlen(help);
        if ((size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, help + offset, size);
        return size;
    }
    
    if (strcmp(path, "/.modified") == 0) {
        char modified_str[64];
        snprintf(modified_str, sizeof(modified_str), "%d\n", state->modified);
        size_t len = strlen(modified_str);
        if ((size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, modified_str + offset, size);
        return size;
    }
    
    if (strcmp(path, "/.save") == 0) {
        const char *msg = "Write 'save' here to save changes\n";
        size_t len = strlen(msg);
        if ((size_t)offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, msg + offset, size);
        return size;
    }
    
    json_t *node = jsonfs_get_node(state->root, path);
    
    if (!node) {
        return -ENOENT;
    }
    
    if (json_is_object(node) || json_is_array(node)) {
        return -EISDIR;
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
        return -EIO;
    }
    
    size_t len = strlen(str_value);
    
    if ((size_t)offset >= len) {
        return 0;
    }
    
    if ((size_t)(offset + size) > len) {
        size = len - offset;
    }
    
    memcpy(buf, str_value + offset, size);
    
    return size;
}

int jsonfs_write(const char *path, const char *buf, size_t size, 
                 off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    (void)offset;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    // Игнорируем swap файлы
    if (is_swap_file(path)) {
        return size;
    }
    
    // Обработка .save файла
    if (strcmp(path, "/.save") == 0) {
        return (jsonfs_force_save(state) == 0) ? (int)size : -EIO;
    }
    
    // Обработка .sync файла
    if (strcmp(path, "/.sync") == 0) {
        sync();
        return size;
    }
    
    char content[MAX_DATA];
    size_t copy_size = size > MAX_DATA - 1 ? MAX_DATA - 1 : size;
    memcpy(content, buf, copy_size);
    content[copy_size] = '\0';
    
    // Удаляем trailing newline если есть
    size_t len = strlen(content);
    if (len > 0 && content[len-1] == '\n') {
        content[len-1] = '\0';
    }
    
    // Парсим содержимое для определения типа
    json_t *new_value = parse_value_by_content(content);
    
    int result = jsonfs_set_value(state->root, path, new_value);
    
    if (result == 0) {
        state->modified = 1;
    } else {
        json_decref(new_value);
    }
    
    return (result == 0) ? (int)size : result;
}

int jsonfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)mode;
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    // Игнорируем swap файлы
    if (is_swap_file(path)) {
        return -ENOENT;
    }
    
    json_t *existing = jsonfs_get_node(state->root, path);
    if (existing) {
        return -EEXIST;
    }
    
    // Создаем пустую строку
    json_t *value = json_string("");
    int result = jsonfs_set_value(state->root, path, value);
    
    if (result == 0) {
        state->modified = 1;
    }
    
    return result;
}

int jsonfs_unlink(const char *path) {
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    if (is_swap_file(path)) {
        return -ENOENT;
    }
    
    int result = jsonfs_delete_path(state->root, path);
    
    if (result == 0) {
        state->modified = 1;
    }
    
    return result;
}

int jsonfs_mkdir(const char *path, mode_t mode) {
    (void)mode;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    json_t *existing = jsonfs_get_node(state->root, path);
    if (existing) {
        return -EEXIST;
    }
    
    json_t *dir = json_object();
    int result = jsonfs_set_value(state->root, path, dir);
    
    if (result == 0) {
        state->modified = 1;
    } else {
        json_decref(dir);
    }
    
    return result;
}

int jsonfs_rmdir(const char *path) {
    return jsonfs_unlink(path);
}

int jsonfs_rename(const char *from, const char *to, unsigned int flags) {
    (void)flags;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    // Если пути совпадают, ничего не делаем
    if (strcmp(from, to) == 0) {
        return 0;
    }
    
    json_t *node = jsonfs_get_node(state->root, from);
    if (!node) {
        return -ENOENT;
    }
    
    json_incref(node);
    
    // Проверяем, существует ли целевой файл
    json_t *existing = jsonfs_get_node(state->root, to);
    
    int result;
    if (existing) {
        // Если целевой файл существует, удаляем его (замена)
        if (jsonfs_delete_path(state->root, to) != 0) {
            json_decref(node);
            return -ENOENT;
        }
    }
    
    // Удаляем исходный файл
    if (jsonfs_delete_path(state->root, from) != 0) {
        json_decref(node);
        return -ENOENT;
    }
    
    // Создаем новый файл с данными исходного
    result = jsonfs_set_value(state->root, to, node);
    
    if (result == 0) {
        state->modified = 1;
    } else {
        json_decref(node);
    }
    
    return result;
}

int jsonfs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    json_t *node = jsonfs_get_node(state->root, path);
    if (!node || !json_is_string(node)) {
        return -EINVAL;
    }
    
    const char *old_str = json_string_value(node);
    size_t old_len = strlen(old_str);
    
    size_t size_cast = (size < 0) ? 0 : (size_t)size;
    
    if (size_cast >= old_len) {
        return 0;
    }
    
    char new_str[MAX_DATA];
    strncpy(new_str, old_str, size_cast);
    new_str[size_cast] = '\0';
    
    json_t *new_node = json_string(new_str);
    if (!new_node) {
        return -ENOMEM;
    }
    
    int result = jsonfs_set_value(state->root, path, new_node);
    
    if (result == 0) {
        state->modified = 1;
    }
    
    return result;
}

int jsonfs_open(const char *path, struct fuse_file_info *fi) {
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    
    if (is_swap_file(path)) {
        return -ENOENT;
    }
    
    if (strcmp(path, "/.save") == 0 || strcmp(path, "/.modified") == 0 || 
        strcmp(path, "/.help") == 0 || strcmp(path, "/.sync") == 0) {
        fi->direct_io = 1;
        return 0;
    }
    
    json_t *node = jsonfs_get_node(state->root, path);
    
    if (!node) {
        return -ENOENT;
    }
    
    if (json_is_object(node) || json_is_array(node)) {
        return -EISDIR;
    }
    
    fi->direct_io = 1;
    return 0;
}

int jsonfs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi) {
    (void)path;
    (void)ts;
    (void)fi;
    return 0;
}

int jsonfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)path;
    (void)isdatasync;
    (void)fi;
    
    jsonfs_state *state = (jsonfs_state*)fuse_get_context()->private_data;
    return jsonfs_force_save(state);
}