/* stubs.c - versi kosong modul yang belum dibuat anggota lain.
   Atribut "weak" artinya: kalau modul aslinya (memory.c, vfs.c, task.c)
   mendefinisikan fungsi yang sama, versi asli otomatis menggantikan stub
   ini. Jadi main.c tidak perlu diubah saat modul teman masuk. */
#include "kernel.h"

__attribute__((weak)) void mem_init(void) {
    kprintf("[ -- ] memori    : belum diimplementasi (stub)\n");
}

__attribute__((weak)) void vfs_init(void) {
    kprintf("[ -- ] vfs       : belum diimplementasi (stub)\n");
}

__attribute__((weak)) void sched_init(void) {
    kprintf("[ -- ] scheduler : belum diimplementasi (stub)\n");
}