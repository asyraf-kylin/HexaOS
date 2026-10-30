/* keyboard.c - driver keyboard PS/2 (scancode set 1, layout US), mode polling.
   Tidak butuh IDT/interrupt: kgetc() memanggil keyboard_ready() berulang kali.
   Kalau nanti IRQ1 sudah ada (idt.c), handler cukup memanggil keyboard_poll(). */
#include "kernel.h"

#define KBD_DATA    0x60
#define KBD_STATUS  0x64
#define KBD_OBF     0x01   /* output buffer penuh: ada byte siap dibaca */
#define KBD_AUX     0x20   /* byte itu berasal dari mouse, bukan keyboard */

#define KBD_BUF_SIZE 64    /* antrean karakter (ring buffer) */

/* Tabel scancode -> ASCII. Indeks = scancode, nilai 0 = tidak ada karakter. */
static const char map_normal[] =
    "\0\0" "1234567890-=" "\b" "\t"
    "qwertyuiop[]" "\n" "\0"
    "asdfghjkl;'" "`" "\0" "\\"
    "zxcvbnm,./" "\0" "*" "\0" " ";

static const char map_shift[] =
    "\0\0" "!@#$%^&*()_+" "\b" "\t"
    "QWERTYUIOP{}" "\n" "\0"
    "ASDFGHJKL:\"" "~" "\0" "|"
    "ZXCVBNM<>?" "\0" "*" "\0" " ";

static int shift_down = 0;
static int caps_on    = 0;
static int extended   = 0;   /* byte sebelumnya 0xE0 (tombol extended) */

static char buf[KBD_BUF_SIZE];
static int  head = 0, tail = 0;

/* Ubah satu scancode jadi karakter. Return 0 kalau tidak menghasilkan karakter
   (tombol modifier, tombol dilepas, panah, dll). */
static char kbd_decode(uint8_t sc) {
    if (sc == 0xE0) { extended = 1; return 0; }

    int release = sc & 0x80;
    sc &= 0x7F;

    if (extended) {                       /* tombol extended: abaikan, kecuali 2 ini */
        extended = 0;
        if (release) return 0;
        if (sc == 0x1C) return '\n';      /* Enter di numpad */
        if (sc == 0x35) return '/';       /* '/' di numpad */
        return 0;                         /* panah, Home, Delete, dst. */
    }

    if (sc == 0x2A || sc == 0x36) { shift_down = !release; return 0; }
    if (release) return 0;
    if (sc == 0x3A) { caps_on = !caps_on; return 0; }

    if (sc >= sizeof(map_normal) - 1) return 0;

    char base = map_normal[sc];
    char c;
    if (base >= 'a' && base <= 'z')       /* huruf: Shift XOR CapsLock */
        c = (shift_down ^ caps_on) ? (char)(base - 'a' + 'A') : base;
    else
        c = shift_down ? map_shift[sc] : base;

    if (c == '\t') c = ' ';               /* Tab dijadikan spasi */
    return c;
}

static void buf_push(char c) {
    int next = (head + 1) % KBD_BUF_SIZE;
    if (next == tail) return;             /* penuh: buang karakter */
    buf[head] = c;
    head = next;
}

/* Baca semua scancode yang menunggu, simpan karakternya ke antrean. */
void keyboard_poll(void) {
    while (inb(KBD_STATUS) & KBD_OBF) {
        int from_mouse = inb(KBD_STATUS) & KBD_AUX;
        uint8_t sc = inb(KBD_DATA);
        if (from_mouse) continue;
        char c = kbd_decode(sc);
        if (c) buf_push(c);
    }
}

void keyboard_init(void) {
    while (inb(KBD_STATUS) & KBD_OBF) inb(KBD_DATA);   /* kosongkan sisa byte boot */
    shift_down = caps_on = extended = 0;
    head = tail = 0;
}

/* 1 kalau ada karakter siap diambil dengan keyboard_getc() */
int keyboard_ready(void) {
    keyboard_poll();
    return head != tail;
}

/* Ambil satu karakter (menunggu sampai ada). */
char keyboard_getc(void) {
    while (!keyboard_ready())
        __asm__ volatile("pause");
    char c = buf[tail];
    tail = (tail + 1) % KBD_BUF_SIZE;
    return c;
}