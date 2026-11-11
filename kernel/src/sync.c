/*
 * sync.c - Sinkronisasi dan Mutex HexaOS
 */
#include "kernel.h"
#include "task.h"
#include "sync.h"
#include "cli.h"

/* Inisialisasi Mutex */
void mutex_init(mutex_t *m, const char *name) {
    if (!m) return;
    m->name = name;
    m->locked = 0;
    m->owner = -1;
}

/* Mengunci mutex (menunggu dengan kooperatif yield sampai terbuka) */
void mutex_lock(mutex_t *m) {
    if (!m) return;
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        task_yield();
    }
    m->owner = thread_current_id();
}

/* Mengunci mutex dengan batas waktu (ticks) untuk mendeteksi deadlock */
int mutex_timedlock(mutex_t *m, uint32_t timeout_ticks) {
    if (!m) return 0;
    uint32_t start = timer_ticks();
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        if (timer_ticks() - start >= timeout_ticks) {
            return 0; /* Timeout: potensi deadlock terdeteksi */
        }
        task_yield();
    }
    m->owner = thread_current_id();
    return 1;
}

/* Membuka kunci mutex */
void mutex_unlock(mutex_t *m) {
    if (!m) return;
    m->owner = -1;
    __sync_lock_release(&m->locked);
}

/* Penguncian berurutan secara global berdasarkan alamat memori (mencegah circular wait) */
void mutex_lock_ordered(mutex_t *m1, mutex_t *m2) {
    if (m1 < m2) {
        mutex_lock(m1);
        mutex_lock(m2);
    } else {
        mutex_lock(m2);
        mutex_lock(m1);
    }
}

/* Melepas kunci berurutan */
void mutex_unlock_ordered(mutex_t *m1, mutex_t *m2) {
    mutex_unlock(m1);
    mutex_unlock(m2);
}

/* Adapter fungsi thread agar kompatibel dengan modul kernel HexaOS */
int32_t thread_create(void (*fn)(void *), void *arg, const char *name) {
    task_t *t = task_spawn(name, (task_entry_t)fn, arg);
    return t ? (int32_t)t->id : -1;
}

void thread_yield(void) {
    task_yield();
}

int32_t thread_current_id(void) {
    task_t *cur = task_current();
    return cur ? (int32_t)cur->id : 0;
}

/* Wrapper CLI untuk skenario pengujian deadlock dan proteksi */
static void cmd_demo_deadlock(int argc, char **argv) {
    (void)argc; (void)argv;
    test_deadlock();
}

static void cmd_demo_protect(int argc, char **argv) {
    (void)argc; (void)argv;
    test_protect();
}

void sync_init(void) {
    cli_register("demo-deadlock", "demo deadlock scenario & recovery", cmd_demo_deadlock);
    cli_register("demo-protect",  "demo user vs kernel mode protection", cmd_demo_protect);
}
