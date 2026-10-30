/* klib.c - fungsi dasar kernel: output, kprintf, string, memori */
#include "kernel.h"

/* ---------- output / input ---------- */
void kputc(char c) {
    vga_putc(c);
    serial_putc(c);
}

void kputs(const char* s) {
    while (*s) kputc(*s++);
}

char kgetc(void) {
    for (;;) {
        if (serial_ready())   return serial_getc();     /* terminal (serial) */
        if (keyboard_ready()) return keyboard_getc();   /* keyboard PS/2 */
        __asm__ volatile("pause");
    }
}

void kdelay_ms(uint32_t ms) {
    uint32_t chunks = (ms + 9) / 10;
    while (chunks--) {
        outb(0x61, (uint8_t)((inb(0x61) & 0xFC) | 0x01));  /* gate ON, speaker OFF */
        outb(0x43, 0xB0);                                   /* kanal 2, lo/hi, mode 0 */
        outb(0x42, 11932 & 0xFF);                           /* 1.193182 MHz x 10 ms */
        outb(0x42, 11932 >> 8);
        for (uint32_t t = 0; t < 3000000u && !(inb(0x61) & 0x20); t++) { }
    }
}
/* ---------- kprintf ---------- */
static void put_num(uint32_t v, unsigned base, int width, char pad, int neg, int upper) {
    char buf[34];
    int i = 0;
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0) buf[i++] = '0';
    while (v) { buf[i++] = digits[v % base]; v /= base; }

    int len = i + (neg ? 1 : 0);
    if (neg && pad == '0') { kputc('-'); neg = 0; }
    for (; len < width; len++) kputc(pad);
    if (neg) kputc('-');
    while (i--) kputc(buf[i]);
}

void kprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') { kputc(*fmt); continue; }
        fmt++;

        char pad = ' ';
        int width = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) put_num((uint32_t)(-(int64_t)v), 10, width, pad, 1, 0);
            else       put_num((uint32_t)v, 10, width, pad, 0, 0);
            break;
        }
        case 'u': put_num(va_arg(ap, uint32_t), 10, width, pad, 0, 0); break;
        case 'x': put_num(va_arg(ap, uint32_t), 16, width, pad, 0, 0); break;
        case 'X': put_num(va_arg(ap, uint32_t), 16, width, pad, 0, 1); break;
        case 'p': kputs("0x"); put_num((uint32_t)(uintptr_t)va_arg(ap, void*), 16, 8, '0', 0, 0); break;
        case 'c': kputc((char)va_arg(ap, int)); break;
        case 's': {
            const char* s = va_arg(ap, const char*);
            kputs(s ? s : "(null)");
            break;
        }
        case '%': kputc('%'); break;
        case '\0': va_end(ap); return;
        default: kputc('%'); kputc(*fmt); break;
        }
    }
    va_end(ap);
}

void kpanic(const char* msg) {
    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** KERNEL PANIC: %s ***\n", msg);
    for (;;) __asm__ volatile("cli; hlt");
}

/* ---------- string & memori ---------- */
size_t kstrlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

/* Cetak OS_AUTHOR satu nama per baris (dipisah di tanda ", ").
   Satu baris penuh lebih dari 80 kolom, jadi tidak muat di layar VGA. */
void print_authors(void) {
    const char* p = OS_AUTHOR;
    kputs("Dibuat oleh:\n  * ");
    while (*p) {
        if (p[0] == ',' && p[1] == ' ') { kputs("\n  * "); p += 2; }
        else kputc(*p++);
    }
    kputc('\n');
}

/* Nama memset/memcpy/... wajib ada: compiler kadang memanggilnya sendiri. */
void* memset(void* dst, int c, size_t n) {
    uint8_t* d = dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    if (d < s) { while (n--) *d++ = *s++; }
    else       { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = a;
    const uint8_t* y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return (int)*x - (int)*y;
    return 0;
}