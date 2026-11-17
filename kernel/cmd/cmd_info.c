#include "cli.h"
#include "kernel.h"

static void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    kprintf("Perintah:\n");
    for (int i = 0; i < cli_count(); i++) {
        const cli_cmd_t* c = cli_get(i);
        if (!c->root_only) { kputs("  "); kputs_pad(c->name, 10); kprintf("%s\n", c->help); }
    }
    if (cli_is_root()) {
        kprintf("\nPerintah admin:\n");
        for (int i = 0; i < cli_count(); i++) {
            const cli_cmd_t* c = cli_get(i);
            if (c->root_only) { kputs("  "); kputs_pad(c->name, 10); kprintf("%s\n", c->help); }
        }
    } else {
        kprintf("\nFitur admin (edit, hapus, diagnostik) tersedia setelah 'login'.\n");
    }
}

static void cmd_about(int argc, char** argv)  { (void)argc; (void)argv; print_logo_unicode(); }
static void cmd_author(int argc, char** argv) { (void)argc; (void)argv; print_authors(); }

void cmd_info_init(void) {
    cli_register("help",  "tampilkan daftar perintah", cmd_help);
    cli_register("about", "info OS dan logo",          cmd_about);
    cli_register("dev",   "daftar kontributor",        cmd_author);
}