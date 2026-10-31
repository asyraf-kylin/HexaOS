#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX        8      /* jumlah slot thread (termasuk shell dan idle) */
#define TASK_STACK_SIZE 8192   /* stack kernel per thread (byte) */
#define TASK_NAME_LEN   16
#define TIMER_HZ        100    /* 1 tick = 10 ms */
#define TASK_QUANTUM    2      /* kuantum Round Robin: 2 tick = 20 ms */

/* Siklus hidup thread: NEW -> READY <-> RUNNING <-> WAITING -> TERMINATED */
typedef enum {
    TASK_UNUSED = 0,           /* slot kosong */
    TASK_NEW,                  /* sudah dibuat, belum dijadwalkan */
    TASK_READY,                /* siap jalan, menunggu giliran CPU */
    TASK_RUNNING,              /* sedang memakai CPU */
    TASK_WAITING,              /* tidur atau menunggu (mutex, dst.) */
    TASK_TERMINATED            /* selesai */
} task_state_t;

typedef void (*task_entry_t)(void* arg);

/* TCB: "kartu identitas" satu thread */
typedef struct task {
    uint32_t     id;
    char         name[TASK_NAME_LEN];
    task_state_t state;
    uint32_t     esp;          /* nilai esp yang disimpan saat thread tidak jalan */
    uint8_t*     stack;        /* dasar stack milik thread ini */
    task_entry_t entry;        /* fungsi yang dijalankan thread */
    void*        arg;
    uint32_t     wake_tick;    /* kalau tidur: bangun di tick ini (0 = tidak tidur) */
    uint32_t     cpu_ticks;    /* berapa tick thread ini memegang CPU */
    uint32_t     switches;     /* berapa kali thread ini mendapat giliran */
} task_t;

/* ---- API thread ---- */
void        task_init(void);                                   /* dipanggil main.c */
task_t*     task_create(const char* name, task_entry_t entry, void* arg);  /* -> NEW */
void        task_start(task_t* t);                             /* NEW -> READY */
task_t*     task_spawn(const char* name, task_entry_t entry, void* arg);   /* create + start */
void        task_yield(void);                                  /* serahkan CPU sukarela */
void        task_sleep(uint32_t ms);                           /* RUNNING -> WAITING sementara */
void        task_block(void);                                  /* RUNNING -> WAITING sampai task_wake() */
void        task_wake(task_t* t);                              /* WAITING -> READY */
void        task_exit(void);                                   /* RUNNING -> TERMINATED */
task_t*     task_current(void);
const char* task_state_name(task_state_t s);
void        task_dump(void);                                   /* cetak tabel thread */
void        task_tick(void);                                   /* dipanggil timer tiap tick */

/* ---- Timer (drivers/timer.c) ---- */
typedef void (*timer_cb_t)(void);
void     timer_init(uint32_t hz, timer_cb_t cb);
uint32_t timer_ticks(void);

/* ---- Context switch (arch/switch.asm) ---- */
void switch_context(uint32_t* old_esp, uint32_t new_esp);

/* ---- Bagian kritis: matikan interrupt sementara (dipakai juga oleh mutex) ---- */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

#endif