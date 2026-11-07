#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_FILENAME 32
#define MAX_FILES 64

typedef struct vfs_node {
    char name[MAX_FILENAME];
    char *content;
    size_t size;
    int is_used;
} vfs_node_t;

void vfs_init(void);
int vfs_create(const char *name);
int vfs_write(const char *name, const char *data);
char *vfs_read(const char *name);
int vfs_delete(const char *name);
void vfs_list(void);
void vfs_run_test(void);

#endif