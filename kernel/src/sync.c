#include "kernel.h"
#include "sync.h"
#include "mutex.h"
#include "semaphore.h"

void sync_init(void) {
    mutex_modul_init();
    semaphore_model_init();
    kprintf("[OK] Sync primitives: mutex & semaphore\n")
}
