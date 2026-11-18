#include "todo.h"
#include "vfs.h"
#include "sync.h"
#include "memory.h"
#include "kernel.h"

/* =====================================================================
 * PENANDA DEMO BAB II/III - PDF poin 4, 6, 7
 * ---------------------------------------------------------------------
 * ATURAN EMAS: kunci mutex_vfs DULU, baru mutex_counter.
 * Semua jalur di file ini mengikuti aturan itu (lihat todo_recount,
 * todo_add). Pelanggaran urutan hanya boleh muncul di tests.c,
 * bukan di sini.
 *
 * [DEMO-RACE] Alur normal (add/done/list) TIDAK menunjukkan race karena
 *   (a) counter diubah di bawah mutex_counter, dan
 *   (b) setiap operasi ditutup todo_recount() yang menghitung ulang dari VFS.
 *   Kalau mau memaksa race lewat perintah biasa, lakukan ini SEMENTARA:
 *     1) di todo_add(), ganti `todo_recount(&cached_active, &cached_done);`
 *        menjadi `mutex_lock(&mutex_counter); cached_active++; mutex_unlock(&mutex_counter);`
 *        -- lalu hapus 3 baris mutex itu untuk versi "tanpa mutex".
 *     2) di svc.c, tambahkan thread kedua yang melakukan cached_active++
 *        tanpa mutex (atau pakai race_worker di tests.c sebagai acuan).
 *     3) ketik add/done bergantian cepat lalu sysinfo: angka Aktif/Selesai
 *        akan selisih dengan isi `list`. Kembalikan kode setelah demo.
 *   Lebih aman & sesuai PDF: pakai `diag race` (perbandingan nyata).
 *
 * [DEMO-DEADLOCK] Jangan pernah membalik urutan kunci di file ini untuk
 *   demo normal: kiosk akan hang sampai QEMU di-reset. Skenario deadlock
 *   yang aman (timeout + lock ordering) sudah ada di `diag deadlock`.
 *
 * [UBAH-SESUAIPDF] Dua perbedaan dengan dokumen "Tugas Besar" / task board:
 *   - ID sekarang mulai 0 dan di-reset saat papan kosong. PDF meminta
 *     ID mulai 1 dan TIDAK dipakai ulang. Lihat penanda global_id_counter
 *     dan reset_id_if_empty() di bawah.
 *   - todo_at() mengurutkan menurut slot VFS, bukan menurut id. PDF
 *     meminta urutan id naik. Lihat penanda di todo_at().
 * ===================================================================== */

static mutex_t mutex_vfs;
static mutex_t mutex_counter;
static int global_id_counter = 0;   /* [UBAH-SESUAIPDF] PDF: mulai dari 1, bukan 0 */
static int cached_active = 0;       /* [DEMO-RACE] counter bersama yang dijaga mutex_counter */
static int cached_done = 0;

// Deklarasi helper vfs_name_at dari vfs.c
int vfs_name_at(int index, char *out, size_t max);

/* [UBAH-SESUAIPDF] Fungsi ini melanggar syarat "ID tidak dipakai ulang".
   Task board kiosk menyuruh: del 1 lalu add lagi harus mendapat id baru (4),
   bukan mengulang nomor lama. Untuk mengikuti PDF:
     - hapus / comment-out seluruh isi fungsi ini (biarkan return), dan
     - hapus pemanggilannya di todo_del() dan todo_purge_done(), dan
     - ubah inisialisasi global_id_counter di todo_init() menjadi 1,
     - ubah `int new_id = global_id_counter;` di todo_add() jadi mulai 1.
   Perilaku sekarang sengaja dipertahankan supaya demo kiosk tetap rapi. */
/* Dipanggil dengan mutex_vfs masih terkunci. Kalau papan sudah kosong,
   nomor tugas berikutnya mulai lagi dari 0. */
static void reset_id_if_empty(void) {
    char fname[32];
    if (vfs_name_at(0, fname, sizeof(fname)) != 0) {
        mutex_lock(&mutex_counter);
        global_id_counter = 0;
        mutex_unlock(&mutex_counter);
    }
}

