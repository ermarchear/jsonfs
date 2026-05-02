#include "../include/common.h"
#include "../include/json_operations.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

json_t *normalize_json(json_t *root, int is_root) {
    json_t *obj = NULL;
    json_t *value = NULL;
    const char *key = NULL;
    char *transform_key = NULL;
    json_t *json_copy_ret = NULL;
    json_t *converted_val = NULL;
    
    CHECK_POINTER(root, NULL);
    obj = json_object();
    CHECK_POINTER(obj, NULL);

    if (json_is_object(root)) {
        json_object_foreach(root, key, value) {
            converted_val = normalize_json(value, 0);
            if (!converted_val) { goto handle_error; }
            if (strchr(key, '/')) {
                transform_key = replace_slash(key);
                json_object_set_new(obj, transform_key, converted_val);
                free(transform_key);
            }
            else {
                json_object_set_new(obj, key, converted_val);
            }
        }
    }
    else if (json_is_array(root)) {
        size_t idx;
        json_array_foreach(root, idx, value) {
            char key_str[32];
            snprintf(key_str, sizeof(key_str), "%s%zu", SPECIAL_PREFIX, idx);
            converted_val = normalize_json(value, 0);
            if (!converted_val) { goto handle_error; }
            json_object_set_new(obj, key_str, converted_val);
        }
    } 
    else {
        if (is_root) {
            json_copy_ret = json_copy(root);
            if (!json_copy_ret) { goto handle_error; }
            json_object_set_new(obj, SCALAR_NAME, json_copy_ret);
        }
        else {
            json_decref(obj);
            obj = json_copy(root);
            CHECK_POINTER(obj, NULL);
        }
    }

    return obj;
    
    handle_error:
        json_decref(obj);
        return NULL;
}

// Полная версия denormalize_json
json_t *denormalize_json(json_t *root) {
    json_t *result = NULL;
    json_t *value = NULL;
    const char *key = NULL;
    char *original_key = NULL;
    json_t *converted_val = NULL;
    int is_array = 0;
    size_t array_len = 0;
    
    CHECK_POINTER(root, NULL);
    
    // Скалярное значение на верхнем уровне
    if (json_is_string(root) || json_is_integer(root) || 
        json_is_real(root) || json_is_boolean(root) || json_is_null(root)) {
        return json_copy(root);
    }
    
    // Массив: ищем ключи вида @0, @1, @2...
    if (json_is_object(root)) {
        json_object_foreach(root, key, value) {
            if (key[0] == '@' && strlen(key) > 1) {
                char *endptr;
                long idx = strtol(key + 1, &endptr, 10);
                if (*endptr == '\0' && idx >= 0) {
                    is_array = 1;
                    if (idx + 1 > array_len) array_len = idx + 1;
                }
            }
        }
        
        if (is_array) {
            // Создаём массив JSON
            result = json_array();
            CHECK_POINTER(result, NULL);
            
            // Заполняем массив
            for (size_t i = 0; i < array_len; i++) {
                char key_str[32];
                snprintf(key_str, sizeof(key_str), "%s%zu", SPECIAL_PREFIX, i);
                json_t *item = json_object_get(root, key_str);
                if (item) {
                    converted_val = denormalize_json(item);
                    if (!converted_val) { goto handle_error; }
                    json_array_append_new(result, converted_val);
                } else {
                    json_array_append_new(result, json_null());
                }
            }
            return result;
        }
    }
    
    // Обычный объект
    if (json_is_object(root)) {
        result = json_object();
        CHECK_POINTER(result, NULL);
        
        json_object_foreach(root, key, value) {
            // Пропускаем скалярный маркер
            if (strcmp(key, SCALAR_NAME) == 0) {
                json_decref(result);
                return denormalize_json(value);
            }
            
            // Пропускаем элементы массива (уже обработаны)
            if (key[0] == '@' && strlen(key) > 1) {
                char *endptr;
                strtol(key + 1, &endptr, 10);
                if (*endptr == '\0') {
                    continue;
                }
            }
            
            // Восстанавливаем оригинальное имя ключа
            if (strstr(key, SPECIAL_SLASH)) {
                original_key = reverse_replace_slash(key);
            } else {
                original_key = strdup(key);
            }
            
            if (!original_key) { goto handle_error; }
            
            converted_val = denormalize_json(value);
            if (!converted_val) { 
                free(original_key);
                goto handle_error; 
            }
            
            json_object_set_new(result, original_key, converted_val);
            free(original_key);
            original_key = NULL;
        }
        
        return result;
    }
    
    // Скаляр
    return json_copy(root);
    
    handle_error:
        free(original_key);
        json_decref(result);
        return NULL;
}

