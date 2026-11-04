#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

/* ---- Info OS ---- */
#define OS_NAME     "HexaOS"
#define OS_VERSION  "1.0"
#define OS_AUTHOR   "Ghithrif Asyraf - 11251008, Ahmad Nur Farizi - 11251048, Remarsya Fredericha - 11251078, Yusuf Maulana - 11251084"

/* ---- Tipe dasar ---- */
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         i32;

/* size_t didefinisikan sendiri (bukan lewat <stddef.h>) supaya tidak
   bentrok dengan NULL di bawah */
typedef __SIZE_TYPE__ size_t;

#define NULL ((void*)0)

/* ---- Port I/O x86 (dipakai vga, serial, timer, keyboard, idt) ---- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* ---- Arsitektur x86: GDT, IDT, interrupt (arch/) ---- */
/* Susunan stack saat interrupt, harus cocok dengan arch/isr.asm */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* urutan pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;             /* didorong oleh CPU */
} registers_t;

typedef void (*isr_handler_t)(registers_t* r);

void gdt_init(void);
void gdt_set_kernel_stack(uint32_t esp0);   /* esp0 untuk TSS (user -> kernel) */
void idt_init(void);                        /* IDT + remap PIC, semua IRQ masih mask */
void isr_register(int int_no, isr_handler_t h);   /* pasang handler vektor 0-255 */
void irq_register(int irq, isr_handler_t h);      /* pasang handler IRQ 0-15 + unmask */
void irq_unregister(int irq);

static inline void interrupts_enable(void)  { __asm__ volatile("sti"); }
static inline void interrupts_disable(void) { __asm__ volatile("cli"); }

/* ---- VGA text mode (drivers/vga.c) ---- */
enum vga_color {
    VGA_BLACK, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
    VGA_BROWN, VGA_LIGHT_GREY, VGA_DARK_GREY, VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN, VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_LIGHT_MAGENTA,
    VGA_YELLOW, VGA_WHITE
};
void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_set_color(uint8_t fg, uint8_t bg);

/* ---- Serial COM1 (drivers/serial.c) ---- */
void serial_init(void);
void serial_putc(char c);
int  serial_ready(void);      /* 1 kalau ada karakter masuk */
char serial_getc(void);       /* menunggu sampai ada karakter */

/* ---- Keyboard PS/2 (drivers/keyboard.c) ---- */
void keyboard_init(void);
void keyboard_poll(void);     /* baca scancode yang menunggu ke antrean */
int  keyboard_ready(void);    /* 1 kalau ada karakter siap diambil */
char keyboard_getc(void);     /* menunggu sampai ada karakter */

/* ---- Output/input umum (kernel/src/klib.c) ---- */
void kputc(char c);           /* ke VGA + serial */
void kputs(const char* s);    /* tanpa newline otomatis */
void kprintf(const char* fmt, ...);   /* %s %c %d %u %x %p %%, lebar & 0 */
char kgetc(void);             /* baca satu karakter (polling) */
void kdelay_ms(uint32_t ms);  /* jeda (PIT kanal 2, tanpa interrupt) */
void kpanic(const char* msg);
void system_reboot(void);     /* restart (keyboard controller / triple fault) */
void system_shutdown(void);   /* matikan VM (QEMU), halt kalau tidak berhasil */
void print_logo(void);        /* kernel/src/logo.c */
void print_authors(void);     /* unjuk author*/

/* ---- String & memori dasar (tidak ada libc di kernel) ---- */
int    kstrcmp(const char* a, const char* b);
size_t kstrlen(const char* s);
void*  memset(void* dst, int c, size_t n);
void*  memcpy(void* dst, const void* src, size_t n);
void*  memmove(void* dst, const void* src, size_t n);
int    memcmp(const void* a, const void* b, size_t n);

/* ---- Init tiap modul, dipanggil dari main.c ---- */
void memory_init(void);   /* Anggota 4: memory.c */
void vfs_init(void);      /* Anggota 4: vfs.c */
void task_init(void);     /* Anggota 2: task.c */
void sync_init(void);     /* Anggota 3: sync.c */
void protect_init(void);  /* Anggota 3: protect.c */

#endif