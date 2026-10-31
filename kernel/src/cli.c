#include "cli.h"
#include "kernel.h"

static cli_cmd_t cmds[CLI_MAX_CMDS];
static int cmd_count = 0;

/* 1. Pendaftaran perintah (dipanggil modul lain) */
int cli_register(const char* name, const char* help, cli_handler_t fn) {
    if (cmd_count >= CLI_MAX_CMDS) return -1;
    cmds[cmd_count].name = name;
    cmds[cmd_count].help = help;
    cmds[cmd_count].fn   = fn;
    cmd_count++;
    return 0;
}

/* 2. Baca satu baris dari keyboard/serial, dengan echo + backspace */
static void read_line(char* buf, int max) {
    int i = 0;
    for (;;) {
        char c = kgetc();                 // menunggu input (dari driver)
        if (c == '\r' || c == '\n') { kputc('\n'); break; }
        if (c == 8 || c == 127) {         // backspace
            if (i > 0) { i--; kputs("\b \b"); }
            continue;
        }
        if (i < max - 1) { buf[i++] = c; kputc(c); }
    }
    buf[i] = '\0';
}

/* 3. Pecah "cat a.txt" jadi argv = {"cat", "a.txt"} */
static int parse(char* line, char** argv) {
    int argc = 0;
    while (*line && argc < CLI_MAX_ARGS) {
        while (*line == ' ') *line++ = '\0';
        if (*line == '\0') break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
    }
    return argc;
}

/* 4. Loop utama */
void cli_run(void) {
    char  line[CLI_BUF_SIZE];
    char* argv[CLI_MAX_ARGS];

    for (;;) {
        kprintf("agent> ");
        read_line(line, CLI_BUF_SIZE);
        int argc = parse(line, argv);
        if (argc == 0) continue;

        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (kstrcmp(argv[0], cmds[i].name) == 0) {
                cmds[i].fn(argc, argv);
                found = 1;
                break;
            }
        }
        if (!found) kprintf("perintah tidak dikenal: %s\n", argv[0]);
    }
}

/* Perintah bawaan */
static void cmd_help(int argc, char** argv) {
    for (int i = 0; i < cmd_count; i++)
        kprintf("  %s - %s\n", cmds[i].name, cmds[i].help);
}
static void cmd_about(int argc, char** argv) { print_logo(); }
static void cmd_clear(int argc, char** argv) { vga_clear(); }
static void cmd_author(int argc, char** argv) { print_authors(); }
static void cmd_reboot(int argc, char** argv) {
    kprintf("Rebooting...\n");
    kdelay_ms(300);
    system_reboot();
}
static void cmd_shutdown(int argc, char** argv) {
    kprintf("Shutting down...\n");
    kdelay_ms(300);
    system_shutdown();
}

void cli_init(void) {
    cli_register("help",  "daftar perintah", cmd_help);
    cli_register("about", "info & logo OS",  cmd_about);
    cli_register("clear", "bersihkan layar", cmd_clear);
    cli_register("author", "daftar anggota tim", cmd_author);
    cli_register("reboot", "restart sistem", cmd_reboot);
    cli_register("shutdown", "matikan sistem", cmd_shutdown);
}
