#include "kernel.h"

static const char* const logo_lines[] = {
    "                            #@@@@@@@@@@@@.",
    "                              #@@@@@@@@@@@@.",
    "                      .########.#@@@@@@@@@@@@.",
    "                        #@@@@@@@#.#@@@@@@@@@@@@.",
    "                          #@@@@@@@# .......@@@@@@.",
    "                            #@@@@@..@@@@@@..@@@###@.",
    "                             .@@@..@@@@@@@@..@#   #@#",
    "                            #@@@@@..@@@@@@..@@@###@.",
    "                          #@@@@@@@# .......@@@@@@.",
    "                        #@@@@@@@#.#@@@@@@@@@@@@.",
    "                      .########.#@@@@@@@@@@@@.",
    "                              #@@@@@@@@@@@@.",
    "                            #@@@@@@@@@@@@."
};

void print_logo_ascii(void) {
        kputs("\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (unsigned i = 0; i < sizeof(logo_lines) / sizeof(logo_lines[0]); i++) {
        kputs(logo_lines[i]);
        kputc('\n');
    }
    vga_set_color(VGA_RED, VGA_BLACK);
    kputc('\n');
    kprintf("%s v%s",OS_NAME, OS_VERSION);
    kputc('\n');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kputc('\n');
}