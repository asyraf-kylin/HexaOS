#include "kernel.h"
#include "todo.h"
#include "svc.h"

__attribute__((weak)) void memory_init(void) { kprintf("[ SKIP ] Memory manager not implemented (stub)\n"); }
__attribute__((weak)) void vfs_init(void)    { kprintf("[ SKIP ] Virtual file system not implemented (stub)\n"); }
__attribute__((weak)) void task_init(void)   { kprintf("[ SKIP ] Task scheduler not implemented (stub)\n"); }
__attribute__((weak)) void sync_init(void)    { }
__attribute__((weak)) void protect_init(void) { }

/* Anggota 4: todo.c */
__attribute__((weak)) void todo_init(void) { kprintf("[ SKIP ] Todo board not implemented (stub)\n"); }
__attribute__((weak)) int  todo_add(const char* t)             { (void)t; return TODO_ERR_NOMOD; }
__attribute__((weak)) int  todo_done(int id)                   { (void)id; return TODO_ERR_NOMOD; }
__attribute__((weak)) int  todo_edit(int id, const char* t)    { (void)id; (void)t; return TODO_ERR_NOMOD; }
__attribute__((weak)) int  todo_del(int id)                    { (void)id; return TODO_ERR_NOMOD; }
__attribute__((weak)) int  todo_purge_done(void)               { return 0; }
__attribute__((weak)) int  todo_count(void)                    { return 0; }
__attribute__((weak)) int  todo_at(int i, int* id, int* d, char* t, int m) {
    (void)i; (void)id; (void)d; (void)t; (void)m; return TODO_ERR_NOID;
}
__attribute__((weak)) void todo_get_counts(int* a, int* d)     { *a = 0; *d = 0; }
__attribute__((weak)) void todo_recount(int* a, int* d)        { *a = 0; *d = 0; }

/* Anggota 2 */
__attribute__((weak)) void summary_start(void) { }
__attribute__((weak)) int  purge_start(void)   { todo_purge_done(); return 0; }
__attribute__((weak)) void synclog_dump(void)  { kprintf("  (log sinkronisasi belum tersedia)\n"); }

/* Uji (test_deadlock & test_protect sudah ada di tests.c) */
__attribute__((weak)) void test_threads(void) { kprintf("diag threads: belum diimplementasikan (Anggota 2)\n"); }
__attribute__((weak)) void test_preempt(void) { kprintf("diag preempt: belum diimplementasikan (Anggota 2)\n"); }
__attribute__((weak)) void test_race(void)    { kprintf("diag race: belum diimplementasikan (Anggota 3)\n"); }
__attribute__((weak)) void test_mem(void)     { kprintf("diag mem: belum diimplementasikan (Anggota 4)\n"); }