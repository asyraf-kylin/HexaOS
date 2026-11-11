#include "kernel.h"
#include "semaphore.h"

void semaphore_wait(semaphore_t* s) {
    for (;;) {
        uint32_t f = irq_save();
        s->count--;
        if (s->count >= 0) {
            irq_restore(f);
            return;
        }
        if (!thread_queue_push(&s->wait_queue, task_current())) {
            s->count++;
            irq_restore(f);
            kprintf(" [sync] pool antrian penuh, mencoba lagi...\n");
            continue;
        }
        task_block();
        irq_restore(f);
        return;
    }
}

void sempahore_signal(semaphore_t* s) {
    uint32_t f = irq_save();
    s->count++;
    if (s->count <= 0) {
        task_t* w = thread_queue_pop(&s->wait_queue);
        if (w) task_wake(w);
    }
    irq_restore(f);
}

void semaphore_modul_init(void) {
}