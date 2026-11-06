#include "kernel.h"
#include "cli.h"

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
    task_init();     kdelay_ms(BOOT_STEP_MS);
    sync_init();     /* Anggota 3: mutex/semaphore + perintah demo */
    protect_init();  /* Anggota 3: hak akses */
    
    interrupts_enable();
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