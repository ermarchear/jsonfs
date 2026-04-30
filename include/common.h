#ifndef COMMON_H
#define COMMON_H

#define FUSE_USE_VERSION 35

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#define MID_SIZE 256
#define BIG_SIZE 1024
#define SHRT_SIZE 32

#define SCALAR_NAME "_scalar"
#define SPECIAL_PREFIX "@"
#define SPECIAL_SLASH "__s__"

#define CHECK_POINTER(ptr, ret) if (!(ptr)) { return (ret); }
#define FILL_OR_RETURN(buf, name) do { \
    if (filler(buf, name, NULL, 0, 0) != 0) return -ENOMEM; \
} while(0)

#endif