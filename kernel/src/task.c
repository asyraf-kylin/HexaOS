/* task.c - manajemen thread dan scheduler preemptive (Round Robin).
   1. Timer berdetak 100x/detik (IRQ0) dan memanggil task_tick().
   2. Tiap TASK_QUANTUM tick, task_tick() memanggil schedule().
   3. schedule() memilih thread READY berikutnya (melingkar) lalu
      switch_context() berpindah stack/register.
   Thread 0 ("shell") = eksekusi kernel_main/CLI yang sudah berjalan. */
#include "kernel.h"
#include "task.h"
#include "cli.h"

static task_t   tasks[TASK_MAX];
static uint8_t  stacks[TASK_MAX][TASK_STACK_SIZE] __attribute__((aligned(16)));
static task_t*  current = NULL;
static task_t*  idle    = NULL;
static uint32_t slice   = 0;

const char* task_state_name(task_state_t s) {
    switch (s) {
    case TASK_NEW:        return "NEW";
    case TASK_READY:      return "READY";
    case TASK_RUNNING:    return "RUNNING";
    case TASK_WAITING:    return "WAITING";
    case TASK_TERMINATED: return "TERMINATED";
    default:              return "-";
    }
}

task_t* task_current(void) { return current; }

/* ---------- scheduler ---------- */

/* Round Robin: cari thread READY mulai dari slot setelah thread sekarang.
   Kalau tidak ada, jalankan idle (hanya hlt). */
static task_t* pick_next(void) {
    int start = (int)(current - tasks);
    for (int i = 1; i <= TASK_MAX; i++) {
        task_t* t = &tasks[(start + i) % TASK_MAX];
        if (t != idle && t->state == TASK_READY) return t;
    }
    return idle;
}

/* HARUS dipanggil dengan interrupt mati. */
static void schedule(void) {
    task_t* prev = current;
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;

    task_t* next = pick_next();
    next->state = TASK_RUNNING;
    if (next == prev) return;

    current = next;
    next->switches++;
    switch_context(&prev->esp, next->esp);
}

/* Dipanggil dari handler IRQ0 tiap tick (interrupt sedang mati). */
void task_tick(void) {
    uint32_t now = timer_ticks();
    current->cpu_ticks++;

    for (int i = 0; i < TASK_MAX; i++) {          /* bangunkan yang tidurnya selesai */
        task_t* t = &tasks[i];
        if (t->state == TASK_WAITING && t->wake_tick &&
            (int32_t)(now - t->wake_tick) >= 0) {
            t->wake_tick = 0;
            t->state = TASK_READY;
        }
    }

    if (current == idle || ++slice >= TASK_QUANTUM) {   /* kuantum habis */
        slice = 0;
        schedule();
    }
}

/* ---------- siklus hidup thread ---------- */

/* Thread baru pertama kali jalan lewat sini (hasil "ret" di switch_context). */
static void task_trampoline(void) {
    __asm__ volatile("sti");   /* datang dari dalam handler interrupt: aktifkan lagi */
    current->entry(current->arg);
    task_exit();
}

task_t* task_create(const char* name, task_entry_t entry, void* arg) {
    uint32_t f = irq_save();

    int slot = -1;
    for (int i = 0; i < TASK_MAX && slot < 0; i++)
        if (tasks[i].state == TASK_UNUSED) slot = i;
    for (int i = 0; i < TASK_MAX && slot < 0; i++)      /* pakai ulang yang sudah selesai */
        if (tasks[i].state == TASK_TERMINATED) slot = i;
    if (slot < 0) { irq_restore(f); return NULL; }

    task_t* t = &tasks[slot];
    memset(t, 0, sizeof(*t));
    t->id = (uint32_t)slot;                 /* ID = nomor slot, dipakai ulang bersama slotnya */
    for (int i = 0; i < TASK_NAME_LEN - 1 && name[i]; i++) t->name[i] = name[i];
    t->stack = stacks[slot];
    t->entry = entry;
    t->arg   = arg;

    /* Stack awal supaya switch_context() "kembali" ke task_trampoline */
    uint32_t* sp = (uint32_t*)(t->stack + TASK_STACK_SIZE);
    *--sp = 0;                              /* alamat kembali palsu */
    *--sp = (uint32_t)task_trampoline;      /* "ret" melompat ke sini */
    *--sp = 0;                              /* ebp */
    *--sp = 0;                              /* ebx */
    *--sp = 0;                              /* esi */
    *--sp = 0;                              /* edi */
    t->esp = (uint32_t)sp;

    t->state = TASK_NEW;
    irq_restore(f);
    return t;
}

