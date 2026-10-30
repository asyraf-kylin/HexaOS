#include "kernel.h"

static const char* const logo_lines[] = {
       "#.                    .#",
       "@@#.                .#@@",
       "@@@@#.            .#@@@@",
       "@@@@@@#.        .#@@@@@@    _   _ _______  __    _       ___  ____",
       "@@@@@@@@#.    .#@@@@@@@@   | | | | ____\\ \\/ /   / \\     / _ \\/ ___|",
       "@@@@@@@@@@#..#@@@@@@@@@@   | |_| |  _|  \\  /   / _ \\   | | | \\___ \\",
       ".#@@@@@@@@@@@@@@@@@@@@#.   |  _  | |___ /  \\  / ___ \\  | |_| |___) |",
       "  .#@@@@@@@@@@@@@@@@#.     |_| |_|_____/_/\\_\\/_/   \\_\\  \\___/|____/",
       "    .#@@@@@@@@@@@@#.",
       "      .#@@@@@@@@#.",
       "        .#@@@@#.",
       "          .##."
};

void print_logo(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    for (unsigned i = 0; i < sizeof(logo_lines) / sizeof(logo_lines[0]); i++) {
        kputs(logo_lines[i]);
        kputc('\n');
    }
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    kputc('\n');
    kprintf("  %s v%s - Tugas Besar Sistem Operasi\n", OS_NAME, OS_VERSION);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputc('\n');
}