json_t *find_json_node(const char *path, json_t *root) {
    char *path_dup = NULL;
    char *saveptr = NULL;

    CHECK_POINTER(path, NULL);
    CHECK_POINTER(root, NULL);

    if (strcmp(path, "/") == 0) { return root; }

    path_dup = strdup(path);
    CHECK_POINTER(path_dup, NULL);

    char *key = strtok_r(path_dup, "/", &saveptr);
    if (!key) { goto handle_error; }

    json_t *curr_obj = root;

    while(key) {
        if (!json_is_object(curr_obj)) { goto handle_error; }
        curr_obj = json_object_get(curr_obj, key);
        if (!curr_obj) { goto handle_error; }
        key = strtok_r(NULL, "/", &saveptr);
    }

    free(path_dup);
    return curr_obj;

    handle_error:
        free(path_dup);
        return NULL;
}

int find_parent_and_key(json_t *root, json_t *node, json_t **parent, const char **key) {
    json_t *v = NULL;
    const char *k = NULL;
    int res_find;

    CHECK_POINTER(root, -EFAULT);
    CHECK_POINTER(node, -EFAULT);
    CHECK_POINTER(parent, -EFAULT);
    CHECK_POINTER(key, -EFAULT);

    if (json_equal(root, node)) { return -EINVAL; }

    if (json_is_object(root)) {
        json_object_foreach(root, k, v) {
            if (node == v) {
                *parent = root;
                *key = k;
                return 0;
            }

            res_find = find_parent_and_key(v, node, parent, key);
            if (res_find == 0) { return 0; }
        }
    }

    return -ENOENT;
}

int spec_prefix_is_present(json_t *root) {
    const char *key = NULL;
    json_t *value = NULL;
    int found = 0;

    if (!json_is_object(root)) return 0;

    json_object_foreach(root, key, value) {
        if (strstr(key, SPECIAL_PREFIX)) {
            return 1;
        }

        if (json_is_object(value)) { 
            found = spec_prefix_is_present(value); 
        }

        if (found) { return 1; }
    }

    return 0;
}

int find_keys_with_spec_slash(json_t *root, json_t **results, int max_results, int count) {
    const char *key = NULL;
    json_t *value = NULL;
    
    if (!json_is_object(root)) return count;
    
    json_object_foreach(root, key, value) {
        if (strstr(key, SPECIAL_SLASH)) {
            if (count < max_results) {
                results[count] = value;
                count++;
            }
        }
        
        if (json_is_object(value)) {
            count = find_keys_with_spec_slash(value, results, max_results, count);
        }
    }
    
    return count;
}

int find_array_in_normal_root(json_t *root, json_t **results, int max_results, int count) {
    const char *key = NULL;
    json_t *value = NULL;
    
    if (!json_is_object(root)) return count;
    
    json_object_foreach(root, key, value) {
        if (key[0] == '@' && !strstr(key, SPECIAL_SLASH)) {
            if (count < max_results) {
                results[count] = root;
                count++;
            }
            break;
        }
    }

    json_object_foreach(root, key, value) {
        if (json_is_object(value)) {
            count = find_array_in_normal_root(value, results, max_results, count);
        }
    }

    return count;
}

