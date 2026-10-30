/* serial.c - driver UART COM1 (115200 baud, 8N1).
   Dengan "qemu -serial stdio", ini menghubungkan OS ke terminal kamu. */
#include "kernel.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* matikan interrupt UART (kita polling) */
    outb(COM1 + 3, 0x80);   /* aktifkan DLAB untuk set baud rate */
    outb(COM1 + 0, 0x01);   /* divisor 1 -> 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8 bit, tanpa parity, 1 stop bit */
    outb(COM1 + 2, 0xC7);   /* aktifkan FIFO */
    outb(COM1 + 4, 0x0B);   /* RTS/DSR aktif */
}

void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');   /* terminal butuh CR+LF */
    for (int t = 0; t < 100000; t++)    /* batas tunggu supaya tidak hang */
        if (inb(COM1 + 5) & 0x20) break;
    outb(COM1, (uint8_t)c);
}

int serial_ready(void) {
    return inb(COM1 + 5) & 0x01;
}

char serial_getc(void) {
    while (!serial_ready())
        __asm__ volatile("pause");
    return (char)inb(COM1);
}