#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "kernel.h"

#include <stdint.h>
#include <stddef.h>
#include "../include/memory.h"
#include "memory.h"
#include "kernel.h"

#ifndef HEAP_DEFAULT
#define HEAP_DEFAULT 1024u
#endif

#ifndef HEAP_MIN
#define HEAP_MIN     256u
#endif

#ifndef HEAP_MAX
#define HEAP_MAX     (1024u * 1024u)
#endif

typedef struct block_meta {
    size_t size;        /* payload yang dialokasikan (sudah align-4) */
    struct block_meta *next;
    int    free;
    size_t requested;   /* byte yang benar-benar diminta kmalloc() */
} block_meta_t;

#define META_SIZE sizeof(block_meta_t)

static uint8_t heap_memory[HEAP_MAX] __attribute__((aligned(4)));
static block_meta_t *heap_start = NULL;
static size_t heap_capacity = HEAP_DEFAULT;

/* Pelacak byte payload yang diminta (1 char = 1 byte) */
static size_t tracked_payload_used = 0;

void memory_init(void) {
    heap_capacity = HEAP_DEFAULT;
    heap_start = (block_meta_t *)heap_memory;
    heap_start->size = heap_capacity - META_SIZE;
    heap_start->next = NULL;
    heap_start->free = 1;
    tracked_payload_used = 0;
    kprintf("[ OK ] Memory manager initialized (%u B default, maks %u B)\n",
            (unsigned)HEAP_DEFAULT, (unsigned)HEAP_MAX);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t alloc_size = (size + 3) & ~3;

    block_meta_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= alloc_size) {
            if (current->size >= alloc_size + META_SIZE + 4) {
                block_meta_t *new_block = (block_meta_t *)((uint8_t *)current + META_SIZE + alloc_size);
                new_block->size = current->size - alloc_size - META_SIZE;
                new_block->free = 1;
                new_block->next = current->next;

                current->size = alloc_size;
                current->next = new_block;
            }
            current->free = 0;
            current->requested = size;   /* dicatat agar kfree mengurangi angka yang sama */
            tracked_payload_used += size;
            return (void *)((uint8_t *)current + META_SIZE);
        }
        current = current->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_meta_t *block = (block_meta_t *)((uint8_t *)ptr - META_SIZE);
    block->free = 1;

    if (tracked_payload_used >= block->requested) {
        tracked_payload_used -= block->requested;
    } else {
        tracked_payload_used = 0;
    }

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
    if (tracked_payload_used >= heap_capacity) return 0;
    return heap_capacity - tracked_payload_used;
}

size_t memory_get_used(void) {
    return tracked_payload_used;
}

size_t memory_get_capacity(void) {
    return heap_capacity;
}

int memory_set_capacity(size_t new_capacity) {
    if (new_capacity < HEAP_MIN || new_capacity > HEAP_MAX) return -1;
    new_capacity = (new_capacity + 3u) & ~(size_t)3u;
    if (new_capacity == heap_capacity) return 0;

    if (new_capacity > heap_capacity) {
        block_meta_t *last = heap_start;
        if (!last) return -1;
        while (last->next) last = last->next;

        size_t delta = new_capacity - heap_capacity;
        if (last->free) {
            last->size += delta;
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
            drop = 1;
        } else if (cur->free && end > new_capacity) {
            size_t room = new_capacity - start;
            if (room <= META_SIZE) {
                drop = 1;
            } else {
                cur->size = room - META_SIZE;
            }
        } else if (start >= new_capacity) {
            return -1;
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