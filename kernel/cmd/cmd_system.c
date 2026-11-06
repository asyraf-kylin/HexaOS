/* cmd_system.c - perintah sistem: clear, reboot, shutdown, int3 */
#include "cli.h"
#include "kernel.h"

static void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("Rebooting...\n");
    kdelay_ms(300);
    system_reboot();
}

static void cmd_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
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