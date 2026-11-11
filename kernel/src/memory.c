#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "kernel.h"

#define HEAP_SIZE (1024 * 1024) // 1 MB Heap

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
    kprintf("[ OK ] Memory manager initialized (1 MB heap)\n");
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