int replace_json_nodes(json_t *old_node, json_t *new_node, json_t *root) {
    json_t *parent = NULL;
    const char *key = NULL;
    int res_find;

    CHECK_POINTER(old_node, -EFAULT);
    CHECK_POINTER(new_node, -EFAULT);
    CHECK_POINTER(root, -EFAULT);

    res_find = find_parent_and_key(root, old_node, &parent, &key);
    if (res_find < 0) { return res_find; }

    if (json_is_object(parent)) {
        json_object_set_new(parent, key, json_copy(new_node));
    } else {
        return -ENOENT;
    }

    json_decref(new_node);
    return 0;
}

int count_subdirs(json_t *obj) {
    int count = 0;
    const char *key = NULL;
    json_t *value = NULL;

    if (!obj || !json_is_object(obj)) {
        return 0;
    }

    json_object_foreach(obj, key, value) {
        if (json_is_object(value)) {
            count++;
        }
    }

    return count;
}

int is_special_file(const char *path) {
    if (strcmp("/.status", path) == 0 || 
        strcmp(".status", path) == 0 ||
        strcmp("/.save", path) == 0 || 
        strcmp(".save", path) == 0) {
        return 1;
    }
    return 0;
}

char *replace_slash(const char *key) {
    char *key_dup = NULL;
    char *new_key = NULL;
    char *part_of_key = NULL;
    char *saveptr = NULL;
    size_t offset = 0;
    int count_slash = 0;
    int is_first_part = 1;

    CHECK_POINTER(key, NULL);

    key_dup = strdup(key);
    if (!key_dup) { goto handle_error; }

    for (size_t i = 0; i < strlen(key_dup); i++) {
        if (key_dup[i] == '/') { count_slash++; }
    }

    new_key = calloc(1, strlen(key) + strlen(SPECIAL_SLASH) * count_slash + 1);
    if (!new_key) { goto handle_error; }

    part_of_key = strtok_r(key_dup, "/", &saveptr);
    while (part_of_key) {
        if (!is_first_part) {
            snprintf(new_key + offset, strlen(SPECIAL_SLASH) + 1, "%s", SPECIAL_SLASH);
            offset += strlen(SPECIAL_SLASH);
        }
        is_first_part = 0;

        snprintf(new_key + offset, strlen(part_of_key) + 1, "%s", part_of_key);
        offset += strlen(part_of_key);

        part_of_key = strtok_r(NULL, "/", &saveptr);
    }

    free(key_dup);
    return new_key;

    handle_error:
        free(key_dup);
        free(new_key);
        return NULL;
}

char *reverse_replace_slash(const char *key) {
    char *key_dup = NULL;
    char *ptr = NULL;

    CHECK_POINTER(key, NULL);

    key_dup = strdup(key);
    if (!key_dup) { goto handle_error; }

    ptr = key_dup;
    while ((ptr = strstr(ptr, SPECIAL_SLASH)) != NULL) {
        *ptr = '/';
        memmove(ptr + 1, ptr + strlen(SPECIAL_SLASH), strlen(ptr + strlen(SPECIAL_SLASH)) + 1);
        ptr++;
    }
   
    return key_dup;

    handle_error:
        free(key_dup);
        return NULL;
}

int separate_filepath(const char *path, char **parent_path, char **basename) {
    char *path_dup = NULL;
    char *last_slash = NULL;

    CHECK_POINTER(path, -1);
    CHECK_POINTER(parent_path, -1);
    CHECK_POINTER(basename, -1);

    *parent_path = NULL;
    *basename = NULL;

    path_dup = strdup(path);
    CHECK_POINTER(path_dup, -1);

    last_slash = strrchr(path_dup, '/');

    if (last_slash) {
        *last_slash = '\0';

        *basename = strdup(last_slash + 1);
        if (!*basename) { goto handle_error; }

        if (path_dup[0] == '\0') {
            *parent_path = strdup("/");
            if (!*parent_path) { goto handle_error; }
        }
        else {
            *parent_path = path_dup;
            path_dup = NULL;
        }
    }
    else {
        *basename = path_dup;
        path_dup = NULL;
        if (!*basename) { goto handle_error; }

        *parent_path = strdup(".");
        if (!*parent_path) { goto handle_error; }
    }

    free(path_dup);
    return 0;

    handle_error:
        free(path_dup);
        free(*parent_path);
        free(*basename);
        return -1;
}