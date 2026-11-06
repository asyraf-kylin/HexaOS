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

/* 2. Baca satu baris dari keyboard/serial, dengan echo + editing */
/* Riwayat perintah (panah atas/bawah). Slot paling lama ditimpa duluan. */
#define HIST_MAX 16
static char hist[HIST_MAX][CLI_BUF_SIZE];
static int  hist_count = 0;
static int  hist_next  = 0;

static void hist_add(const char* line) {
    if (line[0] == '\0') return;
    if (hist_count > 0) {                       /* lewati kalau sama dengan yang terakhir */
        int last = (hist_next + HIST_MAX - 1) % HIST_MAX;
        if (kstrcmp(hist[last], line) == 0) return;
    }
    int i = 0;
    while (line[i] && i < CLI_BUF_SIZE - 1) { hist[hist_next][i] = line[i]; i++; }
    hist[hist_next][i] = '\0';
    hist_next = (hist_next + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}

/* Entri ke-n dari yang terbaru (n = 1 -> perintah terakhir) */
static const char* hist_get(int n) {
    return hist[(hist_next + HIST_MAX - n) % HIST_MAX];
}

/* Geser kursor layar n kolom ke kiri */
static void cursor_left(int n) {
    while (n-- > 0) kputc('\b');
}

/* Geser kursor layar n kolom ke kanan dengan menulis ulang karakter di bawahnya */
static void cursor_right(const char* buf, int from, int n) {
    while (n-- > 0) kputc(buf[from++]);
}

/* Gambar ulang dari posisi cur sampai akhir, timpa sisa dengan spasi,
   lalu kembalikan kursor ke posisi cur */
static void redraw_tail(const char* buf, int cur, int len, int extra_blank) {
    for (int k = cur; k < len; k++) kputc(buf[k]);
    for (int k = 0; k < extra_blank; k++) kputc(' ');
    cursor_left((len - cur) + extra_blank);
}

/* Hapus baris di layar, tulis ulang dengan teks baru. Kursor berakhir di ujung. */
static int replace_line(char* buf, int len, int cur, const char* text) {
    cursor_right(buf, cur, len - cur);          /* pindah ke ujung dulu */
    while (len > 0) { kputs("\b \b"); len--; }
    int i = 0;
    while (text[i] && i < CLI_BUF_SIZE - 1) { buf[i] = text[i]; kputc(text[i]); i++; }
    return i;
}

/* 2. Baca satu baris: echo, kursor kiri/kanan, Home, Backspace, Delete,
      riwayat (atas/bawah), Ctrl+C (batalkan baris) */
static void read_line(char* buf, int max) {
    char draft[CLI_BUF_SIZE];   /* ketikan belum di-Enter, disimpan saat naik ke riwayat */
    int  len = 0;               /* panjang teks di buffer */
    int  cur = 0;               /* posisi kursor 0..len */
    int  pos = 0;               /* 0 = baris baru, n = riwayat ke-n */
    draft[0] = '\0';

    for (;;) {
        char c = kgetc();

        if (c == '\r' || c == '\n') { kputc('\n'); break; }

        if (c == KEY_CTRL_C) { kputs("^C\n"); len = 0; break; }

        /* --- gerak kursor --- */
        if (c == KEY_LEFT)  { if (cur > 0)   { cur--; kputc('\b'); } continue; }
        if (c == KEY_RIGHT) { if (cur < len) { kputc(buf[cur]); cur++; } continue; }
        if (c == KEY_HOME)  { cursor_left(cur); cur = 0; continue; }

        /* --- riwayat --- */
        if (c == KEY_UP) {
            if (pos < hist_count) {
                if (pos == 0) {
                    buf[len] = '\0';
                    for (int k = 0; k <= len; k++) draft[k] = buf[k];
                }
                pos++;
                len = replace_line(buf, len, cur, hist_get(pos));
                cur = len;
            }
            continue;
        }
        if (c == KEY_DOWN) {
            if (pos > 0) {
                pos--;
                len = replace_line(buf, len, cur, pos == 0 ? draft : hist_get(pos));
                cur = len;
            }
            continue;
        }

        /* --- hapus --- */
        if (c == 8 || c == 127) {               /* Backspace: hapus karakter di kiri kursor */
            if (cur > 0) {
                for (int k = cur - 1; k < len - 1; k++) buf[k] = buf[k + 1];
                cur--; len--;
                kputc('\b');
                redraw_tail(buf, cur, len, 1);
            }
            continue;
        }
        if (c == KEY_DELETE) {                  /* Delete: hapus karakter di bawah kursor */
            if (cur < len) {
                for (int k = cur; k < len - 1; k++) buf[k] = buf[k + 1];
                len--;
                redraw_tail(buf, cur, len, 1);
            }
            continue;
        }

        if (c < ' ') continue;                  /* abaikan karakter kontrol lain */

        /* --- sisipkan karakter di posisi kursor --- */
        if (len < max - 1) {
            for (int k = len; k > cur; k--) buf[k] = buf[k - 1];
            buf[cur] = c;
            len++;
            kputc(c);                           /* tulis karakter baru, kursor maju */
            cur++;
            redraw_tail(buf, cur, len, 0);      /* tulis ulang ekor, kursor balik */
        }
    }
    buf[len] = '\0';
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
        kprintf("kylin> "); /* user */
        read_line(line, CLI_BUF_SIZE);
        hist_add(line);                   /* simpan dulu, sebelum parse merusak isi line */
        int argc = parse(line, argv);
        if (argc == 0) continue;

        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (kstrcmp(argv[0], cmds[i].name) == 0) {
                kinterrupt_clear();   /* hapus flag Ctrl+C sebelum menjalankan perintah */
                cmds[i].fn(argc, argv);
                kinterrupt_clear();
                found = 1;
                break;
            }
        }
        if (!found) kprintf("unknown command: %s\n", argv[0]);
    }
}

/* Perintah bawaan */
static void cmd_help(int argc, char** argv) {
    for (int i = 0; i < cmd_count; i++)
        kprintf("  %s - %s\n", cmds[i].name, cmds[i].help);
}
static void cmd_about(int argc, char** argv) { print_logo_unicode(); }
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
    cli_register("help",  "display this help message", cmd_help);
    cli_register("about", "display OS information and logo",  cmd_about);
    cli_register("author", "display the list of contributors", cmd_author);
    cli_register("clear", "clear the screen", cmd_clear);
    cli_register("reboot", "reboot the system", cmd_reboot);
    cli_register("shutdown", "shut down the system", cmd_shutdown);
}