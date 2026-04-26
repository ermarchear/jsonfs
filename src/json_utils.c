#include "../include/jsonfs.h"
#include <fuse.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

// Превращает число в строку
char* number_to_string(double n) {
    char *buf = malloc(64);
    if (!buf) return NULL;
    
    if ((int)n == n) {
        snprintf(buf, 64, "%d", (int)n);
    } else {
        snprintf(buf, 64, "%g", n);
    }
    return buf;
}

// Поиск элемента по пути
struct json_object* find_by_path(const char *path, struct json_object *root) {
    if (strcmp(path, "/") == 0) return root;
    
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char *p = path_copy + 1;
    char *piece = strtok(p, "/");
    struct json_object *cur = root;
    
    while (piece && cur) {
        enum json_type type = json_object_get_type(cur);
        
        if (type == json_type_object) {
            cur = json_object_object_get(cur, piece);
        }
        else if (type == json_type_array) {
            int idx = atoi(piece);
            cur = json_object_array_get_idx(cur, idx);
        }
        else {
            cur = NULL;
            break;
        }
        piece = strtok(NULL, "/");
    }
    
    free(path_copy);
    return cur;
}

// Разделение пути на родителя и имя
int split_path(const char *path, char **parent_path, char **name) {
    char *path_copy = strdup(path);
    if (!path_copy) return -ENOMEM;
    
    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        free(path_copy);
        return -EINVAL;
    }
    
    *last_slash = '\0';
    *parent_path = strdup(path_copy);
    *name = strdup(last_slash + 1);
    
    free(path_copy);
    
    if (!*parent_path || !*name) {
        free(*parent_path);
        free(*name);
        return -ENOMEM;
    }
    
    return 0;
}

// Родительские директории
int ensure_parent_dirs(const char *path, struct jsonfs_data *data) {
    char *path_copy = strdup(path);
    if (!path_copy) return -ENOMEM;
    
    char *p = path_copy + 1;
    struct json_object *cur = data->json_root;
    
    while (p && *p) {
        char *next_slash = strchr(p, '/');
        if (next_slash) *next_slash = '\0';
        
        if (*p) {
            enum json_type type = json_object_get_type(cur);
            if (type != json_type_object) {
                free(path_copy);
                return -ENOTDIR;
            }
            
            struct json_object *child = json_object_object_get(cur, p);
            if (!child) {
                child = json_object_new_object();
                json_object_object_add(cur, p, child);
                data->is_modified = 1;
            }
            cur = child;
        }
        
        if (next_slash) {
            p = next_slash + 1;
        } else {
            break;
        }
    }
    
    free(path_copy);
    return 0;
}