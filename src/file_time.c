#include "../include/common.h"
#include "../include/file_time.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

struct file_time* find_node_file_time(const char *path, struct file_time *root) {
    struct file_time *curr = root;
    while (curr) {
        if (strcmp(path, curr->path) == 0) {
            return curr;
        }
        curr = curr->next_node;
    }
    return NULL;
}

struct file_time* add_node_to_list_ft(const char *path, struct file_time *root, 
                                      enum set_time flags) {
    char *path_dup = strdup(path);
    if (!path_dup) return NULL;
    
    struct file_time *new_node = calloc(1, sizeof(struct file_time));
    if (!new_node) {
        free(path_dup);
        return NULL;
    }
    
    time_t now = time(NULL);
    new_node->path = path_dup;
    new_node->atime = now;
    new_node->mtime = now;
    new_node->ctime = now;
    new_node->next_node = NULL;
    
    if (!root) return new_node;
    
    struct file_time *last = root;
    while (last->next_node) {
        last = last->next_node;
    }
    last->next_node = new_node;
    
    return new_node;
}

// Исправленная версия — возвращает новый корень
struct file_time* remove_node_to_list_ft(const char *path, struct file_time *root) {
    struct file_time *curr = root;
    struct file_time *prev = NULL;
    
    while (curr) {
        if (strcmp(path, curr->path) == 0) {
            if (prev) {
                prev->next_node = curr->next_node;
            } else {
                // Удаляем корневой узел
                root = curr->next_node;
            }
            free_file_time(curr);
            return root;
        }
        prev = curr;
        curr = curr->next_node;
    }
    
    return root;
}

void free_file_time_list(struct file_time *root) {
    struct file_time *curr = root;
    while (curr) {
        struct file_time *next = curr->next_node;
        free_file_time(curr);
        curr = next;
    }
}

void free_file_time(struct file_time *ft) {
    if (!ft) return;
    free(ft->path);
    free(ft);
}