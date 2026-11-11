/*
 * protect.c - Proteksi hak akses HexaOS (pemisahan user mode & kernel mode)
 *
 * Level 1 (aktif default, bisa diuji langsung di QEMU):
 *   - Setiap thread punya privilege: PRIV_KERNEL atau PRIV_USER.
 *   - Tabel region memori dengan izin R/W/X dan flag PERM_USER.
 *   - Semua akses user ke memori / layanan kernel divalidasi lewat
 *     protect_check() dan protect_syscall().
 *
 * Level 2 (opsional, hardware ring 3): aktifkan dengan -DHEXAOS_HW_RING3.
 *   Membutuhkan GDT dengan segmen ring 3 (0x1B kode, 0x23 data) dan TSS
 *   (esp0) yang diset oleh bagian GDT/boot.
 */
#include "kernel.h"
#include "protect.h"
#include "task.h"      /* task_current(), task_t (milik Anggota 2) */
#include "kernel.h"    /* kprintf -- sesuaikan kalau deklarasinya ada di header lain */

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
    uint32_t    end;      /* eksklusif */
    uint32_t    flags;
    const char *name;
    bool        used;
} mem_region_t;

#define PW_MAX          32
#define DEFAULT_ROOT_PW "root"

static uint32_t root_pw_hash;           /* hash password root (djb2) */
static uint32_t hash_str(const char *s);
static mem_region_t regions[MAX_REGIONS];
static priv_t       thread_priv[MAX_THREADS];

void protect_init(void)
{
    for (int i = 0; i < MAX_REGIONS; i++) regions[i].used = false;
    for (int i = 0; i < MAX_THREADS; i++) thread_priv[i] = PRIV_KERNEL;
    root_pw_hash = hash_str(DEFAULT_ROOT_PW);
}

bool protect_add_region(uint32_t start, uint32_t size, uint32_t flags, const char *name)
{
    if (size == 0 || start + size < start) return false;   /* overflow */
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
    return PRIV_USER;           /* ID tak dikenal: perlakukan paling terbatas */
}

/* Seluruh [addr, addr+len) harus berada di dalam satu region yang mengizinkan */
bool protect_check(priv_t who, uint32_t addr, uint32_t len, uint32_t access)
{
    if (len == 0) return true;
    uint32_t end = addr + len;
    if (end < addr) return false;                           /* overflow */

    for (int i = 0; i < MAX_REGIONS; i++) {
        mem_region_t *r = &regions[i];
        if (!r->used || addr < r->start || end > r->end) continue;

        /* user mode hanya boleh memakai region ber-flag PERM_USER */
        if (who == PRIV_USER && !(r->flags & PERM_USER)) continue;
        if ((r->flags & access) == access) return true;
    }
    return false;                                           /* tidak ada region yang mengizinkan */
}

bool protect_require_kernel(const char *op)
{
    if (protect_get_priv(cur_id()) == PRIV_KERNEL) return true;
    kprintf("[protect] DITOLAK: thread %d (user) mencoba operasi kernel '%s'\n",
            cur_id(), op);
    return false;
}


/* --------------------- Password mode root (simulasi) ---------------------
 * Password TIDAK disimpan sebagai teks, hanya hash djb2 (sederhana).
 * Password awal: "root" (ganti lewat protect_change_password saat mode root).
 * Catatan: ini simulasi untuk tugas kuliah, bukan keamanan sungguhan.
 */
static uint32_t hash_str(const char *s)
{
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
    return h;
}

int32_t protect_request_root(const char *password)
{
    int32_t tid = cur_id();
    if (hash_str(password) != root_pw_hash) {
        kprintf("[protect] password root SALAH untuk thread %d\n", tid);
        return E_PERM;
    }
    protect_set_priv(tid, PRIV_KERNEL);
    kprintf("[protect] thread %d naik ke mode root (kernel)\n", tid);
    return E_OK;
}

void protect_drop_root(void)
{
    int32_t tid = cur_id();
    protect_set_priv(tid, PRIV_USER);
    kprintf("[protect] thread %d kembali ke mode user\n", tid);
}

int32_t protect_change_password(const char *old_pw, const char *new_pw)
{
    if (!protect_require_kernel("ganti password")) return E_PERM;   /* hanya mode root */
    if (hash_str(old_pw) != root_pw_hash) {
        kprintf("[protect] ganti password ditolak: password lama salah\n");
        return E_PERM;
    }
    if (new_pw[0] == '\0') return E_FAULT;                          /* password kosong */
    root_pw_hash = hash_str(new_pw);
    kprintf("[protect] password root berhasil diganti\n");
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
    return false;                                                   /* terlalu panjang */
}

/* ----------------------------- Syscall ----------------------------- */
int32_t protect_syscall(uint32_t num, uint32_t a1, uint32_t a2)
{
    int32_t tid  = cur_id();
    priv_t  priv = protect_get_priv(tid);

    switch (num) {
    case SYS_GETPID:
        return tid;

    case SYS_WRITE: {                       /* a1 = buffer, a2 = panjang */
        if (!protect_check(priv, a1, a2, PERM_R)) {
            kprintf("[protect] SYS_WRITE ditolak: buffer 0x%x (%u byte) tidak valid\n", a1, a2);
            return E_FAULT;
        }
        const char *p = (const char *)a1;
        for (uint32_t i = 0; i < a2; i++) kprintf("%c", p[i]);
        return (int32_t)a2;
    }

    case SYS_EXIT:
        kprintf("[protect] thread %d keluar (kode %d)\n", tid, (int32_t)a1);
        return E_OK;

    case SYS_REBOOT:                        /* khusus kernel */
        if (!protect_require_kernel("reboot")) return E_PERM;
        kprintf("[protect] reboot diizinkan untuk thread kernel %d\n", tid);
        return E_OK;

    case SYS_SU: {                          /* a1 = ptr password */
        char pw[PW_MAX];
        if (!copy_user_str(priv, a1, pw, PW_MAX)) return E_FAULT;
        return protect_request_root(pw);
    }

    case SYS_SETPW: {                       /* a1 = ptr lama, a2 = ptr baru */
        char old_pw[PW_MAX], new_pw[PW_MAX];
        if (!protect_require_kernel("ganti password")) return E_PERM;   /* cek mode dulu */
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

/* --------------------- Level 2: hardware ring 3 --------------------- */
#ifdef HEXAOS_HW_RING3
/* Masuk ke user mode lewat iret. Selector: 0x1B = kode ring3, 0x23 = data ring3 */
void enter_user_mode(uint32_t entry, uint32_t user_stack)
{
    __asm__ volatile(
        "cli\n"
        "movw $0x23, %%ax\n"
        "movw %%ax, %%ds\n movw %%ax, %%es\n movw %%ax, %%fs\n movw %%ax, %%gs\n"
        "pushl $0x23\n"          /* SS  */
        "pushl %0\n"             /* ESP */
        "pushfl\n"
        "orl $0x200, (%%esp)\n"  /* EFLAGS: IF=1 */
        "pushl $0x1B\n"          /* CS  */
        "pushl %1\n"             /* EIP */
        "iret\n"
        : : "r"(user_stack), "r"(entry) : "eax", "memory");
}
#endif