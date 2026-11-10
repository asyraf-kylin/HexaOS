#ifndef HEXAOS_SYNC_H
#define HEXAOS_SYNC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *name;
    volatile int locked;
    int32_t owner;
} mutex_t;

/* Mutex API */
void mutex_init(mutex_t *m, const char *name);
void mutex_lock(mutex_t *m);
int  mutex_timedlock(mutex_t *m, uint32_t timeout_ticks);
void mutex_unlock(mutex_t *m);
void mutex_lock_ordered(mutex_t *m1, mutex_t *m2);
void mutex_unlock_ordered(mutex_t *m1, mutex_t *m2);

/* Thread adapter API untuk kompatibilitas modul HexaOS */
int32_t thread_create(void (*fn)(void *), void *arg, const char *name);
void    thread_yield(void);
int32_t thread_current_id(void);

/* Fungsi pengujian / demo */
void test_deadlock(void);
void test_protect(void);

/* Inisialisasi modul sync */
void sync_init(void);

#endif
