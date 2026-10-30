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
    print_logo();

    kprintf("[ OK ] VGA text mode 80x25\n");  kdelay_ms(BOOT_STEP_MS);
    kprintf("[ OK ] Serial COM1 115200\n");   kdelay_ms(BOOT_STEP_MS);
    kprintf("[ OK ] GDT, IDT, PIC (arch)\n");  kdelay_ms(BOOT_STEP_MS); 
    kprintf("[ OK ] Keyboard PS/2\n");  kdelay_ms(BOOT_STEP_MS);

    mem_init();    kdelay_ms(BOOT_STEP_MS);
    vfs_init();    kdelay_ms(BOOT_STEP_MS);
    sched_init();  kdelay_ms(BOOT_STEP_MS);
    interrupts_enable();
    cli_init();
    kprintf("[ OK ] CLI Ready\n\n");
    boot_progress("Memuat sistem");
    kdelay_ms(600);

    vga_clear();
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("%s v%s\n", OS_NAME, OS_VERSION);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    print_authors();
    kputc('\n');
    kprintf("Ketik 'help' untuk daftar perintah.\n\n");
    cli_run();
}