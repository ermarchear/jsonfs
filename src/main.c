#define FUSE_USE_VERSION 35

#include "jsonfs.h"
#include <stdlib.h>

static struct fuse_operations jsonfs_ops = {
    .init       = jsonfs_init,
    .destroy    = jsonfs_destroy,
    .getattr    = jsonfs_getattr,
    .readdir    = jsonfs_readdir,
    .open       = jsonfs_open,
    .read       = jsonfs_read,
    .write      = jsonfs_write,
    .create     = jsonfs_create,
    .unlink     = jsonfs_unlink,
    .mkdir      = jsonfs_mkdir,
    .rmdir      = jsonfs_rmdir,
    .rename     = jsonfs_rename,
    .truncate   = jsonfs_truncate,
    .utimens    = jsonfs_utimens,
    .fsync      = jsonfs_fsync,
};

void print_usage(const char *progname) {
    fprintf(stderr, "JSONFS - JSON Filesystem v2.0\n");
    fprintf(stderr, "Usage: %s <json_file> <mountpoint> [options]\n", progname);
    fprintf(stderr, "\nSpecial files in mount point:\n");
    fprintf(stderr, "  .save      - Write anything to save changes\n");
    fprintf(stderr, "  .modified  - Check if changes exist (1=yes, 0=no)\n");
    fprintf(stderr, "  .sync      - Sync with disk\n");
    fprintf(stderr, "  .help      - Display this help\n");
    fprintf(stderr, "\nType detection:\n");
    fprintf(stderr, "  true/false -> boolean\n");
    fprintf(stderr, "  123       -> integer\n");
    fprintf(stderr, "  12.34     -> float\n");
    fprintf(stderr, "  null      -> null\n");
    fprintf(stderr, "  text      -> string\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s data.json /mnt/jsonfs\n", progname);
    fprintf(stderr, "  echo save > /mnt/jsonfs/.save\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    jsonfs_state *state = malloc(sizeof(jsonfs_state));
    if (!state) {
        perror("malloc");
        return 1;
    }
    
    state->json_path = realpath(argv[1], NULL);
    if (!state->json_path) {
        perror("realpath");
        free(state);
        return 1;
    }
    
    state->root = NULL;
    state->modified = 0;
    state->last_save_time = time(NULL);
    
    printf("JSONFS v2.0 starting\n");
    printf("JSON file: %s\n", state->json_path);
    printf("Mount point: %s\n", argv[2]);
    printf("Type detection: ON\n");
    printf("Changes will NOT be saved automatically. Use .save file to save.\n");
    printf("Swap files are ignored.\n");
    
    char *fuse_argv[20];
    int fuse_argc = 0;
    
    fuse_argv[fuse_argc++] = argv[0];
    fuse_argv[fuse_argc++] = argv[2];
    fuse_argv[fuse_argc++] = "-f";
    fuse_argv[fuse_argc++] = "-s";
    
    for (int i = 3; i < argc; i++) {
        fuse_argv[fuse_argc++] = argv[i];
    }
    
    return fuse_main(fuse_argc, fuse_argv, &jsonfs_ops, state);
}