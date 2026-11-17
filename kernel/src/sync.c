#include "kernel.h"
#include "sync.h"
#include "mutex.h"
#include "semaphore.h"
#include "cli.h"

void sync_init(void) {
    mutex_modul_init();
    semaphore_modul_init();
    kprintf("[ OK ] Sync primitives: mutex & semaphore\n");
}