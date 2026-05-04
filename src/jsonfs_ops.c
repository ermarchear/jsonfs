#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

json_t* jsonfs_get_node(json_t *root, const char *path) {
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
    
    if (parent && json_is_object(parent)) {
        json_object_del(parent, last_key);
        return 0;
    } else if (parent && json_is_array(parent)) {
        char *endptr;
        long idx = strtol(last_key, &endptr, 10);
        if (*endptr == '\0' && idx >= 0) {
            json_array_remove(parent, idx);
            return 0;
        }
    } else if (json_is_object(current)) {
        json_object_del(current, last_key);
        return 0;
    } else if (json_is_array(current)) {
        char *endptr;
        long idx = strtol(last_key, &endptr, 10);
        if (*endptr == '\0' && idx >= 0) {
            json_array_remove(current, idx);
            return 0;
        }
    }
    
    return -ENOENT;
}

int jsonfs_create_path(json_t *root, const char *path, json_t *value) {
    return jsonfs_set_value(root, path, value);
}

char** jsonfs_list_dir(json_t *root, const char *path, int *count) {
    json_t *dir = jsonfs_get_node(root, path);
    if (!dir || (!json_is_object(dir) && !json_is_array(dir))) {
        *count = 0;
        return NULL;
    }
    
    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));
    *count = 0;
    
    if (json_is_object(dir)) {
        const char *key;
        json_t *value;
        json_object_foreach(dir, key, value) {
            if (*count >= capacity) {
                capacity *= 2;
                list = realloc(list, capacity * sizeof(char*));
            }
            list[*count] = strdup(key);
            (*count)++;
        }
    } else if (json_is_array(dir)) {
        for (size_t i = 0; i < json_array_size(dir); i++) {
            if (*count >= capacity) {
                capacity *= 2;
                list = realloc(list, capacity * sizeof(char*));
            }
            char idx[32];
            snprintf(idx, sizeof(idx), "%zu", i);
            list[*count] = strdup(idx);
            (*count)++;
        }
    }
    
    return list;
}