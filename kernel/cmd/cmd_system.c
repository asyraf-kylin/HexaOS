/* cmd_system.c - perintah sistem: clear, reboot, shutdown, int3 */
#include "cli.h"
#include "kernel.h"
#include "protect.h"

static void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!cli_is_root()) {
        kprintf("reboot: Akses ditolak. Harus dalam mode root.\n");
        kprintf("        Ketik 'su' untuk masuk ke mode root.\n");
        return;
    }
    kprintf("Rebooting...\n");
    kdelay_ms(300);
    system_reboot();
}

static void cmd_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!cli_is_root()) {
        kprintf("shutdown: Akses ditolak. Harus dalam mode root.\n");
        kprintf("          Ketik 'su' untuk masuk ke mode root.\n");
        return;
    }
    kprintf("Shutting down...\n");
    kdelay_ms(300);
    system_shutdown();
}

static void cmd_int3(int argc, char** argv) {
    (void)argc; (void)argv;
    __asm__ volatile("int3");
}

void cmd_system_init(void) {
    cli_register("clear",    "clear the screen",              cmd_clear);
    cli_register("reboot",   "reboot the system",             cmd_reboot);
    cli_register("shutdown", "shut down the system",          cmd_shutdown);
    cli_register("int3",     "test interrupt (breakpoint)",   cmd_int3);
}