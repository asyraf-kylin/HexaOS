#ifndef TODO_H
#define TODO_H
/* Implementasi: todo.c (Anggota 4). Aturan lock: mutex_vfs dulu, baru mutex_counter.
   Semua fungsi mengunci mutex sendiri. */
#define TODO_OK          0
#define TODO_ERR_FULL   -1   /* slot VFS habis */
#define TODO_ERR_OOM    -2   /* kmalloc gagal */
#define TODO_ERR_NOID   -3
#define TODO_ERR_EMPTY  -4
#define TODO_ERR_NOMOD  -9   /* stub: todo.c belum terpasang */
#define TODO_TEXT_MAX   120

void todo_init(void);
int  todo_add(const char* text);            /* >0 = id baru, <0 = error */
int  todo_done(int id);
int  todo_undone(int id);                    /* [x] -> [ ] lagi */
int  todo_done_all(void);                    /* return jumlah yang berubah */
int  todo_undone_all(void);                  /* return jumlah yang berubah */
int  todo_edit(int id, const char* text);
int  todo_del(int id);
int  todo_del_all(void);                     /* return jumlah yang dihapus */
int  todo_purge_done(void);                 /* return jumlah yang dihapus */

int  todo_count(void);
int  todo_at(int index, int* id, int* done, char* text, int max);  /* teks DISALIN */

void todo_get_counts(int* active, int* done);   /* baca counter bersama */
void todo_recount(int* active, int* done);      /* hitung ulang dari VFS (thread summary) */
#endif