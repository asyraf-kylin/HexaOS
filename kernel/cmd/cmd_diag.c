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
 *   diag mem      -> poin 6 : kmalloc sampai NULL lalu kfree semua
 *   diag protect  -> poin 8 : user/kernel + password, semua "SESUAI"
 *
 * Semua entri memanggil fungsi kernel asli di tests.c / task.c / memory.c.
 * [JANGAN-HAPUS] File ini kalau Bab III butuh bukti poin 3, 4, 5, 6, 8.
 * [UBAH] Kalau ingin kiosk terlihat "murni aplikasi", perintah ini bisa
 *   disembunyikan dari help (cli_register_root -> tidak dipanggil di
 *   cli_init), tetapi jangan dihapus fungsi test_*-nya.
 * [HAPUS] Cara aman membuang seluruh diag: hapus file ini, hapus
 *   cmd_diag_init() di cli.c, dan hapus prototipe test_* di svc.h.
 * ===================================================================== */

typedef struct { const char* name; const char* desc; void (*fn)(void); } diag_entry_t;

static const diag_entry_t diag_table[] = {
    { "threads",  "siklus status thread NEW/READY/RUNNING/WAITING/TERMINATED", test_threads  },
    { "preempt",  "bukti preemption Round Robin (tanpa yield)",                test_preempt  },
    { "race",     "race condition: shared counter tanpa vs dengan mutex",     test_race     },
    { "deadlock", "deadlock 2 mutex + solusi lock ordering",                  test_deadlock },
    { "mem",      "kmalloc sampai Out-of-Memory lalu kfree semuanya",         test_mem      },
    { "protect",  "uji user mode vs kernel mode",                             test_protect  },
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