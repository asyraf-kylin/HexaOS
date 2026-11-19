/* klib.c - fungsi dasar kernel: output, kprintf, string, memori */
#include "kernel.h"

/* ---------- output / input ---------- */
void kputc(char c) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    vga_putc(c);
    serial_putc(c);
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

void kputs(const char* s) {
    while (*s) kputc(*s++);
}

static int serial_wait(void) {
    for (uint32_t i = 0; i < 200000 && !serial_ready(); i++) { }
    return serial_ready();
}

/* Panah terminal = ESC [ A (atas) / ESC [ B (bawah). Return 0 kalau escape tak dipakai. */
static char serial_key(void) {
    char c = serial_getc();
    if (c == 0x03) kinterrupt_raise();
    if (c != 0x1B) return c;
    if (!serial_wait()) return c;
    char c2 = serial_getc();
    if (c2 != '[' && c2 != 'O') return 0;
    if (!serial_wait()) return 0;
    char c3 = serial_getc();
    while ((c3 >= '0' && c3 <= '9') || c3 == ';') {      /* mis. ESC [ 3 ~ (Delete) */
        if (!serial_wait()) return 0;
        c3 = serial_getc();
    }
    if (c3 == 'A') return KEY_UP;
    if (c3 == 'B') return KEY_DOWN;
    return 0;
}

char kgetc(void) {
    for (;;) {
        if (serial_ready()) {
            char c = serial_key();
            if (c) return c;
            continue;
        }
        if (keyboard_ready()) return keyboard_getc();
        __asm__ volatile("pause");
    }
}

/* ---------- Ctrl+C ----------
   Di prompt, Ctrl+C datang lewat kgetc() sebagai 0x03. Saat perintah berjalan,
   perintah itu sendiri memeriksa kinterrupted() di dalam loop-nya. */
static volatile int interrupt_flag = 0;

void kinterrupt_raise(void) { interrupt_flag = 1; }

int kinterrupted(void) {
    keyboard_poll();
    while (serial_ready()) {
        if (serial_getc() == 0x03) interrupt_flag = 1;   /* ketikan lain dibuang */
    }
    int r = interrupt_flag;
    interrupt_flag = 0;
    return r;
}

void kinterrupt_clear(void) {
    interrupt_flag = 0;
    keyboard_flush();
    while (serial_ready()) serial_getc();
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
/* ---------- kprintf ----------
   kprintf(): tampil di layar (VGA) DAN serial.
   kprintf_bg(): ditulis ke buffer internal, tidak ke VGA/serial -> benar-benar
   "di belakang layar", dipakai untuk jejak privilege/protect. */
typedef void (*putc_fn)(char);

static void out_putc(putc_fn out, char c) { out(c); }
static void out_puts(putc_fn out, const char* s) { while (*s) out(*s++); }

static void put_num(putc_fn out, uint32_t v, unsigned base, int width, char pad, int neg, int upper) {
    char buf[34];
    int i = 0;
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0) buf[i++] = '0';
    while (v) { buf[i++] = digits[v % base]; v /= base; }

    int len = i + (neg ? 1 : 0);
    if (neg && pad == '0') { out_putc(out, '-'); neg = 0; }
    for (; len < width; len++) out_putc(out, pad);
    if (neg) out_putc(out, '-');
    while (i--) out_putc(out, buf[i]);
}

static void kvprintf(putc_fn out, const char* fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { out_putc(out, *fmt); continue; }
        fmt++;

        char pad = ' ';
        int width = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) put_num(out, (uint32_t)(-(int64_t)v), 10, width, pad, 1, 0);
            else       put_num(out, (uint32_t)v, 10, width, pad, 0, 0);
            break;
        }
        case 'u': put_num(out, va_arg(ap, uint32_t), 10, width, pad, 0, 0); break;
        case 'x': put_num(out, va_arg(ap, uint32_t), 16, width, pad, 0, 0); break;
        case 'X': put_num(out, va_arg(ap, uint32_t), 16, width, pad, 0, 1); break;
        case 'p': out_puts(out, "0x"); put_num(out, (uint32_t)(uintptr_t)va_arg(ap, void*), 16, 8, '0', 0, 0); break;
        case 'c': out_putc(out, (char)va_arg(ap, int)); break;
        case 's': {
            const char* s = va_arg(ap, const char*);
            out_puts(out, s ? s : "(null)");
            break;
        }
        case '%': out_putc(out, '%'); break;
        case '\0': return;
        default: out_putc(out, '%'); out_putc(out, *fmt); break;
        }
    }
}

void kprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(kputc, fmt, ap);
    va_end(ap);
}

/* Log internal: ditulis ke memori, TIDAK ke VGA maupun serial. Dipakai
   untuk jejak privilege/protect yang tidak boleh mengganggu layar demo. */
static char  bg_log[2048];
static unsigned bg_log_len;

static void bg_putc(char c) {
    if (bg_log_len + 1 < sizeof(bg_log)) bg_log[bg_log_len++] = c;
}

const char* kget_bg_log(void) {
    bg_log[bg_log_len] = '\0';
    return bg_log;
}

void kprintf_bg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(bg_putc, fmt, ap);
    va_end(ap);
}

void kpanic(const char* msg) {
    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** KERNEL PANIC: %s ***\n", msg);
    for (;;) __asm__ volatile("cli; hlt");
}

/* ---------- reboot & shutdown ---------- */
void system_reboot(void) {
    interrupts_disable();

    /* Cara 1: pulse reset lewat keyboard controller (port 0x64, perintah 0xFE) */
    for (uint32_t t = 0; t < 100000u && (inb(0x64) & 0x02); t++) { }
    outb(0x64, 0xFE);

    /* Cara 2 (cadangan): IDT kosong + interrupt -> triple fault -> CPU reset */
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile("lidt %0; int3" : : "m"(null_idt));

    for (;;) __asm__ volatile("hlt");
}

void system_shutdown(void) {
    interrupts_disable();

    outw(0x604,  0x2000);   /* QEMU (ACPI PM1a) */
    outw(0xB004, 0x2000);   /* QEMU lama / Bochs */
    outw(0x4004, 0x3400);   /* VirtualBox */

    /* Tidak mati (mis. hardware asli): berhenti di sini */
    kputs("\nSafe to shut down.\n");
    for (;;) __asm__ volatile("hlt");
}

/* ---------- string & memori ---------- */
size_t kstrlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void kputs_pad(const char* s, int width) {
    int n = 0;
    while (s[n]) { kputc(s[n]); n++; }
    for (; n < width; n++) kputc(' ');
}

size_t kjoin_args(int argc, char** argv, int from, char* out, size_t max) {
    size_t n = 0;
    if (max == 0) return 0;
    for (int i = from; i < argc; i++) {
        if (i > from && n < max - 1) out[n++] = ' ';
        for (const char* p = argv[i]; *p && n < max - 1; p++) out[n++] = *p;
    }
    out[n] = '\0';
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
    kputs("Developed by:\n  * ");
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