#include "kernel.h"
#include "cli.h"
#include "todo.h"
#include "svc.h"

/* =====================================================================
 * PENANDA DEMO BAB III - urutan boot (PDF poin 1)
 * ---------------------------------------------------------------------
 * vga/serial/gdt/idt/keyboard -> logo ASCII -> memory -> vfs -> todo ->
 * task (pasang IRQ0/RR) -> sync -> protect -> STI -> summary_start -> CLI.
 *
 * [JANGAN] menghapus print_logo_ascii() / print_logo_unicode(): PDF poin 1
 *   mewajibkan logo ASCII saat booting.
 * [JANGAN] menghapus task_init() (scheduler) dan summary_start() (thread 2).
 * [SUDAH-DIHAPUS] sync.c, sync_init(), dan mutex_modul_init(): dulu hanya
 *   kprintf + fungsi kosong. Mutex nyata tetap diinisialisasi per objek
 *   lewat mutex_init() di todo.c / tests.c.
 * [OPSIONAL] pindahkan PDF ke folder docs/ dan tambahkan build/ ke .gitignore
 *   sesuai task board (tidak memengaruhi biner).
 * ===================================================================== */

#define BOOT_STEP_MS  350   
#define BOOT_BAR_MS   60    
#define BOOT_BAR_LEN  24

static void boot_progress(const char* label) {
    for (int i = 0; i <= BOOT_BAR_LEN; i++) {
        kprintf("\r  %s [", label);
        for (int j = 0; j < BOOT_BAR_LEN; j++) kputc(j < i ? '#' : '.');
        kprintf("] %d%%", i * 100 / BOOT_BAR_LEN);
        kdelay_ms(BOOT_BAR_MS);
    }
    kputc('\n');
}

void kernel_main(void) {
    vga_init();
    serial_init();
    gdt_init();
    idt_init();
    keyboard_init();
    print_logo_ascii();

    kprintf("[ OK ] VGA text mode initialized (80x25)\n");  kdelay_ms(BOOT_STEP_MS);
    kprintf("[ OK ] Serial port COM1 initialized (115200 baud)\n");   kdelay_ms(BOOT_STEP_MS);
    kprintf("[ OK ] Architecture setup complete (GDT, IDT, PIC)\n");  kdelay_ms(BOOT_STEP_MS); 
    kprintf("[ OK ] Keyboard initialized (PS/2)\n");  kdelay_ms(BOOT_STEP_MS);

    memory_init();   kdelay_ms(BOOT_STEP_MS);
    vfs_init();      kdelay_ms(BOOT_STEP_MS);
    todo_init();     kdelay_ms(BOOT_STEP_MS);
    task_init();     kdelay_ms(BOOT_STEP_MS);
    protect_init();  /* Anggota 3: hak akses */
    
    interrupts_enable();
    summary_start();  /* [JANGAN-HAPUS] thread latar belakang ke-2 (PDF poin 2) */
    cli_init();
    kprintf("[ OK ] Command-line interface ready\n\n");
    boot_progress("Loading system");
    kdelay_ms(600);

    vga_clear();
    print_logo_unicode();
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    print_authors();
    kputc('\n');
    kprintf("Type 'help' for a list of available commands.\n\n");
    cli_run();
}