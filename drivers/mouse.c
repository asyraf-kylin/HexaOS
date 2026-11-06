/* mouse.c - mouse PS/2 (mode polling) khusus untuk roda scroll.
   Memakai protokol IntelliMouse (paket 4 byte, byte ke-4 = roda). Gerak dan
   tombol diabaikan; roda menggulir riwayat layar. Dibaca lewat keyboard_poll(),
   karena mouse dan keyboard berbagi port 0x60/0x64. */
#include "kernel.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define LINES_PER_NOTCH 3

static int enabled   = 0;
static int has_wheel = 0;
static int pkt_len   = 3;
static int idx       = 0;
static uint8_t pkt[4];

static int wait_in(void) {                   /* tunggu input buffer controller kosong */
    for (int i = 0; i < 100000; i++) if (!(inb(KBD_STATUS) & 0x02)) return 1;
    return 0;
}
static int wait_out(void) {                  /* tunggu ada byte untuk dibaca */
    for (int i = 0; i < 100000; i++) if (inb(KBD_STATUS) & 0x01) return 1;
    return 0;
}

/* Kirim satu byte ke mouse lalu tunggu ACK (0xFA). 0 = berhasil. */
static int mouse_write(uint8_t b) {
    if (!wait_in()) return -1;
    outb(KBD_STATUS, 0xD4);                  /* "byte berikut untuk mouse" */
    if (!wait_in()) return -1;
    outb(KBD_DATA, b);
    if (!wait_out()) return -1;
    return inb(KBD_DATA) == 0xFA ? 0 : -1;
}

void mouse_init(void) {
    enabled = 0;
    if (!wait_in()) return;
    outb(KBD_STATUS, 0xA8);                  /* aktifkan port mouse */

    if (!wait_in()) return;
    outb(KBD_STATUS, 0x20);                  /* baca config controller */
    if (!wait_out()) return;
    uint8_t cfg = inb(KBD_DATA);
    cfg &= (uint8_t)~0x22;                   /* IRQ12 mati (kita polling), clock mouse hidup */
    if (!wait_in()) return;
    outb(KBD_STATUS, 0x60);
    if (!wait_in()) return;
    outb(KBD_DATA, cfg);

    if (mouse_write(0xF6)) return;           /* setelan bawaan */

    static const uint8_t magic[3] = { 200, 100, 80 };   /* urutan "aktifkan roda" */
    for (int i = 0; i < 3; i++) {
        if (mouse_write(0xF3) || mouse_write(magic[i])) return;
    }
    if (mouse_write(0xF2)) return;           /* minta ID perangkat */
    if (!wait_out()) return;
    has_wheel = (inb(KBD_DATA) == 3);        /* ID 3 = mouse dengan roda */
    pkt_len = has_wheel ? 4 : 3;

    if (mouse_write(0xF4)) return;           /* mulai kirim data */
    idx = 0;
    enabled = 1;
}

/* Dipanggil keyboard_poll() untuk tiap byte yang berasal dari mouse */
void mouse_feed(uint8_t b) {
    if (!enabled) return;
    if (idx == 0 && !(b & 0x08)) return;     /* byte pertama paket selalu punya bit 3 */
    pkt[idx++] = b;
    if (idx < pkt_len) return;
    idx = 0;

    if (!has_wheel) return;
    int z = pkt[3] & 0x0F;                   /* roda: 4 bit bertanda */
    if (z & 0x08) z -= 16;
    if (z) vga_scroll_view(-z * LINES_PER_NOTCH);   /* roda ke atas (z<0) = ke riwayat */
}