void task_start(task_t* t) {
    uint32_t f = irq_save();
    if (t && t->state == TASK_NEW) t->state = TASK_READY;
    irq_restore(f);
}

task_t* task_spawn(const char* name, task_entry_t entry, void* arg) {
    task_t* t = task_create(name, entry, arg);
    if (t) task_start(t);
    return t;
}

void task_yield(void) {
    uint32_t f = irq_save();
    slice = 0;
    schedule();
    irq_restore(f);
}

void task_sleep(uint32_t ms) {
    uint32_t ticks = (ms * TIMER_HZ + 999) / 1000;
    if (ticks == 0) ticks = 1;
    uint32_t f = irq_save();
    current->wake_tick = timer_ticks() + ticks;
    current->state = TASK_WAITING;
    schedule();
    irq_restore(f);
}

void task_block(void) {
    uint32_t f = irq_save();
    current->wake_tick = 0;
    current->state = TASK_WAITING;
    schedule();
    irq_restore(f);
}

void task_wake(task_t* t) {
    uint32_t f = irq_save();
    if (t && t->state == TASK_WAITING) {
        t->wake_tick = 0;
        t->state = TASK_READY;
    }
    irq_restore(f);
}

void task_exit(void) {
    (void)irq_save();                       /* tidak dipulihkan: thread berakhir */
    current->state = TASK_TERMINATED;
    schedule();
    for (;;) __asm__ volatile("hlt");
}

/* Hentikan paksa thread lain (dipakai saat Ctrl+C). Shell dan idle tidak boleh
   dimatikan. Thread yang dimatikan tidak pernah dijadwalkan lagi; slotnya
   bisa dipakai ulang oleh task_create(). */
void task_kill(task_t* t) {
    if (!t || t == &tasks[0] || t == idle) return;
    if (t == current) { task_exit(); return; }
    uint32_t f = irq_save();
    if (t->state != TASK_UNUSED && t->state != TASK_TERMINATED) {
        t->wake_tick = 0;
        t->state = TASK_TERMINATED;
    }
    irq_restore(f);
}

/* ---------- tampilan & demo ---------- */

static void pad(const char* s, int width) {
    int n = 0;
    while (s[n]) { kputc(s[n]); n++; }
    for (; n < width; n++) kputc(' ');
}

void task_dump(void) {
    kprintf("ID  NAMA        STATUS      CPU(tick)  GILIRAN\n");
    for (int i = 0; i < TASK_MAX; i++) {
        task_t* t = &tasks[i];
        if (t->state == TASK_UNUSED) continue;
        kprintf("%2u  ", t->id);
        pad(t->name, 10);
        kputs("  ");
        pad(task_state_name(t->state), 10);
        kprintf("  %8u  %7u\n", t->cpu_ticks, t->switches);
    }
}

static void idle_entry(void* arg) {
    for (;;) __asm__ volatile("hlt");
}

static void cmd_threads(int argc, char** argv) { task_dump(); }

/* Dua thread dengan tugas BERBEDA, supaya perubahan statusnya terlihat. */
static void thread_counter(void* arg) {
    for (int i = 1; i <= 3; i++) {
        kprintf("  [counter] status=%s, menghitung %d/3\n",
                task_state_name(task_current()->state), i);
        task_sleep(200);
    }
    kprintf("  [counter] selesai\n");
}

static void thread_greeter(void* arg) {
    static const char* const pesan[] = { "halo dari thread B", "sedang bekerja", "sampai jumpa" };
    for (int i = 0; i < 3; i++) {
        kprintf("  [greeter] status=%s, \"%s\"\n",
                task_state_name(task_current()->state), pesan[i]);
        task_sleep(300);
    }
    kprintf("  [greeter] selesai\n");
}

/* Tidur di shell, tapi memeriksa Ctrl+C tiap 10 ms. Return 1 kalau diinterupsi. */
static int sleep_interruptible(uint32_t ms) {
    uint32_t end = timer_ticks() + (ms * TIMER_HZ + 999) / 1000;
    while ((int32_t)(timer_ticks() - end) < 0) {
        if (kinterrupted()) return 1;
        task_sleep(10);
    }
    return 0;
}

