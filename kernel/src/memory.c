#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "kernel.h"

#define HEAP_SIZE 1024 // 1 KB Heap

/* =====================================================================
 * PENANDA DEMO BAB III - PDF poin 6 (alokator)
 * ---------------------------------------------------------------------
 * Dua cara menampilkan OOM, JANGAN disamakan:
 *  1) `add` terus sampai > 64 tugas -> VFS penuh. Pesan "Penyimpanan penuh!"
 *     muncul dari MAX_FILES (vfs.h), heap masih tersisa. Ini batas file.
 *  2) `diag mem` -> test_mem() di bawah mengalokasi blok 1 KB sampai
 *     kmalloc() benar-benar mengembalikan NULL (heap OOM), lalu kfree semua.
 *     Screenshot: Free saat OOM ~0, lalu kembali ke angka awal.
 * [DEMO] `sysinfo` menampilkan Heap terpakai/tersedia dari fungsi di bawah.
 * [JANGAN] mengubah urutan di vfs_write(): alokasi baru dulu, baru kfree lama.
 *          Kalau dibalik, OOM membuat isi file lama hilang.
 * [UBAH] HEAP_SIZE hanya kalau ingin batas OOM lebih cepat tercapai.
 * ===================================================================== */

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    int free;
} block_meta_t;

#define META_SIZE sizeof(block_meta_t)

static uint8_t heap_memory[HEAP_SIZE];
static block_meta_t *heap_start = NULL;

void memory_init(void) {
    heap_start = (block_meta_t *)heap_memory;
    heap_start->size = HEAP_SIZE - META_SIZE;
    heap_start->next = NULL;
    heap_start->free = 1;
    kprintf("[ OK ] Memory manager initialized (1 KB heap)\n");
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
    return HEAP_SIZE - memory_get_free();
}

/* ==========================================================
 * Pengujian Alokasi Memori / OOM (Anggota 4 - A4)
 * ========================================================== */
/* [DEMO-POIN6] Dipanggil `diag mem`. Bisa dihapus bersama entri "mem" di
   cmd_diag.c dan prototipe di svc.h kalau diag tidak dipakai. */
void test_mem(void) {
    kprintf("[MEM] Pengujian alokasi heap sampai OOM...\n");
    kprintf("[MEM] Titik Awal: Heap stabil (Free: %u bytes).\n", (unsigned int)memory_get_free());
    
    void *blocks[1024];
    int count = 0;
    
    // Alokasi blok 1 KB berulang kali sampai kmalloc mengembalikan NULL
    while (count < 1024) {
        void *ptr = kmalloc(1024);
        if (!ptr) {
            kprintf("[MEM] Titik OOM Tercapai: kmalloc mengembalikan NULL setelah %d blok.\n", count);
            break;
        }
        blocks[count++] = ptr;
    }
    
    kprintf("[MEM] Status saat OOM - Terpakai: %u bytes, Sisa: %u bytes.\n", 
            (unsigned int)memory_get_used(), (unsigned int)memory_get_free());

    kprintf("[MEM] Membebaskan kembali seluruh blok memori (kfree)...\n");
    for (int i = 0; i < count; i++) {
        kfree(blocks[i]);
    }
    
    kprintf("[MEM] Titik Selesai: Semua memori berhasil dipulihkan ke keadaan awal (Free: %u bytes).\n", 
            (unsigned int)memory_get_free());
}