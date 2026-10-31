/* stubs.c - versi kosong modul yang belum dibuat anggota lain.
   Atribut "weak" artinya: kalau modul aslinya (memory.c, vfs.c, task.c)
   mendefinisikan fungsi yang sama, versi asli otomatis menggantikan stub
   ini. Jadi main.c tidak perlu diubah saat modul teman masuk. */
#include "kernel.h"

__attribute__((weak)) void memory_init(void) {
    kprintf("[ SKIP ] Memory manager not implemented (stub)\n");
}

__attribute__((weak)) void vfs_init(void) {
    kprintf("[ SKIP ] Virtual file system not implemented (stub)\n");
}

__attribute__((weak)) void task_init(void) {
    kprintf("[ SKIP ] Task scheduler not implemented (stub)\n");
}

__attribute__((weak)) void sync_init(void)    { }
__attribute__((weak)) void protect_init(void) { }