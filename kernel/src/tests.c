/*
 * tests.c - Skenario pengujian Anggota 3 (bagian c dan d)
 *   demo-deadlock : deadlock 2 thread + 2 mutex, lalu solusinya
 *   demo-protect  : user mode vs kernel mode
 *
 * Daftarkan dua fungsi test_* di bawah ke tabel perintah shell.
 */
#include "kernel.h"
#include "sync.h"
#include "protect.h"
#include "task.h"

    /* Adapter ke API thread HexaOS (task.h) */
static int32_t cur_id(void)
{
    task_t *t = task_current();
    return t ? (int32_t)t->id : -1;
}
static void spawn(void (*fn)(void *), const char *name) { task_spawn(name, fn, 0); }

static volatile int  done_count;
static void wait_all(int n) { while (done_count < n) task_yield(); }
static void mark_done(void) { __sync_fetch_and_add(&done_count, 1); }

/* ======================== (c) Deadlock ============================ */
static mutex_t lock_a, lock_b;
static int     fix_mode;                 /* 0 = deadlock, 1 = lock ordering */
#define DEADLOCK_TIMEOUT_TICKS 50        /* sesuaikan dengan frekuensi PIT */

static void dl_thread(mutex_t *first, mutex_t *second, const char *who)
{
    if (fix_mode) {
        /* SOLUSI: kedua thread mengambil lock dengan urutan global yang sama */
        mutex_lock_ordered(first, second);
        kprintf("[%s] memegang A dan B (urutan terurut), bekerja...\n", who);
        task_yield();
        mutex_unlock_ordered(first, second);
        kprintf("[%s] selesai tanpa deadlock\n", who);
    } else {
        mutex_lock(first);
        kprintf("[%s] dapat '%s', menunggu '%s'...\n", who, first->name, second->name);
        task_yield();                  /* beri kesempatan thread lain mengunci mutex kedua */
        if (!mutex_timedlock(second, DEADLOCK_TIMEOUT_TICKS)) {
            /* DETEKSI + PEMULIHAN: timeout -> lepas lock yang dipegang */
            kprintf("[%s] DEADLOCK TERDETEKSI (timeout) -> melepas '%s'\n", who, first->name);
            mutex_unlock(first);
        } else {
            kprintf("[%s] dapat kedua lock\n", who);
            mutex_unlock(second);
            mutex_unlock(first);
        }
    }
    mark_done();
}

static void dl_t1(void *arg) { (void)arg; dl_thread(&lock_a, &lock_b, "T1"); }  /* A lalu B */
static void dl_t2(void *arg) { (void)arg; dl_thread(&lock_b, &lock_a, "T2"); }  /* B lalu A */

void test_deadlock(void)
{
    mutex_init(&lock_a, "A");
    mutex_init(&lock_b, "B");

    kprintf("== Skenario 1: deadlock (T1: A->B, T2: B->A) + deteksi timeout ==\n");
    fix_mode = 0; done_count = 0;
    spawn(dl_t1, "dl-T1");
    spawn(dl_t2, "dl-T2");
    wait_all(2);

    mutex_init(&lock_a, "A");
    mutex_init(&lock_b, "B");

    kprintf("== Skenario 2: solusi lock ordering ==\n");
    fix_mode = 1; done_count = 0;
    spawn(dl_t1, "dl-T1");
    spawn(dl_t2, "dl-T2");
    wait_all(2);
}

    /* ===================== (d) Proteksi akses ========================= */
    /* Semua data milik "program user" dikumpulkan di satu struct supaya region
     * user-data tepat menutupi data ini saja (selain itu = memori kernel). */
static struct {
    char msg[32];      /* pesan yang dicetak lewat SYS_WRITE */
    char pw_salah[16];
    char pw_awal[16];  /* password awal root */
    char pw_baru[16];
} udata = { "Halo dari user mode\n", "asal", "root", "hexa123" };

static volatile int prot_done;
static int kernel_var;                    /* variabel milik kernel (bukan di region user) */

static void cek(const char *ket, int32_t hasil, int32_t harapan)
{
    kprintf("[user] %s -> %d (%s)\n", ket, hasil, hasil == harapan ? "SESUAI" : "SALAH");
}

static void user_thread(void *arg)
{
    (void)arg;
    protect_set_priv(cur_id(), PRIV_USER);

    kprintf("[user] pid = %d\n", protect_syscall(SYS_GETPID, 0, 0));

    /* --- User space vs kernel space --- */
    protect_syscall(SYS_WRITE, (uint32_t)udata.msg, 20);                       /* diizinkan */
    cek("SYS_WRITE ke memori kernel (harus ditolak)",
            protect_syscall(SYS_WRITE, (uint32_t)&kernel_var, 4), E_FAULT);
    cek("SYS_REBOOT sebagai user (harus ditolak)",
            protect_syscall(SYS_REBOOT, 0, 0), E_PERM);

        /* --- Mode root dengan password --- */
    cek("ganti password sebagai user (harus ditolak)",
        protect_syscall(SYS_SETPW, (uint32_t)udata.pw_awal, (uint32_t)udata.pw_baru), E_PERM);
    cek("masuk root, password salah (harus ditolak)",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_salah, 0), E_PERM);
    cek("masuk root, password benar",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_awal, 0), E_OK);
    cek("SYS_REBOOT sebagai root (diizinkan)",
        protect_syscall(SYS_REBOOT, 0, 0), E_OK);
    cek("ganti password sebagai root",
        protect_syscall(SYS_SETPW, (uint32_t)udata.pw_awal, (uint32_t)udata.pw_baru), E_OK);

        /* --- Kembali ke user, password lama sudah tidak berlaku --- */
    protect_syscall(SYS_DROP, 0, 0);
    cek("SYS_REBOOT setelah keluar dari root (harus ditolak)",
        protect_syscall(SYS_REBOOT, 0, 0), E_PERM);
    cek("masuk root pakai password LAMA (harus ditolak)",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_awal, 0), E_PERM);
    cek("masuk root pakai password BARU",
        protect_syscall(SYS_SU, (uint32_t)udata.pw_baru, 0), E_OK);
    protect_syscall(SYS_DROP, 0, 0);

    prot_done = 1;
}

void test_protect(void)
{
    protect_init();
    /* Sesuaikan alamat region kernel dengan linker script HexaOS (boot/linker.ld) */
    protect_add_region(0x00100000, 0x00400000, PERM_R | PERM_W | PERM_X, "kernel");
    protect_add_region((uint32_t)&udata, sizeof(udata), PERM_R | PERM_W | PERM_USER, "user-data");

    kprintf("== Tes proteksi user/kernel + mode root (password) ==\n");
    kprintf("[kernel] SYS_REBOOT oleh kernel -> %d\n", protect_syscall(SYS_REBOOT, 0, 0));

    prot_done = 0;
    spawn(user_thread, "user-demo");
    while (!prot_done) task_yield();
}
