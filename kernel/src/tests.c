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

/* ADAPTER: sesuaikan dengan thread_create milik HexaOS.
 * Diasumsikan: membuat thread baru berstatus Ready yang menjalankan fn(arg). */
extern int32_t thread_create(void (*fn)(void *), void *arg, const char *name);

static volatile int  done_count;
static void wait_all(int n) { while (done_count < n) thread_yield(); }
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
        thread_yield();
        mutex_unlock_ordered(first, second);
        kprintf("[%s] selesai tanpa deadlock\n", who);
    } else {
        mutex_lock(first);
        kprintf("[%s] dapat '%s', menunggu '%s'...\n", who, first->name, second->name);
        thread_yield();                  /* beri kesempatan thread lain mengunci mutex kedua */
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
    thread_create(dl_t1, 0, "dl-T1");
    thread_create(dl_t2, 0, "dl-T2");
    wait_all(2);

    mutex_init(&lock_a, "A");
    mutex_init(&lock_b, "B");

    kprintf("== Skenario 2: solusi lock ordering ==\n");
    fix_mode = 1; done_count = 0;
    thread_create(dl_t1, 0, "dl-T1");
    thread_create(dl_t2, 0, "dl-T2");
    wait_all(2);
}

/* ===================== (d) Proteksi akses ========================= */
static char user_msg[] = "Halo dari user mode\n";
static volatile int prot_done;

static void user_thread(void *arg)
{
    (void)arg;
    int32_t me = thread_current_id();
    protect_set_priv(me, PRIV_USER);

    kprintf("[user] pid = %d\n", protect_syscall(SYS_GETPID, 0, 0));

    /* 1. Buffer valid milik user -> diizinkan */
    protect_syscall(SYS_WRITE, (uint32_t)user_msg, sizeof(user_msg) - 1);

    /* 2. Buffer menunjuk memori kernel (di luar halaman user-data) -> harus ditolak */
    int32_t r = protect_syscall(SYS_WRITE, ((uint32_t)user_msg & ~0xFFFu) + 0x2000, 16);
    kprintf("[user] SYS_WRITE ke memori kernel -> %d (%s)\n", r, r == E_FAULT ? "DITOLAK, benar" : "SALAH");

    /* 3. Operasi khusus kernel -> harus ditolak */
    r = protect_syscall(SYS_REBOOT, 0, 0);
    kprintf("[user] SYS_REBOOT -> %d (%s)\n", r, r == E_PERM ? "DITOLAK, benar" : "SALAH");

    prot_done = 1;
}

void test_protect(void)
{
    protect_init();
    /* Sesuaikan alamat region dengan linker script HexaOS */
    protect_add_region(0x00100000, 0x00400000, PERM_R | PERM_W | PERM_X,             "kernel");
    protect_add_region((uint32_t)user_msg & ~0xFFFu, 0x1000, PERM_R | PERM_W | PERM_USER, "user-data");

    kprintf("== Tes proteksi user/kernel ==\n");
    kprintf("[kernel] SYS_REBOOT oleh kernel -> %d\n", protect_syscall(SYS_REBOOT, 0, 0));

    prot_done = 0;
    thread_create(user_thread, 0, "user-demo");
    while (!prot_done) thread_yield();
}