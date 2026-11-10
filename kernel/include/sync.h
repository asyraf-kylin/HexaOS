#ifndef SYNC_H
#define SYNC_H

#include "kernel.h"
#include "sync.h"
#include "mutex.h"
#include "semaphore.h"

void sync_init(void) {
    mutex_modul_init();
    semaphore_modul_init();
    kprintf(" [ OK ] Sync primitve: mutex & semaphore\n");
}

#endif