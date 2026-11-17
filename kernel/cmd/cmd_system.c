#include "cli.h"
#include "kernel.h"
#include "memory.h"
#include "task.h"
#include "todo.h"
#include "svc.h"

static void cmd_clear(int argc, char** argv) { (void)argc; (void)argv; vga_clear(); }

static void cmd_sysinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    int active, done;
    todo_get_counts(&active, &done);
    kprintf("=== SYSINFO ===\n");
    kprintf("Uptime : %u detik\n", (unsigned)(timer_ticks() / TIMER_HZ));
    kprintf("Heap   : terpakai %u B, tersedia %u B\n",
            (unsigned)memory_get_used(), (unsigned)memory_get_free());
    kprintf("Tugas  : aktif %d, selesai %d\n", active, done);
    kprintf("\n[Thread]\n");
    task_dump();
    kprintf("\n[Log sinkronisasi]\n");
    synclog_dump();
}

static void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("Rebooting...\n"); kdelay_ms(300); system_reboot();
}

static void cmd_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("Shutting down...\n"); kdelay_ms(300); system_shutdown();
}

static void cmd_hud(int argc, char** argv) {
    (void)argc; (void)argv;
    cli_hud_toggle();
    if (cli_hud_is_active()) {
        kprintf("Task Monitor HUD [AKTIF] di pojok kanan bawah layar.\n");
    } else {
        kprintf("Task Monitor HUD [NONAKTIF].\n");
    }
}

void cmd_system_init(void) {
    cli_register     ("clear",    "bersihkan layar",             cmd_clear);
    cli_register_root("hud",      "toggle task monitor HUD pojok kanan bawah (atau tombol Tab)", cmd_hud, 0);
    cli_register_root("sysinfo",  "status OS: RAM, thread, log", cmd_sysinfo,  0);
    cli_register_root("reboot",   "restart sistem",              cmd_reboot,   0);
    cli_register_root("shutdown", "matikan sistem",              cmd_shutdown, 0);
}