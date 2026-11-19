
/*
 * protect.c - Proteksi hak akses HexaOS (pemisahan user mode & kernel mode)
 *
 * Level 1 (aktif default, bisa diuji langsung di QEMU):
 *   - Setiap thread punya privilege: PRIV_KERNEL atau PRIV_USER.
 *   - Tabel region memori dengan izin R/W/X dan flag PERM_USER.
 *   - Semua akses user ke memori / layanan kernel divalidasi lewat
 *     protect_check() dan protect_syscall().
 *
 * Level 2 (hardware x86 CPL): transisi ring 0 -> ring 3 -> #GP -> ring 0
 *   terjadi di arch/usermode.* yang dipanggil cmd_login()/cmd_logout().
 *
 * Pesan diagnostik `[protect]` ditulis ke buffer internal (kprintf_bg), bukan
 * ke layar maupun serial. Yang tampil hanya hasil untuk pengguna, misalnya
 * "Anda telah menjadi root." Bukti akses ditolak tetap terlihat lewat
 * "Akses Ditolak" di cli_run().
 *
 * [UBAH] DEFAULT_ROOT_PW "root" harus sama dengan password yang diketik
 *        pengguna saat `login admin`.
 * ===================================================================== */

#include "kernel.h"
#include "protect.h"
#include "task.h"
#include "kernel.h"

/* ID thread yang sedang berjalan */
static int32_t cur_id(void)
{
    task_t *t = task_current();
    return t ? (int32_t)t->id : -1;
}

#define MAX_REGIONS 16
#define MAX_THREADS 32

typedef struct {
    uint32_t    start;
    uint32_t    end;
    uint32_t    flags;
    const char *name;
    bool        used;
} mem_region_t;

#define PW_MAX          32
#define DEFAULT_ROOT_PW "root"

static uint32_t root_pw_hash;
static uint32_t hash_str(const char *s);
static mem_region_t regions[MAX_REGIONS];
static priv_t thread_priv[MAX_THREADS];

/* Menyimpan kondisi proteksi sebelum pengujian */
static mem_region_t saved_regions[MAX_REGIONS];
static priv_t saved_priv[MAX_THREADS];
static uint32_t saved_hash;

void protect_init(void)
{
    for (int i = 0; i < MAX_REGIONS; i++) regions[i].used = false;
    for (int i = 0; i < MAX_THREADS; i++) thread_priv[i] = PRIV_KERNEL;
    root_pw_hash = hash_str(DEFAULT_ROOT_PW);
}

/* Memulai pengujian proteksi */
void protect_test_begin(void)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        saved_priv[i] = thread_priv[i];
    }

    for (int i = 0; i < MAX_REGIONS; i++) {
        saved_regions[i] = regions[i];
        regions[i].used = false;
    }

    saved_hash = root_pw_hash;
    root_pw_hash = hash_str(DEFAULT_ROOT_PW);
}

/* Mengembalikan kondisi proteksi setelah pengujian */
void protect_test_end(void)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_priv[i] = saved_priv[i];
    }

    for (int i = 0; i < MAX_REGIONS; i++) {
        regions[i] = saved_regions[i];
    }

    root_pw_hash = saved_hash;
}

bool protect_add_region(uint32_t start, uint32_t size, uint32_t flags, const char *name)
{
    if (size == 0 || start + size < start) return false;

    for (int i = 0; i < MAX_REGIONS; i++) {
        if (!regions[i].used) {
            regions[i].start = start;
            regions[i].end   = start + size;
            regions[i].flags = flags;
            regions[i].name  = name;
            regions[i].used  = true;
            return true;
        }
    }
    return false;
}

void protect_set_priv(int32_t tid, priv_t p)
{
    if (tid >= 0 && tid < MAX_THREADS) thread_priv[tid] = p;
}

priv_t protect_get_priv(int32_t tid)
{
    if (tid >= 0 && tid < MAX_THREADS) return thread_priv[tid];
    return PRIV_USER;
}

/* Seluruh [addr, addr+len) harus berada di dalam satu region yang mengizinkan */
bool protect_check(priv_t who, uint32_t addr, uint32_t len, uint32_t access)
{
    if (len == 0) return true;

    uint32_t end = addr + len;
    if (end < addr) return false;

    for (int i = 0; i < MAX_REGIONS; i++) {
        mem_region_t *r = &regions[i];
        if (!r->used || addr < r->start || end > r->end) continue;

        if (who == PRIV_USER && !(r->flags & PERM_USER)) continue;
        if ((r->flags & access) == access) return true;
    }
    return false;
}

