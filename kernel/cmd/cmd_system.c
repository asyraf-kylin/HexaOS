#include "cli.h"
#include "kernel.h"
#include "memory.h"
#include "task.h"
#include "todo.h"
#include "svc.h"
#include "vfs.h"

static void cmd_clear(int argc, char** argv) { (void)argc; (void)argv; vga_clear(); }

static void cmd_sysinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    int active, done;
    todo_get_counts(&active, &done);
    kprintf("=== SYSINFO ===\n");
    kprintf("Uptime : %u detik\n", (unsigned)(timer_ticks() / TIMER_HZ));
    kprintf("Heap   : kapasitas %u B, terpakai %u B, tersedia %u B\n",
            (unsigned)memory_get_capacity(),
            (unsigned)memory_get_used(), (unsigned)memory_get_free());
    kprintf("Tugas  : aktif %d, selesai %d\n", active, done);
    kprintf("\n[Thread]\n");
    task_dump();
    kprintf("\n[Log sinkronisasi]\n");
    synclog_dump();
}

/* [ADMIN] Ubah kapasitas heap saat runtime, selama masih dalam batas.
 * Guest tidak bisa memanggil ini: didaftarkan lewat cli_register_root(). */
static int parse_capacity(const char* s, size_t* out) {
    if (!s || s[0] == '\0') return -1;
    size_t v = 0;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10u + (size_t)(*p - '0');
        if (v > HEAP_MAX) return -1;
    }
    if (v == 0) return -1;
    *out = v;
    return 0;
}

static void cmd_setheap(int argc, char** argv) {
    if (argc != 2) {
        kprintf("Penggunaan: setheap <byte> (rentang %u - %u B)\n",
                (unsigned)HEAP_MIN, (unsigned)HEAP_MAX);
        return;
    }
    size_t wanted;
    if (parse_capacity(argv[1], &wanted) != 0) {
        kprintf("setheap: angka tidak valid.\n");
        return;
    }
    if (memory_set_capacity(wanted) != 0) {
        kprintf("setheap: gagal. Harus %u - %u B dan tidak boleh memotong blok terpakai.\n",
                (unsigned)HEAP_MIN, (unsigned)HEAP_MAX);
        return;
    }
    kprintf("Heap sekarang %u B (terpakai %u B, tersedia %u B).\n",
            (unsigned)memory_get_capacity(),
            (unsigned)memory_get_used(), (unsigned)memory_get_free());
}

/* [ADMIN] Tampilkan tabel RAMDisk mentah (bukan tampilan todo).
   Dipakai sebagai bukti langsung operasi VFS di Bab III poin 7. */
static void cmd_vfsdump(int argc, char** argv) {
    (void)argc; (void)argv;
    vfs_list();
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
    cli_register_root("setheap",  "ubah kapasitas heap: setheap <byte>", cmd_setheap, 1);
    cli_register_root("vfsdump",  "tampilkan tabel berkas RAMDisk",       cmd_vfsdump, 0);
    cli_register_root("reboot",   "restart sistem",              cmd_reboot,   0);
    cli_register_root("shutdown", "matikan sistem",              cmd_shutdown, 0);
}