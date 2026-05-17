#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Проверка валидности ключа (без слэшей и спецсимволов)
static bool is_valid_key(const char *key) {
    if (!key || strlen(key) == 0) return false;
    if (strchr(key, '/') != NULL) return false;
    if (strcmp(key, ".") == 0 || strcmp(key, "..") == 0) return false;
    return true;
}

json_t* jsonfs_get_node(json_t *root, const char *path) {
    if (!root || !path) return NULL;
    
    if (strcmp(path, "/") == 0) {
        return root;
    }
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    
    json_t *current = root;
    char *token = strtok(path_copy + 1, "/");
    
    while (token) {
        if (!current) {
            return NULL;
        }
        
        if (!is_valid_key(token)) {
            return NULL;
        }
        
        if (json_is_object(current)) {
            current = json_object_get(current, token);
        } else if (json_is_array(current)) {
            char *endptr;
            long idx = strtol(token, &endptr, 10);
            if (*endptr == '\0' && idx >= 0) {
                current = json_array_get(current, idx);
            } else {
                return NULL;
            }
        } else {
            return NULL;
        }
        
        token = strtok(NULL, "/");
    }
    
    return current;
}

int jsonfs_set_value(json_t *root, const char *path, json_t *value) {
    if (!root || !path || !value) {
        if (value) json_decref(value);
        return -EINVAL;
    }
    
    if (strcmp(path, "/") == 0) {
        json_decref(value);
        return -EINVAL;
    }
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    
    json_t *current = root;
    char *last_key = NULL;
    char *token = strtok(path_copy + 1, "/");
    
    while (token) {
        char *next_token = strtok(NULL, "/");
        
        if (next_token == NULL) {
            last_key = token;
            break;
        }
        
        if (!is_valid_key(token)) {
            json_decref(value);
            return -EINVAL;
        }
        
        if (json_is_object(current)) {
            json_t *next = json_object_get(current, token);
            if (!next) {
                next = json_object();
                json_object_set_new(current, token, next);
            }
            current = next;
        } else if (json_is_array(current)) {
            char *endptr;
            long idx = strtol(token, &endptr, 10);
            if (*endptr == '\0' && idx >= 0) {
                json_t *next = json_array_get(current, idx);
                if (!next) {
                    while ((size_t)json_array_size(current) <= (size_t)idx) {
                        json_array_append_new(current, json_object());
                    }
                    next = json_array_get(current, idx);
                }
                current = next;
            } else {
                json_decref(value);
                return -ENOENT;
            }
        } else {
            json_decref(value);
            return -ENOTDIR;
        }
        
        token = next_token;
    }
    
    if (!last_key) {
        json_decref(value);
        return -ENOENT;
    }
    
    if (!is_valid_key(last_key) && !isdigit(last_key[0])) {
        json_decref(value);
        return -EINVAL;
    }
    
    // Устанавливаем значение
    if (json_is_object(current)) {
        json_object_set_new(current, last_key, value);
        return 0;
    } else if (json_is_array(current)) {
        char *endptr;
        long idx = strtol(last_key, &endptr, 10);
        if (*endptr == '\0' && idx >= 0) {
            while ((size_t)json_array_size(current) <= (size_t)idx) {
                json_array_append_new(current, json_null());
            }
            json_array_set_new(current, idx, value);
            return 0;
        }
    }
    
    json_decref(value);
    return -ENOTDIR;
}

int jsonfs_delete_path(json_t *root, const char *path) {
    if (!root || !path) return -EINVAL;
    
    if (strcmp(path, "/") == 0) {
        return -EBUSY;
    }
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    
    json_t *current = root;
    json_t *parent = NULL;
    char *last_key = NULL;
    char *token = strtok(path_copy + 1, "/");
    
    while (token) {
        char *next_token = strtok(NULL, "/");
        
        if (next_token == NULL) {
            last_key = token;
            break;
        }
        
        if (!is_valid_key(token)) {
            return -ENOENT;
        }
        
        parent = current;
        
        if (json_is_object(current)) {
            current = json_object_get(current, token);
        } else if (json_is_array(current)) {
            char *endptr;
            long idx = strtol(token, &endptr, 10);
            if (*endptr == '\0' && idx >= 0) {
                current = json_array_get(current, idx);
            } else {
                return -ENOENT;
            }
        } else {
            return -ENOTDIR;
        }
        
        if (!current) {
            return -ENOENT;
        }
        
        token = next_token;
    }
    
    if (!last_key) {
        return -ENOENT;
    }
    
    // Удаляем последний элемент
    if (parent && json_is_object(parent)) {
        if (is_valid_key(last_key)) {
            json_object_del(parent, last_key);
            return 0;
        }
    } else if (parent && json_is_array(parent)) {
        char *endptr;
        long idx = strtol(last_key, &endptr, 10);
        if (*endptr == '\0' && idx >= 0) {
            json_array_remove(parent, idx);
            return 0;
        }
    } else if (json_is_object(current)) {
        if (is_valid_key(last_key)) {
            json_object_del(current, last_key);
            return 0;
        }
    }
    
    return -ENOENT;
}