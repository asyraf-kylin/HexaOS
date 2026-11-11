#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>
#include "task.h"
#include "wait_queue.h"

typedef struct {
    bool            locked;
    task_t*         owner;
    thread_queue_t  wait_queue;
} mutex_t;

void mutex_init(mutex_t* m);
void mutex_lock(mutex_t* m);
void mutex_unlock(mutex_t* m);
bool mutex_trylock(mutex_t* m);

void mutex_modul_init(void);

#endif