#include <stdint.h>
#include <stddef.h>
#include "vfs.h"
#include "memory.h"
#include "../include/kernel.h"

static vfs_node_t file_table[MAX_FILES];

static int str_len(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

static int str_cmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static void str_copy(char *dest, const char *src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void vfs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        file_table[i].is_used = 0;
        file_table[i].content = NULL;
        file_table[i].size = 0;
        file_table[i].name[0] = '\0';
    }
    kprintf("[ OK ] Virtual file system initialized (RAMFS)\n");
}

int vfs_create(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && str_cmp(file_table[i].name, name) == 0) {
            return -1; // File sudah ada
        }
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_used) {
            str_copy(file_table[i].name, name, MAX_FILENAME);
            file_table[i].content = NULL;
            file_table[i].size = 0;
            file_table[i].is_used = 1;
            return 0; // Sukses
        }
    }
    return -2; // Disk penuh
}

int vfs_write(const char *name, const char *data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && str_cmp(file_table[i].name, name) == 0) {
            if (file_table[i].content) {
                kfree(file_table[i].content);
            }
            size_t len = str_len(data);
            file_table[i].content = (char *)kmalloc(len + 1);
            if (!file_table[i].content) return -3; // Out of memory

            str_copy(file_table[i].content, data, len + 1);
            file_table[i].size = len;
            return 0;
        }
    }
    return -1; // File tidak ditemukan
}

char *vfs_read(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && str_cmp(file_table[i].name, name) == 0) {
            return file_table[i].content;
        }
    }
    return NULL;
}

int vfs_delete(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && str_cmp(file_table[i].name, name) == 0) {
            if (file_table[i].content) {
                kfree(file_table[i].content);
            }
            file_table[i].is_used = 0;
            file_table[i].content = NULL;
            file_table[i].size = 0;
            file_table[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}

void vfs_list(void) {
    kprintf("Daftar File (RAMDisk):\n");
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used) {
            kprintf(" - %s [Size: %u bytes]\n", file_table[i].name, (unsigned int)file_table[i].size);
            count++;
        }
    }
    if (count == 0) {
        kprintf(" (tidak ada berkas)\n");
    }
}

void vfs_run_test(void) {
    kprintf("\n[TEST] Memulai Pengujian Modul VFS & Memory...\n");
    
    // 1. Create Test
    if (vfs_create("test.txt") == 0) {
        kprintf(" [PASS] vfs_create('test.txt')\n");
    } else {
        kprintf(" [FAIL] vfs_create\n");
    }

    // 2. Write Test
    if (vfs_write("test.txt", "HexaOS RAMDisk Ready!") == 0) {
        kprintf(" [PASS] vfs_write\n");
    } else {
        kprintf(" [FAIL] vfs_write\n");
    }

    // 3. Read Test
    char *res = vfs_read("test.txt");
    if (res && str_cmp(res, "HexaOS RAMDisk Ready!") == 0) {
        kprintf(" [PASS] vfs_read: %s\n", res);
    } else {
        kprintf(" [FAIL] vfs_read\n");
    }

    // 4. Delete Test
    if (vfs_delete("test.txt") == 0) {
        kprintf(" [PASS] vfs_delete\n");
    } else {
        kprintf(" [FAIL] vfs_delete\n");
    }

    kprintf("[TEST] Pengujian selesai.\n\n");
}