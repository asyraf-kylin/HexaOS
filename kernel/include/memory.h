#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Konfigurasi heap: guest selalu mulai di 1 KB, admin bisa setheap sampai 1 MB. */
#define HEAP_DEFAULT 1024u
#define HEAP_MIN     256u
#define HEAP_MAX     (1024u * 1024u)

void memory_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t memory_get_free(void);
size_t memory_get_used(void);
size_t memory_get_capacity(void);
size_t memory_block_cost(size_t data_len); /* META_SIZE + payload align-4 */
int  memory_set_capacity(size_t new_capacity); /* 0 = sukses, -1 = ditolak */

#endif