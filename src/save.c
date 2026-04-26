#include "../include/jsonfs.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <json-c/json.h>
#include <errno.h>

// Сохранение JSON в файл
int save_json(struct jsonfs_data *data) {
    if (!data->is_modified) return 0;
    
    if (json_object_to_file(data->json_filename, data->json_root) != 0) {
        return -EIO;
    }
    data->is_modified = 0;
    return 0;
}