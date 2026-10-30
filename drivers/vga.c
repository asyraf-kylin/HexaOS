/* vga.c - driver layar teks VGA 80x25 (memori video di 0xB8000) */
#include "kernel.h"

#define VGA_W 80
#define VGA_H 25

static volatile uint16_t* const vga_mem = (volatile uint16_t*)0xB8000;
static int cx = 0, cy = 0;
static uint8_t color = (VGA_BLACK << 4) | VGA_LIGHT_GREY;

static inline uint16_t cell(char c) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cy * VGA_W + cx);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    for (int y = 1; y < VGA_H; y++)
        for (int x = 0; x < VGA_W; x++)
            vga_mem[(y - 1) * VGA_W + x] = vga_mem[y * VGA_W + x];
    for (int x = 0; x < VGA_W; x++)
        vga_mem[(VGA_H - 1) * VGA_W + x] = cell(' ');
    cy = VGA_H - 1;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++)
        vga_mem[i] = cell(' ');
    cx = cy = 0;
    update_cursor();
}

void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_putc(char c) {
    switch (c) {
    case '\n': cx = 0; cy++; break;
    case '\r': cx = 0; break;
    case '\b':
        if (cx > 0) cx--;
        else if (cy > 0) { cy--; cx = VGA_W - 1; }
        break;
    case '\t': cx = (cx + 8) & ~7; break;
    default:
        vga_mem[cy * VGA_W + cx] = cell(c);
        cx++;
    }
    if (cx >= VGA_W) { cx = 0; cy++; }
    if (cy >= VGA_H) scroll();
    update_cursor();
}