void todo_init(void) {
    mutex_init(&mutex_vfs, "vfs_lock");       // Berikan nama mutex
    mutex_init(&mutex_counter, "counter_lock"); // Berikan nama mutex
    global_id_counter = 0;
    todo_recount(&cached_active, &cached_done);
}

static void make_todo_filename(int id, char *buf, int max) {
    int temp = id;
    int len = 0;
    char rev[16];
    const char *p = "todo_";
    int i = 0;
    while (p[i] && i < max - 1) {
        buf[i] = p[i];
        i++;
    }
    if (temp == 0) {
        rev[len++] = '0';
    } else {
        while (temp > 0) {
            rev[len++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    while (len > 0 && i < max - 5) {
        buf[i++] = rev[--len];
    }
    const char *ext = ".txt";
    int j = 0;
    while (ext[j] && i < max - 1) {
        buf[i++] = ext[j++];
    }
    buf[i] = '\0';
}

// Sesuai dengan todo.h: menerima 2 parameter pointer
/* [DEMO-DEADLOCK] Urutan kunci di sini adalah solusi lock ordering:
   vfs dulu, counter kemudian. Jangan dibalik. Dipanggil thread `summary`
   tiap 1 detik (svc.c) dan juga setelah add/done/del di file ini. */
void todo_recount(int *active_out, int *done_out) {
    mutex_lock(&mutex_vfs);
    mutex_lock(&mutex_counter);
    
    int active = 0;
    int done = 0;
    char fname[32];
    
    int idx = 0;
    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        char *content = vfs_read(fname);
        if (content) {
            if (content[1] == 'x' || content[1] == 'X') {
                done++;
            } else {
                active++;
            }
        }
        idx++;
    }
    
    cached_active = active;
    cached_done = done;
    
    if (active_out) *active_out = active;
    if (done_out) *done_out = done;
    
    mutex_unlock(&mutex_counter);
    mutex_unlock(&mutex_vfs);
}

int todo_add(const char *text) {
    if (!text || text[0] == '\0') return TODO_ERR_EMPTY;

    mutex_lock(&mutex_vfs);
    mutex_lock(&mutex_counter);
    int new_id = global_id_counter;      /* tugas pertama bernomor 0 */
    global_id_counter++;
    mutex_unlock(&mutex_counter);
    
    char filename[32];
    make_todo_filename(new_id, filename, sizeof(filename));
    
    int res_create = vfs_create(filename);
    if (res_create < 0) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_FULL;
    }
    
    char write_buf[TODO_TEXT_MAX + 16];
    write_buf[0] = '[';
    write_buf[1] = ' ';
    write_buf[2] = ']';
    write_buf[3] = ' ';
    
    int idx = 4;
    int t = 0;
    while (text[t] && idx < (int)sizeof(write_buf) - 2) {
        write_buf[idx++] = text[t++];
    }
    write_buf[idx] = '\0';
    
    int res_write = vfs_write(filename, write_buf);
    if (res_write == -3) {
        vfs_delete(filename);
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_OOM;
    }
    
    mutex_unlock(&mutex_vfs);
    /* [DEMO-RACE] todo_recount() inilah yang menutup celah race di alur normal:
       angka di papan selalu dihitung ulang dari file VFS, jadi tidak bisa
       "selisih" walaupun preemption terjadi di tengah add. Lihat banner file. */
    todo_recount(&cached_active, &cached_done);
    return new_id;
}

int todo_done(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    char *content = vfs_read(fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char buf[256];
    int i = 0;
    while (content[i] && i < (int)sizeof(buf) - 1) {
        buf[i] = content[i];
        i++;
    }
    buf[i] = '\0';
    
    if (buf[1] == ' ') {
        buf[1] = 'x';
        vfs_write(fname, buf);
    }
    
    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

/* Kembalikan tugas yang sudah selesai ke status belum selesai. */
int todo_undone(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));

    char *content = vfs_read(fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }

    char buf[256];
    int i = 0;
    while (content[i] && i < (int)sizeof(buf) - 1) {
        buf[i] = content[i];
        i++;
    }
    buf[i] = '\0';

    if (buf[1] == 'x' || buf[1] == 'X') {
        buf[1] = ' ';
        vfs_write(fname, buf);
    }

    mutex_unlock(&mutex_vfs);
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_edit(int id, const char *text) {
    if (!text || text[0] == '\0') return TODO_ERR_EMPTY;

    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    char *content = vfs_read(fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char status = content[1];
    
    char buf[256];
    buf[0] = '[';
    buf[1] = status;
    buf[2] = ']';
    buf[3] = ' ';
    
    int idx = 4;
    int t = 0;
    while (text[t] && idx < (int)sizeof(buf) - 2) {
        buf[idx++] = text[t++];
    }
    buf[idx] = '\0';
    
    vfs_write(fname, buf);
    mutex_unlock(&mutex_vfs);
    return TODO_OK;
}

int todo_del(int id) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    make_todo_filename(id, fname, sizeof(fname));
    
    int res = vfs_delete(fname);
    if (res == 0) reset_id_if_empty();
    mutex_unlock(&mutex_vfs);
    
    if (res < 0) return TODO_ERR_NOID;
    
    todo_recount(&cached_active, &cached_done);
    return TODO_OK;
}

int todo_purge_done(void) {
    mutex_lock(&mutex_vfs);
    char fname[32];
    int purged = 0;
    int idx = 0;
    
    while (vfs_name_at(idx, fname, sizeof(fname)) == 0) {
        char *content = vfs_read(fname);
        if (content) {
            if (content[1] == 'x' || content[1] == 'X') {
                vfs_delete(fname);
                purged++;
                continue; 
            }
        }
        idx++;
    }
    if (purged > 0) reset_id_if_empty();
    mutex_unlock(&mutex_vfs);
    
    todo_recount(&cached_active, &cached_done);
    return purged;
}

int todo_count(void) {
    int active, done;
    todo_get_counts(&active, &done);
    return active + done;
}

/* [UBAH-SESUAIPDF] Sekarang yang diurutkan adalah posisi slot VFS. Karena
   vfs_delete() memakai ulang slot kosong, urutan di layar BISA bukan urutan id.
   PDF/task board meminta urutan id naik (1,2,3,4). Untuk mengikuti:
     kumpulkan semua nama file todo_<id>.txt, cari id minimum yang belum
     dipakai pada setiap langkah (maks. 64 file, jadi O(n^2) cukup), baru
     baca isinya. Jangan lupa tetap di dalam mutex_vfs. */
int todo_at(int index, int *id_out, int *done_out, char *text_out, int max) {
    mutex_lock(&mutex_vfs);
    
    char fname[32];
    int found_idx = 0;
    char found_fname[32];
    int found = 0;
    
    int i = 0;
    while (vfs_name_at(i, fname, sizeof(fname)) == 0) {
        if (found_idx == index) {
            int k = 0;
            while (fname[k] && k < (int)sizeof(found_fname) - 1) {
                found_fname[k] = fname[k];
                k++;
            }
            found_fname[k] = '\0';
            found = 1;
            break;
        }
        found_idx++;
        i++;
    }
    
    if (!found) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char *content = vfs_read(found_fname);
    if (!content) {
        mutex_unlock(&mutex_vfs);
        return TODO_ERR_NOID;
    }
    
    char buf[256];
    int sz = 0;
    while (content[sz] && sz < (int)sizeof(buf) - 1) {
        buf[sz] = content[sz];
        sz++;
    }
    buf[sz] = '\0';
    
    mutex_unlock(&mutex_vfs);
    
    int parsed_id = 0;
    const char *p = found_fname + 5;
    while (*p >= '0' && *p <= '9') {
        parsed_id = parsed_id * 10 + (*p - '0');
        p++;
    }
    
    if (id_out) *id_out = parsed_id;
    if (done_out) *done_out = (buf[1] == 'x' || buf[1] == 'X') ? 1 : 0;
    
    int src_idx = 4;
    int dst_idx = 0;
    while (buf[src_idx] && dst_idx < max - 1) {
        text_out[dst_idx++] = buf[src_idx++];
    }
    text_out[dst_idx] = '\0';
    
    return TODO_OK;
}

void todo_get_counts(int *active, int *done) {
    mutex_lock(&mutex_counter);
    if (active) *active = cached_active;
    if (done) *done = cached_done;
    mutex_unlock(&mutex_counter);
}