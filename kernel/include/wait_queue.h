#ifndef WAIT_QUEUE_H
#define WAIT_QUEUE_H

#include "task.h"

typedef struct wait_node {
    task_t*              task;
    struct wait_node* next;
} wait_node_t;

typedef struct thread_queue {
    wait_node_t* head;
    wait_node_t* tail;
} thread_queue_t;

void    thread_queue_init(thread_queue_t* q);
int     thread_queue_push(thread_queue_t* q, task_t* t);
task_t* thread_queue_pop(thread_queue_t* q);

#endif