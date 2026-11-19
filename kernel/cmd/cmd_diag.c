#include "cli.h"
#include "kernel.h"
#include "svc.h"

/* =====================================================================
 * PENANDA DEMO BAB III - tabel bukti per komponen PDF
 * ---------------------------------------------------------------------
 *   diag threads  -> poin 2 : NEW/READY/RUNNING/WAITING/TERMINATED
 *   diag preempt  -> poin 3 : A/B bergantian TANPA yield (RR nyata)
 *   diag race     -> poin 4 : tanpa mutex < 100000, dengan mutex = 100000
 *   diag deadlock -> poin 5 : blokir -> timeout -> lock ordering
 *
 * Poin 6 dan 8 tidak memakai entri `diag mem` / `diag protect` (sudah
 * dihapus). Bukti keduanya menyatu pada alur normal: `add`/`del` memakai
 * kmalloc/kfree dan melaporkan heap, sedangkan `login`/`logout` melakukan
 * transisi ring 0->3->#GP hardware->ring 0. Tanpa perintah demo tambahan.
 * Sub sistem memory/protect sendiri tetap utuh.
 *
 * Semua entri memanggil fungsi kernel asli di tests.c / task.c.
 * [UBAH] Kalau ingin kiosk terlihat "murni aplikasi", perintah ini bisa
 *   disembunyikan dari help (cli_register_root -> tidak dipanggil di
 *   cli_init), tetapi jangan dihapus fungsi test_*-nya.
 * ===================================================================== */

typedef struct { const char* name; const char* desc; void (*fn)(void); } diag_entry_t;

static const diag_entry_t diag_table[] = {
    { "threads",  "siklus status thread NEW/READY/RUNNING/WAITING/TERMINATED", test_threads  },
    { "preempt",  "bukti preemption Round Robin (tanpa yield)",                test_preempt  },
    { "race",     "race condition: shared counter tanpa vs dengan mutex",     test_race     },
    { "deadlock", "deadlock 2 mutex + solusi lock ordering",                  test_deadlock },
};
#define DIAG_COUNT ((int)(sizeof(diag_table) / sizeof(diag_table[0])))

static void diag_usage(void) {
    kprintf("Penggunaan: diag <nama>\n");
    for (int i = 0; i < DIAG_COUNT; i++) {
        kputs("  "); kputs_pad(diag_table[i].name, 10); kprintf("%s\n", diag_table[i].desc);
    }
}

static void cmd_diag(int argc, char** argv) {
    if (argc < 2) { diag_usage(); return; }
    for (int i = 0; i < DIAG_COUNT; i++)
        if (kstrcmp(argv[1], diag_table[i].name) == 0) { diag_table[i].fn(); return; }
    kprintf("diag: uji '%s' tidak dikenal.\n", argv[1]);
    diag_usage();
}

void cmd_diag_init(void) {
    cli_register_root("diag", "jalankan uji kernel: diag <nama>", cmd_diag, 1);
}