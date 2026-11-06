/* cmd_info.c - perintah informasi: help, about, author */
#include "cli.h"
#include "kernel.h"

static void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    for (int pass = 1; pass >= 0; pass--) {          /* 1 = inti, 0 = modul lain */
        for (int i = 0; i < cli_count(); i++) {
            const cli_cmd_t* c = cli_get(i);
            if (c->core == pass)
                kprintf("  %s - %s\n", c->name, c->help);
        }
    }
}

static void cmd_about(int argc, char** argv) {
    (void)argc; (void)argv;
    print_logo_unicode();
}

static void cmd_author(int argc, char** argv) {
    (void)argc; (void)argv;
    print_authors();
}

void cmd_info_init(void) {
    cli_register_core("help",   "display this help message",        cmd_help);
    cli_register_core("about",  "display OS information and logo",  cmd_about);
    cli_register_core("dev", "display the list of contributors", cmd_author);
}