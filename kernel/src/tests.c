
/*
 * tests.c - Skenario pengujian Anggota 3 (bagian c dan d)
 *
 * demo-deadlock : Deadlock 2 thread + 2 mutex, kemudian solusinya
 * demo-protect  : User mode vs kernel mode
 * test_race     : Race condition tanpa mutex vs dengan mutex
 *
 * Daftarkan tiga fungsi test_* di bawah ke tabel perintah shell.
 */

/* =====================================================================
 * PENANDA DEMO BAB II/III - PDF poin 4 (sinkronisasi), 5 (deadlock), 8
 * ---------------------------------------------------------------------
 * Fungsi test_* di sini MEMANGGIL KERNEL ASLI: task_spawn, mutex_lock,
 * timer, protect_syscall. Bukan printf yang berpura-pura ada thread.
 * Dipanggil dari `diag <nama>` (lihat kernel/cmd/cmd_diag.c).
 *
 * [JANGAN-HAPUS] cmd_diag.c + file ini kalau butuh bukti PDF poin 4 & 5.
 *   PDF mewajibkan perbandingan "tanpa mutex vs dengan mutex" dan
 *   "deadlock lalu diselesaikan". Itu tidak bisa dibuktikan hanya dengan
 *   alur add/list/done yang memang sudah benar.
 *
 * RACE: alur normal kiosk TIDAK akan menunjukkan race. Semua counter
 *   dilindungi mutex dan todo_recount() menghitung ulang dari VFS.
 *   [UBAH] lihat penanda [DEMO-RACE] di todo.c / svc.c bila ingin memaksa
 *   race muncul lewat perintah biasa (add/done berulang).
 *
 * DEADLOCK: sengaja dibuat di sini, dengan timeout 50 tick (500 ms)
 *   supaya demo tidak menggantung kiosk. [JANGAN] memindahkan urutan
 *   lock terbalik ke todo.c untuk pemakaian sehari-hari: kiosk akan hang
 *   dan perlu reboot (lihat penanda [DEMO-DEADLOCK] di todo.c).
 *
 * PROTECT: protect_test_begin()/protect_test_end() wajib berpasangan
 *   agar password dan privilege admin tidak berubah setelah diag protect.
 *   [JANGAN] memanggil protect_init() di sini.
 * ===================================================================== */

#include "kernel.h"
#include "sync.h"
#include "protect.h"
#include "task.h"

/* ============================================================
 * Adapter ke API thread HexaOS (task.h)
 * ============================================================ */

static int32_t cur_id(void)
{
    task_t *t = task_current();
    return t ? (int32_t)t->id : -1;
}

static void spawn(void (*fn)(void *), const char *name)
{
    task_spawn(name, fn, 0);
}

/* ============================================================
 * Variabel dan fungsi bantuan
 * ============================================================ */

static volatile int done_count;

static void wait_all(int n)
{
    while (done_count < n) {
        task_yield();
    }
}

static void mark_done(void)
{
    __sync_fetch_and_add(&done_count, 1);
}

/* ============================================================
 * (c) Pengujian Deadlock
 * ============================================================ */

static mutex_t lock_a;
static mutex_t lock_b;

static int fix_mode;  /* 0 = deadlock, 1 = lock ordering */

#define DEADLOCK_TIMEOUT_TICKS 50  /* 50 tick x 10 ms = 500 ms */
/* [DEMO-DEADLOCK] Timeout ini adalah "deteksi": mutex_timedlock() benar-benar
   menunggu sampai habis. Tanpa timeout, QEMU hang permanen saat skenario 1. */

static void dl_thread(mutex_t *first, mutex_t *second, const char *who)
{
    if (fix_mode) {
        /* Solusi: kedua thread mengambil lock dengan urutan global yang sama */
        mutex_lock_ordered(first, second);

        kprintf("[%s] memegang A dan B (urutan terurut), bekerja...\n", who);

        task_yield();

        mutex_unlock_ordered(first, second);

        kprintf("[%s] selesai tanpa deadlock\n", who);
    } else {
        mutex_lock(first);

        kprintf("[%s] dapat '%s', menunggu '%s'...\n",
                who, first->name, second->name);

        task_yield();  /* [DEMO-DEADLOCK] T1 pegang A, T2 pegang B; yielded dulu
                          supaya circular wait benar-benar terjadi. */

        if (!mutex_timedlock(second, DEADLOCK_TIMEOUT_TICKS)) {
            /* Deteksi dan pemulihan: timeout -> lepas lock yang dipegang */
            kprintf("[%s] DEADLOCK TERDETEKSI (timeout) -> melepas '%s'\n",
                    who, first->name);

            mutex_unlock(first);
        } else {
            kprintf("[%s] dapat kedua lock\n", who);

            mutex_unlock(second);
            mutex_unlock(first);
        }
    }

    mark_done();
}