/* Buang thread TERMINATED sisa demo sebelumnya supaya tabel demo bersih.
   Aman: thread TERMINATED sudah pindah dari stack-nya dan tidak dijadwalkan lagi. */
static void reap_terminated(void) {
    uint32_t f = irq_save();
    for (int i = 0; i < TASK_MAX; i++)
        if (tasks[i].state == TASK_TERMINATED) tasks[i].state = TASK_UNUSED;
    irq_restore(f);
}

/* Ctrl+C saat demo*/
static void demo_abort(task_t* a, task_t* b) {
    task_kill(a);
    task_kill(b);
    kputs("\n^C\n");                        /* newline: prompt berikutnya di baris baru */
}

static void cmd_demo_thread(int argc, char** argv) {
    reap_terminated();
    task_t* a = task_create("counter", thread_counter, NULL);
    task_t* b = task_create("greeter", thread_greeter, NULL);
    if (!a || !b) {
        if (a) a->state = TASK_TERMINATED;
        if (b) b->state = TASK_TERMINATED;
        kprintf("gagal membuat thread: slot penuh\n");
        return;
    }

    kprintf("\n[1] Baru dibuat -> status NEW:\n");
    task_dump();

    uint32_t f = irq_save();
    task_start(a);
    task_start(b);
    kprintf("\n[2] Sudah di-start -> status READY:\n");
    task_dump();
    irq_restore(f);

    kprintf("\n[3] Snapshot tiap 250 ms (Ctrl+C = batal):\n");
    for (int i = 1; i <= 4; i++) {
        if (sleep_interruptible(250)) { demo_abort(a, b); return; }
        kprintf("-- snapshot %d --\n", i);
        task_dump();
    }

    if (sleep_interruptible(600)) { demo_abort(a, b); return; }
    kprintf("\n[4] Akhir -> semua TERMINATED:\n");
    task_dump();
}

/* Bukti preemption: dua thread loop TANPA yield, giliran tetap bergantian. */
static void spinner(void* arg) {
    char c = (char)(uint32_t)arg;
    uint32_t end = timer_ticks() + 120;
    uint32_t last = 0;
    while (timer_ticks() < end) {
        uint32_t now = timer_ticks();
        if (now != last) { last = now; kputc(c); }
    }
}

static void cmd_demo_preempt(int argc, char** argv) {
    reap_terminated();
    kprintf("Dua thread loop tanpa yield, 1 huruf = 1 tick CPU (Ctrl+C = batal):\n");
    task_t* a = task_spawn("spin-A", spinner, (void*)'A');
    task_t* b = task_spawn("spin-B", spinner, (void*)'B');
    if (!a || !b) {
        task_kill(a); task_kill(b);
        kprintf("gagal membuat thread: slot penuh\n");
        return;
    }
    if (sleep_interruptible(1600)) { demo_abort(a, b); return; }
    kputc('\n');
    kprintf("giliran A=%u, B=%u (ganti tiap %d tick = %d ms)\n",
            a->switches, b->switches, TASK_QUANTUM, TASK_QUANTUM * 1000 / TIMER_HZ);
}

/* ---------- init ---------- */

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));

    /* Thread 0 = eksekusi yang sedang berjalan (kernel_main -> CLI) */
    task_t* shell = &tasks[0];
    shell->id    = 0;
    shell->name[0] = 's'; shell->name[1] = 'h'; shell->name[2] = 'e';
    shell->name[3] = 'l'; shell->name[4] = 'l';
    shell->state = TASK_RUNNING;
    shell->switches = 1;
    current = shell;

    idle = task_spawn("idle", idle_entry, NULL);

    timer_init(TIMER_HZ, task_tick);

    cli_register("threads",      "list all threads",     cmd_threads);
    cli_register("demo-thread",  "demo thread status cycles",     cmd_demo_thread);
    cli_register("demo-preempt", "demo preemption (without yield)", cmd_demo_preempt);

    kprintf("[ OK ] Scheduler Round Robin, kuantum %d ms\n", TASK_QUANTUM * 1000 / TIMER_HZ);
}