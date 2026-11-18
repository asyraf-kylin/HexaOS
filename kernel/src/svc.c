/* svc.c - layanan latar belakang HexaOS (Anggota 2).
   Thread summary dan purge menulis ke ring buffer log, bukan kprintf, supaya
   tidak merusak teks yang sedang diketik pengguna. Log dibaca lewat sysinfo. */
#include "kernel.h"
#include "task.h"
#include "todo.h"
#include "svc.h"

/* =====================================================================
 * PENANDA DEMO BAB III - PDF poin 2, 3, 4
 * ---------------------------------------------------------------------
 * Dua thread latar belakang sungguhan (task_spawn/task_create):
 *   summary : loop todo_recount() lalu task_sleep(1000) -> WAITING
 *   purge   : todo_purge_done(), task_sleep(500), task_exit()
 * Keduanya DILARANG kprintf (merusak teks yang sedang diketik). Output
 * ditulis ke ring buffer dan dibaca lewat `sysinfo` -> [Log sinkronisasi].
 *
 * [DEMO-POIN2] Jendela WAITING 500 ms di purge sengaja dibuat supaya
 *   `purge` lalu `sysinfo` berulang bisa menangkap NEW/READY/RUNNING/
 *   WAITING/TERMINATED (sesuai tabel bukti task board).
 * [DEMO-RR] Kolom CPU(tick)/GILIRAN summary bertambah walaupun layar diam.
 *   Itu context switch dari IRQ0, bukan printf.
 * [DEMO-RACE] Thread di sini MEMAKAI mutex lewat todo_recount(). Kalau
 *   ingin membuktikan race lewat alur normal, tambahkan writer TANPA mutex
 *   ke cached_active (lihat penanda [DEMO-RACE] di todo.c), bukan dengan
 *   mengubah logika recount di sini.
 * [JANGAN] hapus summary_start() di main.c kalau ingin thread ini ada.
 * ===================================================================== */

#define LOG_MAX 16
#define LOG_LEN 64

static char log_buf[LOG_MAX][LOG_LEN];
static int  log_head = 0;
static int  log_count = 0;

static int append_str(char* dst, int pos, const char* src, int max) {
    while (*src && pos < max - 1) dst[pos++] = *src++;
    return pos;
}

static int append_int(char* dst, int pos, int val, int max) {
    char digits[12];
    int n = 0;
    if (val == 0) {
        digits[n++] = '0';
    } else {
        while (val > 0 && n < 11) {
            digits[n++] = (char)('0' + val % 10);
            val /= 10;
        }
    }
    while (n > 0 && pos < max - 1) dst[pos++] = digits[--n];
    return pos;
}

static void log_push(const char* line) {
    uint32_t f = irq_save();
    int n = 0;
    while (line[n] && n < LOG_LEN - 1) {
        log_buf[log_head][n] = line[n];
        n++;
    }
    log_buf[log_head][n] = '\0';
    log_head = (log_head + 1) % LOG_MAX;
    if (log_count < LOG_MAX) log_count++;
    irq_restore(f);
}

/* Worker rekap: sekali sehitung ulang counter dari VFS, lalu tidur. */
static void summary_entry(void* arg) {
    (void)arg;
    for (;;) {
        int active = 0, done = 0;
        todo_recount(&active, &done);

        char line[LOG_LEN];
        int n = append_str(line, 0, "summary: aktif ", LOG_LEN);
        n = append_int(line, n, active, LOG_LEN);
        n = append_str(line, n, " / selesai ", LOG_LEN);
        n = append_int(line, n, done, LOG_LEN);
        line[n] = '\0';
        log_push(line);

        /* [DEMO-POIN2] 1000 ms = 100 tick. Selama ini state = WAITING.
           [DEMO-RR] Setelah bangun, summary dapat giliran via pick_next(). */
        task_sleep(1000);
    }
}

void summary_start(void) {
    task_spawn("summary", summary_entry, NULL);
}

/* Purge: hapus tugas selesai sekali jalan, beri jendela WAITING untuk sysinfo. */
static void purge_entry(void* arg) {
    (void)arg;
    int removed = todo_purge_done();

    char line[LOG_LEN];
    int n = append_str(line, 0, "purge: dihapus ", LOG_LEN);
    n = append_int(line, n, removed, LOG_LEN);
    line[n] = '\0';
    log_push(line);

    /* [DEMO-POIN2] Jendela 500 ms ini yang ditangkap `sysinfo` berulang
       setelah perintah `purge`. Setelah itu task_exit() -> TERMINATED. */
    task_sleep(500);
    task_exit();
}

int purge_start(void) {
    task_t* t = task_create("purge", purge_entry, NULL);
    if (!t) return -1;
    task_start(t);
    task_yield();   /* jalankan sekali sampai WAITING agar sysinfo sempat melihatnya */
    return 0;
}

void synclog_dump(void) {
    if (log_count == 0) {
        kprintf("  (belum ada aktivitas thread latar belakang)\n");
        return;
    }

    int start = (log_head - log_count + LOG_MAX) % LOG_MAX;
    for (int i = 0; i < log_count; i++) {
        int idx = (start + i) % LOG_MAX;
        char line[LOG_LEN];
        uint32_t f = irq_save();
        int n = 0;
        while (log_buf[idx][n] && n < LOG_LEN - 1) {
            line[n] = log_buf[idx][n];
            n++;
        }
        line[n] = '\0';
        irq_restore(f);
        kprintf("  %s\n", line);
    }
}
