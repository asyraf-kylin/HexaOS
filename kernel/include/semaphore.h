#ifndef SEMAPHOPRE_H
#define SEMAPHORE_H

#include "task.h"
#include "wait_queue.h"

typedef struct {
    int             count;
    thread_queue_t  wait_queue;
} semaphore_t;

void semaphore_init(semaphore_t* s, int value);
void sempahore_wait(semaphore_t* s);
void semaphore_signal(semaphore_t* s);

void semaphore_modul_init(void);

#endif