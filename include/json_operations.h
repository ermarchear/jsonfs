#ifndef JSON_OPERATIONS_H
#define JSON_OPERATIONS_H

#include <jansson.h>

#define SCALAR_NAME "_scalar"
#define SPECIAL_PREFIX "@"
#define SPECIAL_SLASH "__s__"

json_t *normalize_json(json_t *root, int is_root);
json_t *denormalize_json(json_t *root);
json_t *find_json_node(const char *path, json_t *root);
int find_parent_and_key(json_t *root, json_t *node, json_t **parent, const char **key);
int spec_prefix_is_present(json_t *root);
int find_keys_with_spec_slash(json_t *root, json_t **results, int max_results, int count);
int find_array_in_normal_root(json_t *root, json_t **results, int max_results, int count);
int replace_json_nodes(json_t *old_node, json_t *new_node, json_t *root);
int count_subdirs(json_t *obj);
int is_special_file(const char *path);
char *replace_slash(const char *key);
char *reverse_replace_slash(const char *key);
int separate_filepath(const char *path, char **parent_path, char **basename);

#endif