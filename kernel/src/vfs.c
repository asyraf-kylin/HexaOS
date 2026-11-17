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

/* Perbarui vfs_write di vfs.c agar aman dari OOM */
int vfs_write(const char *name, const char *data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used && str_cmp(file_table[i].name, name) == 0) {
            size_t len = str_len(data);

            // Alokasi blok memori baru terlebih dahulu (aman dari OOM)
            char *new_content = (char *)kmalloc(len + 1);
            if (!new_content) return -3; // Out of memory

            // Bebaskan konten lama jika ada
            if (file_table[i].content) {
                kfree(file_table[i].content);
            }

            file_table[i].content = new_content; // <-- Gunakan ini, bukan file_content
            str_copy(file_table[i].content, data, len + 1);
            file_table[i].size = len;
            return 0;
        }
    }
    return -1;
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

/* Tambahkan fungsi ini di vfs.c */
int vfs_name_at(int index, char *out, size_t max) {
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].is_used) {
            if (found == index) {
                str_copy(out, file_table[i].name, max);
                return 0; // Berhasil
            }
            found++;
        }
    }
    return -1; // Tidak ditemukan pada indeks tersebut
}