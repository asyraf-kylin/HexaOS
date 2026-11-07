#include "cli.h"
#include "kernel.h"

static cli_cmd_t cmds[CLI_MAX_CMDS];
static int cmd_count = 0;

/* 1. Pendaftaran perintah (dipanggil modul lain) */
static int do_register(const char* name, const char* help, cli_handler_t fn, int core, int max_args) {
    if (cmd_count >= CLI_MAX_CMDS) return -1;
    cmds[cmd_count].name = name;
    cmds[cmd_count].help = help;
    cmds[cmd_count].fn   = fn;
    cmds[cmd_count].core = core;
    cmds[cmd_count].max_args = max_args;
    cmd_count++;
    return 0;
}

int cli_register(const char* name, const char* help, cli_handler_t fn) {
    return do_register(name, help, fn, 0, 0);          /* tanpa argumen */
}

int cli_register_core(const char* name, const char* help, cli_handler_t fn) {
    return do_register(name, help, fn, 1, 0);          /* tanpa argumen */
}

int cli_register_args(const char* name, const char* help, cli_handler_t fn, int max_args) {
    return do_register(name, help, fn, 0, max_args);
}

int cli_count(void) { return cmd_count; }

const cli_cmd_t* cli_get(int i) {
    return (i >= 0 && i < cmd_count) ? &cmds[i] : NULL;
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

/* 3. Pecah "cat a.txt" jadi argv = {"cat", "a.txt"}.
      Return jumlah token, atau -1 kalau token melebihi CLI_MAX_ARGS. */
static int parse(char* line, char** argv) {
    int argc = 0;
    for (;;) {
        while (*line == ' ') *line++ = '\0';
        if (*line == '\0') break;
        if (argc >= CLI_MAX_ARGS) return -1;
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
        if (argc < 0) { kprintf("too many arguments (max %d words)\n", CLI_MAX_ARGS); continue; }

        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (kstrcmp(argv[0], cmds[i].name) == 0) {
                int max = cmds[i].max_args;
                if (max >= 0 && argc - 1 > max) {          /* argumen berlebih: tolak, jangan jalankan */
                    if (max == 0) kprintf("%s: this command takes no arguments\n", argv[0]);
                    else          kprintf("%s: too many arguments (max %d)\n", argv[0], max);
                    found = 1;
                    break;
                }
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

void cli_init(void) {
    cmd_info_init();
    cmd_system_init();
}