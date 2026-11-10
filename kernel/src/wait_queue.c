#include "kernel.h"
#include "wait_queue.h"

#define WAIT_NODE_MAX 32

static wait_node_t pool[WAIT_NODE_MAX];
static wait_node_t* node_alloc(task_t* t) {
    for (int i = 0; i < WAIT_NODE_MAX; i++) {
        if (pool[i].task == NULL) {
            pool[i].task = t;
            pool[i].next = NULL;
            return &pool[i];
        }
    }
    return NULL;
}

void thread_queue_init(thread_queue_t* q) {
    q->head = NULL;
    q->tail = NULL;
}

int thread_queue_push(thread_queue_t* q, task_t* t) {
    wait_node_t* n = node_alloc(t);
    if (!n) return 0;

    if (q->tail) q->tail->next = n;
    else
    q->tail = n;
    return 1;
}

task_t* thread_queue_pop(thread_queue_t* q) {
    wait_node_t* n = q->head;

    while (n) {
        wait_node_t* next = n->next;

        if (n->task->state == TASK_TERMINATED || n->task->state == TASK_UNUSED) {
            n->task = NULL;
            n->next = NULL;
            q->head = next;
            if (!q->head) q->tail = NULL;
            n = next;
            continue;
        }
        q->head = next;
        if(!q->head) q->tail = NULL;
        task_t* t = n->task;
        n->task = NULL;
        n->next = NULL;
        return t;
    }
    q->tail = NULL;
    return NULL;
}