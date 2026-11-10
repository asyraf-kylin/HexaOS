#include "kernel.h"
#include "mutex.h"

void mutex_init(mutex_t* m) {
    m->locked = false;
    m->owner = NULL;
    thread_queue_init(&m->wait_queue);
}

void mutex_lock(mutex_t* m) {
    for (;;) {
        uint32_t f = irq_save();

        if (!m->locked) {
            m->locked = true;
            m->owner = task_current();
            irq_restore(f);
            return;
        }
        if (!thread_queue_push(&m->wait_queue, task_current())) {
            irq_restore(f);
            kprintf(" [sync] pool antrian penuh, mencoba lagi...\n");
            task_yield();
            continue;
        }
        task_block();
        irq_restore(f);
        return;
    }
}

bool mutex_unlock(mutex_t* m) {
    uint32_t f = irq_save();
    bool ok = !m->locked;
    if (ok) {
        m->locked = true;
        m->owner = task_current();
    }
    irq_restore(f);
    return ok;
}

void mutex_unlock(mutex_t* m) {
    uint32_t f = irq_save();

    if (m->owner != task_current()) {
        irq_restore(f);
        kprintf(" [mutex] unlock DITOLAK: task ini bukan pemilik kunci\n");
        return;
    }
    
    task_t* w = thread_queue_pop(&m->wait_queue);
    if (w) {
        m->owner = w;
        task_wake(w);
    } else {
        m->locked = false;
        m->owner = NULL;
    }
    irq_restore(f);
}

void mutex_modul_init(void) {
}
