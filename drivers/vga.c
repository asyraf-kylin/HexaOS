/* vga.c - driver layar teks VGA 80x25 (memori video di 0xB8000)
   + scrollback: semua baris disimpan di buffer lingkar supaya bisa digulir
   ke atas (PageUp / roda mouse) untuk melihat riwayat output. */
#include "kernel.h"

#define VGA_W    80
#define VGA_H    25
#define BUF_ROWS 400                 /* 25 baris layar + 375 baris riwayat (64 KB) */

static volatile uint16_t* const vga_mem = (volatile uint16_t*)0xB8000;
static uint16_t rows[BUF_ROWS][VGA_W];   /* isi teks + warna tiap sel */
static int top  = 0;                 /* baris pertama layar "live" di buffer lingkar */
static int hist = 0;                 /* jumlah baris riwayat di atas layar live */
static int view = 0;                 /* 0 = layar live, n = tergulir n baris ke atas */
static int cx = 0, cy = 0;
static uint8_t color = (VGA_BLACK << 4) | VGA_LIGHT_GREY;

/* Baris ke-r relatif terhadap layar live (r = 0..24 layar, r < 0 riwayat) */
#define ROW(r) rows[(top + (r) + BUF_ROWS) % BUF_ROWS]

static inline uint16_t cell(char c) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void blank_row(uint16_t* row) {
    for (int x = 0; x < VGA_W; x++) row[x] = cell(' ');
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cy * VGA_W + cx);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    outb(0x3D4, 0x0A);                       /* sembunyikan kursor saat melihat riwayat */
    uint8_t v = inb(0x3D5);
    outb(0x3D5, view ? (uint8_t)(v | 0x20) : (uint8_t)(v & ~0x20));
}

/* Gambar ulang seluruh layar dari buffer sesuai posisi gulir */
static void render(void) {
    for (int y = 0; y < VGA_H; y++) {
        const uint16_t* src = ROW(y - view);
        for (int x = 0; x < VGA_W; x++) vga_mem[y * VGA_W + x] = src[x];
    }
    if (view) {                              /* penanda di pojok kanan atas */
        char tag[24];
        int n = 0;
        const char* a = " RIWAYAT -";
        while (*a) tag[n++] = *a++;
        char d[8]; int dn = 0; int v = view;
        while (v) { d[dn++] = (char)('0' + v % 10); v /= 10; }
        while (dn) tag[n++] = d[--dn];
        tag[n++] = ' ';
        for (int i = 0; i < n; i++)
            vga_mem[VGA_W - n + i] = (uint16_t)(uint8_t)tag[i] | (0x70 << 8);   /* hitam di atas abu-abu */
    }
}

static void scroll(void) {
    top = (top + 1) % BUF_ROWS;              /* baris paling atas jadi riwayat */
    blank_row(ROW(VGA_H - 1));
    if (hist < BUF_ROWS - VGA_H) hist++;
    cy = VGA_H - 1;
    render();
}

static inline uint32_t irq_off(void) {
    uint32_t f;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(f) : : "memory");
    return f;
}
static inline void irq_back(uint32_t f) {
    __asm__ volatile("push %0; popf" : : "r"(f) : "memory", "cc");
}

/* Gulir tampilan: delta > 0 ke atas (lebih lama), delta < 0 ke bawah */
void vga_scroll_view(int delta) {
    uint32_t f = irq_off();
    int nv = view + delta;
    if (nv < 0) nv = 0;
    if (nv > hist) nv = hist;
    if (nv != view) { view = nv; render(); update_cursor(); }
    irq_back(f);
}

void vga_scroll_reset(void) {
    vga_scroll_view(-view);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_clear(void) {
    for (int r = 0; r < VGA_H; r++) blank_row(ROW(r));
    hist = 0;                                /* clear juga menghapus riwayat */
    view = 0;
    cx = cy = 0;
    render();
    update_cursor();
}

void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_putc(char c) {
    if (view) { view = 0; render(); }        /* ada output baru: kembali ke layar live */
    switch (c) {
    case '\n': cx = 0; cy++; break;
    case '\r': cx = 0; break;
    case '\b':
        if (cx > 0) cx--;
        else if (cy > 0) { cy--; cx = VGA_W - 1; }
        break;
    case '\t': cx = (cx + 8) & ~7; break;
    default:
        ROW(cy)[cx] = cell(c);
        vga_mem[cy * VGA_W + cx] = cell(c);
        cx++;
    }
    if (cx >= VGA_W) { cx = 0; cy++; }
    if (cy >= VGA_H) scroll();
    update_cursor();
}