#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include <stdint.h>
#include "task.h"
#include "wait_queue.h"

typedef struct {
    const char*     name;
    bool            locked;
    task_t*         owner;
    thread_queue_t  wait_queue;
} mutex_t;

void mutex_init(mutex_t* m, const char* name);
void mutex_lock(mutex_t* m);
bool mutex_trylock(mutex_t* m);
int  mutex_timedlock(mutex_t* m, uint32_t timeout_ticks);
void mutex_unlock(mutex_t* m);
void mutex_lock_ordered(mutex_t* m1, mutex_t* m2);
void mutex_unlock_ordered(mutex_t* m1, mutex_t* m2);

void mutex_modul_init(void);

#endif
