#include "kernel.h"

void kernel_main(void) {
    vga_init();
    serial_init();
    print_logo();
    cli_run();
}
