#include "../include/jsonfs.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Поиск узла с временными метками
struct file_time* find_file_time(const char *path, struct file_time *root) {
    struct file_time *curr = root;
    while (curr) {
        if (strcmp(path, curr->path) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// Добавление нового узла с временными метками
struct file_time* add_file_time(const char *path, struct file_time *root) {
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
    new_node->next = NULL;
    
    if (!root) return new_node;
    
    struct file_time *last = root;
    while (last->next) {
        last = last->next;
    }
    last->next = new_node;
    
    return new_node;
}

// Обновление времени доступа
void update_atime(const char *path, struct jsonfs_data *data) {
    struct file_time *ft = find_file_time(path, data->ft_list);
    if (ft) {
        ft->atime = time(NULL);
    } else {
        ft = add_file_time(path, data->ft_list);
        if (ft && !data->ft_list) data->ft_list = ft;
    }
}

// Обновление времени модификации
void update_mtime(const char *path, struct jsonfs_data *data) {
    struct file_time *ft = find_file_time(path, data->ft_list);
    if (ft) {
        ft->mtime = time(NULL);
        ft->ctime = time(NULL);
    } else {
        ft = add_file_time(path, data->ft_list);
        if (ft && !data->ft_list) data->ft_list = ft;
    }
}

// Обновление времени изменения метаданных
void update_ctime(const char *path, struct jsonfs_data *data) {
    struct file_time *ft = find_file_time(path, data->ft_list);
    if (ft) {
        ft->ctime = time(NULL);
    } else {
        ft = add_file_time(path, data->ft_list);
        if (ft && !data->ft_list) data->ft_list = ft;
    }
}

// Освобождение списка временных меток
void free_file_time_list(struct file_time *root) {
    struct file_time *curr = root;
    while (curr) {
        struct file_time *next = curr->next;
        free(curr->path);
        free(curr);
        curr = next;
    }
}