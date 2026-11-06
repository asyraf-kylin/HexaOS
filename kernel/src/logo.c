/* logo.c - logo HexaOS (putih, dan merah).
   Logo ditulis sebagai teks Unicode biasa (UTF-8) supaya mudah diedit, tapi
   layar VGA text mode memakai CP437 dan terminal serial memakai UTF-8.
   Jadi tiap karakter diterjemahkan: VGA dapat byte CP437, serial dapat UTF-8
   asli + kode warna ANSI.

   Penanda warna di dalam string:  BLU = biru, WHT = putih, RED = merah.
   Karakter Unicode yang didukung ada di tabel cp437_map (blok dan garis).  */
#include "kernel.h"

#define BLU "\x01"
#define WHT "\x02"
#define RED "\x03"

static const char* const logo_lines[] = {
    "     " WHT "▀███████████▄" "\n",
    "       " WHT "▀███████████▄" "\n",
    " " WHT "▄▄▄▄▄▄▄ ▀██████████▄" "\n",
    "  " WHT "███████▄ ▀██████████▄      ██" BLU "╗  " WHT "██" BLU "╗" WHT "███████" BLU "╗" WHT "██" BLU "╗  " WHT "██" BLU "╗ " WHT "█████" BLU "╗   " RED "██████" BLU "╗ " RED "███████" BLU "╗" "\n",
    "   " WHT "▀███████▄ " RED "▄▄▄▄ " WHT "▀█████▄    ██" BLU "║  " WHT "██" BLU "║" WHT "██" BLU "╔════╝╚" WHT "██" BLU "╗" WHT "██" BLU "╔╝" WHT "██" BLU "╔══" WHT "██" BLU "╗ " RED "██" BLU "╔═══" RED "██" BLU "╗" RED "██" BLU "╔════╝" "\n",
    "     " WHT "▀█████ " RED "██████ " WHT "▀██▀▀▀█▄  ███████" BLU "║" WHT "█████" BLU "╗   ╚" WHT "███" BLU "╔╝ " WHT "███████" BLU "║ " RED "██" BLU "║   " RED "██" BLU "║" RED "███████" BLU "╗" "\n",
    "       " WHT "███ " RED "████████ " WHT "██   ███ ██" BLU "╔══" WHT "██" BLU "║" WHT "██" BLU "╔══╝   " WHT "██" BLU "╔" WHT "██" BLU "╗ " WHT "██" BLU "╔══" WHT "██" BLU "║ " RED "██" BLU "║   " RED "██" BLU "║╚════" RED "██" BLU "║" "\n",
    "     " WHT "▄█████ " RED "██████ " WHT "▄██▄▄▄█▀  ██" BLU "║  " WHT "██" BLU "║" WHT "███████" BLU "╗" WHT "██" BLU "╔╝ " WHT "██" BLU "╗" WHT "██" BLU "║  " WHT "██" BLU "║ ╚" RED "██████" BLU "╔╝" RED "███████" BLU "║" "\n",
    "   " WHT "▄███████▀ " RED "▀▀▀▀ " WHT "▄█████▀    " BLU "╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝  ╚═════╝ ╚══════╝" "\n",
    "  " WHT "▄██████▀ ▄██████████▀" "\n",
    " " WHT "▀▀▀▀▀▀▀ ▄██████████▀" "\n",
    "       " WHT "▄███████████▀" "\n",
    "     " WHT "▄███████████▀" "\n",
};

/* Unicode -> CP437 (hanya yang dipakai untuk menggambar) */
static const struct { uint16_t cp; uint8_t cp437; } cp437_map[] = {
    { 0x2588, 0xDB },  /* █ */   { 0x2580, 0xDF },  /* ▀ */
    { 0x2584, 0xDC },  /* ▄ */   { 0x258C, 0xDD },  /* ▌ */
    { 0x2590, 0xDE },  /* ▐ */   { 0x2591, 0xB0 },  /* ░ */
    { 0x2592, 0xB1 },  /* ▒ */   { 0x2593, 0xB2 },  /* ▓ */
    { 0x2550, 0xCD },  /* ═ */   { 0x2551, 0xBA },  /* ║ */
    { 0x2554, 0xC9 },  /* ╔ */   { 0x2557, 0xBB },  /* ╗ */
    { 0x255A, 0xC8 },  /* ╚ */   { 0x255D, 0xBC },  /* ╝ */
    { 0x2500, 0xC4 },  /* ─ */   { 0x2502, 0xB3 },  /* │ */
    { 0x25A0, 0xFE },  /* ■ */
};

static uint8_t to_cp437(uint32_t cp) {
    for (unsigned i = 0; i < sizeof(cp437_map) / sizeof(cp437_map[0]); i++)
        if (cp437_map[i].cp == cp) return cp437_map[i].cp437;
    return '?';
}

static void serial_puts(const char* s) {
    while (*s) serial_putc(*s++);
}

static void logo_color(char tag) {
    switch (tag) {
    case 1: vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK); serial_puts("\033[94m"); break;
    case 2: vga_set_color(VGA_WHITE,      VGA_BLACK); serial_puts("\033[97m"); break;
    case 3: vga_set_color(VGA_LIGHT_RED,  VGA_BLACK); serial_puts("\033[91m"); break;
    }
}

/* Cetak satu string logo: VGA dapat CP437, serial dapat UTF-8 aslinya */
static void logo_puts(const char* s) {
    while (*s) {
        uint8_t b = (uint8_t)*s;
        if (b >= 1 && b <= 3) { logo_color((char)b); s++; continue; }

        if (b < 0x80) {                         /* ASCII biasa */
            vga_putc((char)b); serial_putc((char)b); s++; continue;
        }

        uint32_t cp; int n;                     /* UTF-8 -> codepoint */
        if      ((b & 0xE0) == 0xC0) { cp = b & 0x1F; n = 2; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; n = 3; }
        else                         { cp = b & 0x07; n = 4; }
        for (int i = 1; i < n && s[i]; i++) cp = (cp << 6) | ((uint8_t)s[i] & 0x3F);

        vga_putc((char)to_cp437(cp));
        for (int i = 0; i < n && *s; i++) serial_putc(*s++);
    }
}

void print_logo(void) {
    kputs("\n");
    for (unsigned i = 0; i < sizeof(logo_lines) / sizeof(logo_lines[0]); i++)
        logo_puts(logo_lines[i]);

    kputc('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    serial_puts("\033[97m");
    kprintf("  %s v%s - Final Project of Operating Systems Course at ITK\n", OS_NAME, OS_VERSION);

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);   /* kembalikan warna normal */
    serial_puts("\033[0m");
    kputc('\n');
}