static void dl_t1(void *arg)
{
    (void)arg;
    dl_thread(&lock_a, &lock_b, "T1");  /* A lalu B */
}

static void dl_t2(void *arg)
{
    (void)arg;
    dl_thread(&lock_b, &lock_a, "T2");  /* B lalu A */
}

void test_deadlock(void)
{
    mutex_init(&lock_a, "A");
    mutex_init(&lock_b, "B");

    /* [DEMO-DEADLOCK] Skenario 1 = bug sengaja. Screenshot di sini:
       kedua thread terblokir, lalu salah satu melaporkan timeout. */
    kprintf("== Skenario 1: deadlock (T1: A->B, T2: B->A) + deteksi timeout ==\n");

    fix_mode = 0;
    done_count = 0;

    spawn(dl_t1, "dl-T1");
    spawn(dl_t2, "dl-T2");

    wait_all(2);

    mutex_init(&lock_a, "A");
    mutex_init(&lock_b, "B");

    /* [DEMO-DEADLOCK] Skenario 2 = solusi: mutex_lock_ordered() memakai
       urutan alamat mutex yang sama untuk kedua thread (menghilangkan
       circular wait). Screenshot di sini sebagai "penanganan". */
    kprintf("== Skenario 2: solusi lock ordering ==\n");

    fix_mode = 1;
    done_count = 0;

    spawn(dl_t1, "dl-T1");
    spawn(dl_t2, "dl-T2");

    wait_all(2);
}

/* ============================================================
 * (e) Pengujian Race Condition
 * ============================================================ */

#define RACE_ITER 50000

static volatile int race_counter;
static mutex_t race_lock;

static int race_use_mutex;  /* 0 = tanpa mutex, 1 = dengan mutex */

static void race_worker(void *arg)
{
    (void)arg;

    for (int i = 0; i < RACE_ITER; i++) {
        if (race_use_mutex) {
            mutex_lock(&race_lock);
        }

        int tmp = race_counter;  /* Baca */

        /* [DEMO-RACE] Jangan dihapus saat uji tanpa mutex.
           Dengan -O2, `int tmp = x; x = tmp + 1;` bisa jadi satu instruksi
           dan CPU tunggal tidak bisa menyelanya -> race tidak pernah muncul.
           task_yield() di sini membuka jendela preemption di antara baca
           dan tulis, supaya "update hilang" benar-benar terlihat. */
        if (i % 1000 == 0) {
            task_yield();  /* Memberi kesempatan thread lain menyela */
        }

        race_counter = tmp + 1;  /* Tulis */

        if (race_use_mutex) {
            mutex_unlock(&race_lock);
        }
    }

    mark_done();
}

static void race_jalankan(int pakai_mutex, const char *label)
{
    race_use_mutex = pakai_mutex;
    race_counter = 0;
    done_count = 0;

    spawn(race_worker, "race-1");
    spawn(race_worker, "race-2");

    wait_all(2);

    kprintf("%s: %d (harapan %d) -> %s\n",
            label,
            race_counter,
            2 * RACE_ITER,
            race_counter == 2 * RACE_ITER
                ? "BENAR"
                : "SALAH (ada update hilang)");
}

/* [DEMO-POIN4] Urutan cetak yang harus masuk ke Bab III:
   "Tanpa mutex: < 100000 -> SALAH (ada update hilang)"
   "Dengan mutex: 100000 -> BENAR"
   Itu perbandingan yang diminta PDF, bukan sekadar angka akhir. */
void test_race(void)
{
    mutex_init(&race_lock, "race");

    kprintf("== Uji race condition: 2 thread x %d kenaikan ==\n",
            RACE_ITER);

    race_jalankan(0, "Tanpa mutex");
    race_jalankan(1, "Dengan mutex");
}

