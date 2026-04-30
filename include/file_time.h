#ifndef FILE_TIME_H
#define FILE_TIME_H

#include <time.h>

enum set_time {
    SET_ATIME = 1 << 0,
    SET_MTIME = 1 << 1,
    SET_CTIME = 1 << 2
};

struct file_time {
    char *path;
    time_t atime;
    time_t mtime;
    time_t ctime;
    struct file_time *next_node;   // ← next_node, а не next
};

struct file_time *add_node_to_list_ft(const char* path, struct file_time *root, 
                                      enum set_time flags);
int remove_node_to_list_ft(const char* path, struct file_time *root);
struct file_time *find_node_file_time(const char *path, struct file_time *root);
void free_file_time(struct file_time *ft);

#endif