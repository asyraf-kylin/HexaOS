#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "kernel.h"

/* =====================================================================
 * PENANDA DEMO BAB III - PDF poin 6 (alokator)
 * ---------------------------------------------------------------------
 * Default kapasitas heap = HEAP_DEFAULT (1 KB), supaya alur kiosk normal
 * cepat menunjukkan OOM: `add` berulang -> vfs_write() -> kmalloc() NULL
 * -> TODO_ERR_OOM -> CLI "Penyimpanan penuh!". Batas MAX_FILES (64) belum
 * tercapai karena heap habis lebih dulu.
 *
 * Setelah `login admin`, kapasitas bisa diubah dengan `setheap <byte>`
 * (HEAP_MIN .. HEAP_MAX = 1 MB). Buffer statis disiapkan sebesar HEAP_MAX;
 * yang dipakai alokator cuma `heap_capacity` byte pertama. Menambah kapasitas
 * menambah blok bebas di ujung; mengecilkan hanya boleh kalau blok di ujung
 * memang kosong, jadi tidak ada alokasi yang terpotong.
 *
 * [JANGAN] mengubah urutan di vfs_write(): alokasi baru dulu, baru kfree lama.
 *          Kalau dibalik, OOM membuat isi file lama hilang.
 * ===================================================================== */

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    int free;
} block_meta_t;

#define META_SIZE sizeof(block_meta_t)

/* Buffer penuh disiapkan sekali; kapasitas aktif bisa diubah admin. */
static uint8_t heap_memory[HEAP_MAX] __attribute__((aligned(4)));
static block_meta_t *heap_start = NULL;
static size_t heap_capacity = HEAP_DEFAULT;

void memory_init(void) {
    heap_capacity = HEAP_DEFAULT;
    heap_start = (block_meta_t *)heap_memory;
    heap_start->size = heap_capacity - META_SIZE;
    heap_start->next = NULL;
    heap_start->free = 1;
    kprintf("[ OK ] Memory manager initialized (%u B default, maks %u B)\n",
            (unsigned)HEAP_DEFAULT, (unsigned)HEAP_MAX);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Alignment 4-byte
    size = (size + 3) & ~3;

    block_meta_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            // Split block jika tersisa ruang cukup
            if (current->size >= size + META_SIZE + 4) {
                block_meta_t *new_block = (block_meta_t *)((uint8_t *)current + META_SIZE + size);
                new_block->size = current->size - size - META_SIZE;
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }
            current->free = 0;
            return (void *)((uint8_t *)current + META_SIZE);
        }
        current = current->next;
    }
    return NULL; // Out of memory
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_meta_t *block = (block_meta_t *)((uint8_t *)ptr - META_SIZE);
    block->free = 1;

    // Coalesce (gabungkan blok kosong yang berurutan)
    block_meta_t *current = heap_start;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += META_SIZE + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

size_t memory_get_free(void) {
    size_t free_mem = 0;
    block_meta_t *curr = heap_start;
    while (curr) {
        if (curr->free) free_mem += curr->size;
        curr = curr->next;
    }
    return free_mem;
}

size_t memory_get_used(void) {
    return heap_capacity - memory_get_free();
}

size_t memory_get_capacity(void) {
    return heap_capacity;
}

/* Biaya satu blok hasil kmalloc(data_len): header + payload yang di-align.
   Dipakai vfsdump supaya jelas bedanya "ukuran file" vs "biaya heap". */
size_t memory_block_cost(size_t data_len) {
    size_t aligned = (data_len + 3u) & ~(size_t)3u;
    return META_SIZE + aligned;
}

/* Ubah kapasitas aktif heap. Aman untuk blok yang sudah dialokasikan:
 *  - membesar: perluas blok bebas terakhir, atau tambah blok bebas baru;
 *  - mengecil: tolak kalau ada blok terpakai yang akan terpotong. */
int memory_set_capacity(size_t new_capacity) {
    if (new_capacity < HEAP_MIN || new_capacity > HEAP_MAX) return -1;
    new_capacity = (new_capacity + 3u) & ~(size_t)3u; // alignment 4-byte
    if (new_capacity == heap_capacity) return 0;

    if (new_capacity > heap_capacity) {
        block_meta_t *last = heap_start;
        if (!last) return -1;
        while (last->next) last = last->next;

        size_t delta = new_capacity - heap_capacity;
        if (last->free) {
            last->size += delta;          // seluruh delta bisa dipakai
        } else {
            if (delta < META_SIZE + 4u) return -1;
            block_meta_t *added = (block_meta_t *)((uint8_t *)heap_memory + heap_capacity);
            added->size = delta - META_SIZE;
            added->next = NULL;
            added->free = 1;
            last->next = added;
        }
        heap_capacity = new_capacity;
        return 0;
    }

    /* Kecilkan: pastikan tidak ada blok terpakai di atas batas baru. */
    for (block_meta_t *b = heap_start; b; b = b->next) {
        if (b->free) continue;
        size_t start = (size_t)((uint8_t *)b - heap_memory);
        if (start + META_SIZE + b->size > new_capacity) return -1;
    }

    block_meta_t *prev = NULL;
    block_meta_t *cur = heap_start;
    while (cur) {
        block_meta_t *next = cur->next;
        size_t start = (size_t)((uint8_t *)cur - heap_memory);
        size_t end = start + META_SIZE + cur->size;
        int drop = 0;

        if (cur->free && start >= new_capacity) {
            drop = 1;                      // blok bebas seluruhnya di luar batas
        } else if (cur->free && end > new_capacity) {
            size_t room = new_capacity - start;
            if (room <= META_SIZE) {
                drop = 1;
            } else {
                cur->size = room - META_SIZE; // pangkas ekor blok bebas
            }
        } else if (start >= new_capacity) {
            return -1;                     // blok terpakai di luar batas
        }

        if (drop) {
            if (prev) prev->next = next;
            else      heap_start = next;
        } else {
            prev = cur;
        }
        cur = next;
    }

    heap_capacity = new_capacity;
    return 0;
}