/* ============================================================
 * (d) Pengujian Proteksi Akses
 * ============================================================ */

/*
 * Semua data milik program user dikumpulkan dalam satu struct
 * agar region user-data hanya mencakup data tersebut.
 * Selain itu, memori termasuk ke dalam wilayah kernel.
 */

static struct {
    char msg[32];
    char pw_salah[16];
    char pw_awal[16];
    char pw_baru[16];
} udata = {
    "Halo dari user mode\n",
    "asal",
    "root",
    "hexa123"
};

static volatile int prot_done;
static int kernel_var;  /* Variabel milik kernel, bukan di region user */

static void cek(const char *ket, int32_t hasil, int32_t harapan)
{
    kprintf("[user] %s -> %d (%s)\n",
            ket,
            hasil,
            hasil == harapan ? "SESUAI" : "SALAH");
}

static void user_thread(void *arg)
{
    (void)arg;

    protect_set_priv(cur_id(), PRIV_USER);

    kprintf("[user] pid = %d\n",
            protect_syscall(SYS_GETPID, 0, 0));

    /* --------------------------------------------------------
     * Pengujian User Space dan Kernel Space
     * -------------------------------------------------------- */

    /* Diizinkan: menulis dari alamat memori user */
    protect_syscall(SYS_WRITE, (uint32_t)udata.msg, 20);

    /* Ditolak: mengakses memori kernel */
    cek("SYS_WRITE ke memori kernel (harus ditolak)",
        protect_syscall(SYS_WRITE, (uint32_t)&kernel_var, 4),
        E_FAULT);

    /* Ditolak: reboot sebagai user */
    cek("SYS_REBOOT sebagai user (harus ditolak)",
        protect_syscall(SYS_REBOOT, 0, 0),
        E_PERM);

    /* --------------------------------------------------------
     * Pengujian Mode Root dengan Password
     * -------------------------------------------------------- */

    cek("Ganti password sebagai user (harus ditolak)",
        protect_syscall(SYS_SETPW,
                        (uint32_t)udata.pw_awal,
                        (uint32_t)udata.pw_baru),
        E_PERM);

    cek("Masuk root, password salah (harus ditolak)",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_salah, 0),
        E_PERM);

    cek("Masuk root, password benar",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_awal, 0),
        E_OK);

    cek("SYS_REBOOT sebagai root (diizinkan)",
        protect_syscall(SYS_REBOOT, 0, 0),
        E_OK);

    cek("Ganti password sebagai root",
        protect_syscall(SYS_SETPW,
                        (uint32_t)udata.pw_awal,
                        (uint32_t)udata.pw_baru),
        E_OK);

    /* --------------------------------------------------------
     * Kembali ke User Mode dan Menguji Password Baru
     * -------------------------------------------------------- */

    protect_syscall(SYS_DROP, 0, 0);

    cek("SYS_REBOOT setelah keluar dari root (harus ditolak)",
        protect_syscall(SYS_REBOOT, 0, 0),
        E_PERM);

    cek("Masuk root pakai password lama (harus ditolak)",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_awal, 0),
        E_PERM);

    cek("Masuk root pakai password baru",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_baru, 0),
        E_OK);

    protect_syscall(SYS_DROP, 0, 0);

    prot_done = 1;
}

/* [DEMO-POIN8] Semua baris harus berakhir "SESUAI". Setelah selesai,
   peran shell dan password admin kembali seperti semula (protect_test_end).
   [JANGAN] mengganti protect_test_begin() dengan protect_init(). */
void test_protect(void)
{
    protect_test_begin();

    /*
     * Sesuaikan alamat region kernel dengan linker script
     * HexaOS (boot/linker.ld).
     */
    protect_add_region(
        0x00100000,
        0x00400000,
        PERM_R | PERM_W | PERM_X,
        "kernel"
    );

    protect_add_region(
        (uint32_t)&udata,
        sizeof(udata),
        PERM_R | PERM_W | PERM_USER,
        "user-data"
    );

    kprintf("== Tes proteksi user/kernel + mode root (password) ==\n");

    kprintf("[kernel] SYS_REBOOT oleh kernel -> %d\n",
            protect_syscall(SYS_REBOOT, 0, 0));

    prot_done = 0;

    spawn(user_thread, "user-demo");

    while (!prot_done) {
        task_yield();
    }

    protect_test_end();
}