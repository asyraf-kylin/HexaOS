#include "kernel.h"
#include "sync.h"
#include "mutex.h"
#include "semaphore.h"
#include "cli.h"

/* Wrapper CLI untuk skenario pengujian deadlock dan proteksi */
static void cmd_demo_deadlock(int argc, char **argv) {
    (void)argc; (void)argv;
    test_deadlock();
}

static void cmd_demo_protect(int argc, char **argv) {
    (void)argc; (void)argv;
    test_protect();
}

void sync_init(void) {
    mutex_modul_init();
    semaphore_modul_init();
    cli_register("demo-deadlock", "demo deadlock scenario & recovery", cmd_demo_deadlock);
    cli_register("demo-protect",  "demo user vs kernel mode protection", cmd_demo_protect);
    kprintf("[ OK ] Sync primitives: mutex & semaphore\n");
}