bool protect_require_kernel(const char *op)
{
    if (protect_get_priv(cur_id()) == PRIV_KERNEL) return true;

    kprintf_bg("[protect] DITOLAK: thread %d (user) mencoba operasi kernel '%s'\n",
            cur_id(), op);
    return false;
}

/* --------------------- Password mode root (simulasi) --------------------- */

static uint32_t hash_str(const char *s)
{
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
    return h;
}

int32_t protect_check_root_password(const char *password)
{
    if (hash_str(password) != root_pw_hash) {
        kprintf_bg("[protect] password root SALAH untuk thread %d\n", cur_id());
        return E_PERM;
    }
    return E_OK;
}

void protect_grant_root(void)
{
    int32_t tid = cur_id();
    protect_set_priv(tid, PRIV_KERNEL);
    kprintf_bg("[protect] thread %d naik ke mode root (kernel)\n", tid);
}

int32_t protect_request_root(const char *password)
{
    int32_t rc = protect_check_root_password(password);
    if (rc != E_OK) return rc;
    protect_grant_root();
    return E_OK;
}

void protect_drop_root(void)
{
    int32_t tid = cur_id();
    protect_set_priv(tid, PRIV_USER);
    kprintf_bg("[protect] thread %d kembali ke mode user\n", tid);
}

int32_t protect_change_password(const char *old_pw, const char *new_pw)
{
    if (!protect_require_kernel("ganti password")) return E_PERM;

    if (hash_str(old_pw) != root_pw_hash) {
        kprintf_bg("[protect] ganti password ditolak: password lama salah\n");
        return E_PERM;
    }

    if (new_pw[0] == '\0') return E_FAULT;

    root_pw_hash = hash_str(new_pw);
    kprintf_bg("[protect] password root berhasil diganti\n");
    return E_OK;
}

/* Salin string dari pointer milik user dengan validasi tiap byte */
static bool copy_user_str(priv_t who, uint32_t addr, char *out, uint32_t max)
{
    for (uint32_t i = 0; i < max; i++) {
        if (!protect_check(who, addr + i, 1, PERM_R)) return false;

        out[i] = *(const volatile char *)(addr + i);
        if (out[i] == '\0') return true;
    }
    return false;
}

/* ----------------------------- Syscall ----------------------------- */
int32_t protect_syscall(uint32_t num, uint32_t a1, uint32_t a2)
{
    int32_t tid = cur_id();
    priv_t priv = protect_get_priv(tid);

    switch (num) {
    case SYS_GETPID:
        return tid;

    case SYS_WRITE: {
        if (!protect_check(priv, a1, a2, PERM_R)) {
            kprintf_bg("[protect] SYS_WRITE ditolak: buffer 0x%x (%u byte) tidak valid\n",
                    a1, a2);
            return E_FAULT;
        }

        const char *p = (const char *)a1;
        for (uint32_t i = 0; i < a2; i++) kprintf("%c", p[i]);
        return (int32_t)a2;
    }

    case SYS_EXIT:
        kprintf_bg("[protect] thread %d keluar (kode %d)\n", tid, (int32_t)a1);
        return E_OK;

    case SYS_REBOOT:
        if (!protect_require_kernel("reboot")) return E_PERM;

        kprintf_bg("[protect] reboot diizinkan untuk thread kernel %d\n", tid);
        return E_OK;

    case SYS_SU: {
        char pw[PW_MAX];
        if (!copy_user_str(priv, a1, pw, PW_MAX)) return E_FAULT;
        return protect_request_root(pw);
    }

    case SYS_SETPW: {
        char old_pw[PW_MAX], new_pw[PW_MAX];

        if (!protect_require_kernel("ganti password")) return E_PERM;
        if (!copy_user_str(priv, a1, old_pw, PW_MAX)) return E_FAULT;
        if (!copy_user_str(priv, a2, new_pw, PW_MAX)) return E_FAULT;

        return protect_change_password(old_pw, new_pw);
    }

    case SYS_DROP:
        protect_drop_root();
        return E_OK;

    default:
        return E_NOSYS;
    }
}

