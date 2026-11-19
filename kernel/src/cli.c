#include "cli.h"
#include "kernel.h"
#include "protect.h"
#include "task.h"
#include "memory.h"

/* =====================================================================
 * PENANDA DEMO BAB III - PDF poin 1 (boot/CLI) dan poin 8 (hak akses)
 * ---------------------------------------------------------------------
 * SATU-SATUNYA pintu cek hak akses ada di cli_run(): baris
 * `if (cmds[i].root_only && !shell_root) ...` -> "Akses Ditolak".
 * [JANGAN] menyebar cek shell_root ke cmd_*.c. Perintah admin baru cukup
 *   didaftarkan dengan cli_register_root() (contoh: edit/del/purge/diag).
 *
 * [DEMO-POIN1] `help`, `add`, `list` = 3 perintah buatan kelompok minimal.
 * [DEMO-POIN8] `del 1` sebagai guest -> ditolak; `login` -> prompt admin;
 *   lalu `del 1` berhasil. Itu bukti "diizinkan vs ditolak".
 * [DEMO-RR] HUD (Tab saat admin / `hud`) menampilkan CPU & GILIRAN thread
 *   secara live -> bukti visual context switch tanpa perlu diag.
 * [UBAH-SESUAIPDF] Prompt sekarang `kiosk@hexa>`; task board meminta
 *   `guest@hexa>` / `admin@hexa#`. Ini kosmetik, ubah di bawah bila perlu.
 * [JANGAN] menghapus print_logo_unicode() di main.c: PDF wajib logo ASCII.
 * ===================================================================== */

/* Mode root shell: 0 = user biasa, 1 = root */
static int shell_root = 0;

int cli_is_root(void) { return shell_root; }

static cli_cmd_t cmds[CLI_MAX_CMDS];
static int cmd_count = 0;

/* 1. Pendaftaran perintah (dipanggil modul lain) */
static int do_register(const char* name, const char* help, cli_handler_t fn,
                       int core, int max_args, int root_only) {
    if (cmd_count >= CLI_MAX_CMDS) return -1;
    cmds[cmd_count].name = name;
    cmds[cmd_count].help = help;
    cmds[cmd_count].fn   = fn;
    cmds[cmd_count].core = core;
    cmds[cmd_count].max_args = max_args;
    cmds[cmd_count].root_only = root_only;
    cmd_count++;
    return 0;
}

int cli_register(const char* name, const char* help, cli_handler_t fn) {
    return do_register(name, help, fn, 0, 0, 0);          /* tanpa argumen */
}

int cli_register_core(const char* name, const char* help, cli_handler_t fn) {
    return do_register(name, help, fn, 1, 0, 0);          /* tanpa argumen */
}

int cli_register_args(const char* name, const char* help, cli_handler_t fn, int max_args) {
    return do_register(name, help, fn, 0, max_args, 0);
}

int cli_register_root(const char* name, const char* help, cli_handler_t fn, int max_args) {
    return do_register(name, help, fn, 0, max_args, 1);
}

int cli_count(void) { return cmd_count; }

const cli_cmd_t* cli_get(int i) {
    return (i >= 0 && i < cmd_count) ? &cmds[i] : NULL;
}

/* -------------------------------------------------------------
 * Task Monitor HUD (Panel kanan penuh tinggi layar, khusus Root)
 * ------------------------------------------------------------- */
#define HUD_W 36
#define HUD_H 25                          /* mentok atas & bawah, tanpa celah */
#define HUD_TASK_ROWS (HUD_H - 5)         /* header + kolom + N baris + garis + heap + footer */
#define HUD_X (80 - HUD_W)
#define HUD_Y 0

static int hud_active = 0;

static const char* hud_state_name(task_state_t s) {
    switch (s) {
    case TASK_NEW:        return "NEW";
    case TASK_READY:      return "RDY";
    case TASK_RUNNING:    return "RUN";
    case TASK_WAITING:    return "WAIT";
    case TASK_TERMINATED: return "TERM";
    default:              return "-";
    }
}

static void hud_pad(char* dst, const char* src, int width) {
    int i = 0;
    while (src[i] && i < width) { dst[i] = src[i]; i++; }
    for (; i < width; i++) dst[i] = ' ';
    dst[width] = '\0';
}

static void hud_fmt_u(uint32_t val, char* buf, int max) {
    if (max <= 1) { if (max == 1) buf[0] = '\0'; return; }
    char tmp[16];
    int t = 0;
    if (val == 0) tmp[t++] = '0';
    else {
        while (val > 0 && t < 15) {
            tmp[t++] = '0' + (val % 10);
            val /= 10;
        }
    }
    int b = 0;
    while (t > 0 && b < max - 1) buf[b++] = tmp[--t];
    buf[b] = '\0';
}

static void hud_render(void) {
    if (!hud_active || !shell_root) return;

    uint8_t border_attr = (VGA_BLUE << 4) | VGA_WHITE;
    uint8_t text_attr   = (VGA_BLUE << 4) | VGA_LIGHT_CYAN;
    uint8_t val_attr    = (VGA_BLUE << 4) | VGA_YELLOW;
    uint8_t stat_attr   = (VGA_BLUE << 4) | VGA_WHITE;

    char line[HUD_W + 1];

    // Baris 0: Header
    for (int i = 0; i < HUD_W; i++) line[i] = ' ';
    const char* title = " [TASK & SYS MONITOR] ";
    int tlen = kstrlen(title);
    int toff = (HUD_W - tlen) / 2;
    for (int i = 0; i < tlen && (toff + i) < HUD_W; i++) line[toff + i] = title[i];
    line[HUD_W] = '\0';
    vga_write_overlay(HUD_X, HUD_Y + 0, line, border_attr);

    // Baris 1: Kolom Header Tabel Thread
    for (int i = 0; i < HUD_W; i++) line[i] = ' ';
    const char* col_hdr = " ID  NAME        STATE   CPU    SW";
    for (int i = 0; col_hdr[i] && i < HUD_W; i++) line[i] = col_hdr[i];
    line[HUD_W] = '\0';
    vga_write_overlay(HUD_X, HUD_Y + 1, line, val_attr);

    // Baris 2..: Daftar Task / Thread (semua slot)
    for (int row = 0; row < HUD_TASK_ROWS; row++) {
        for (int i = 0; i < HUD_W; i++) line[i] = ' ';
        line[HUD_W] = '\0';

        uint32_t tid = 0, cpu = 0, sw = 0;
        char name[16];
        task_state_t state = TASK_UNUSED;
        if (task_get_info(row, &tid, name, sizeof(name), &state, &cpu, &sw)) {
            char num_buf[16];
            // ID
            hud_fmt_u(tid, num_buf, sizeof(num_buf));
            line[1] = ' '; line[2] = (num_buf[1]) ? num_buf[0] : ' ';
            line[3] = (num_buf[1]) ? num_buf[1] : num_buf[0];

            // Name (8 char)
            char npad[9];
            hud_pad(npad, name, 8);
            for (int k = 0; k < 8; k++) line[5 + k] = npad[k];

            // State (4 char)
            char spad[5];
            hud_pad(spad, hud_state_name(state), 4);
            for (int k = 0; k < 4; k++) line[17 + k] = spad[k];

            // CPU ticks (5 char)
            char cpubuf[16], cpad[6];
            hud_fmt_u(cpu, cpubuf, sizeof(cpubuf));
            hud_pad(cpad, cpubuf, 5);
            for (int k = 0; k < 5; k++) line[24 + k] = cpad[k];

            // Switches (4 char)
            char swbuf[16], swpad[5];
            hud_fmt_u(sw, swbuf, sizeof(swbuf));
            hud_pad(swpad, swbuf, 4);
            for (int k = 0; k < 4; k++) line[31 + k] = swpad[k];

            vga_write_overlay(HUD_X, HUD_Y + 2 + row, line, text_attr);
        } else {
            const char* empty_slot = "  -  (slot kosong)                 ";
            for (int k = 0; empty_slot[k] && k < HUD_W; k++) line[k] = empty_slot[k];
            vga_write_overlay(HUD_X, HUD_Y + 2 + row, line, (VGA_BLUE << 4) | VGA_DARK_GREY);
        }
    }

    // Garis Pemisah
    for (int i = 0; i < HUD_W; i++) line[i] = '-';
    line[HUD_W] = '\0';
    vga_write_overlay(HUD_X, HUD_Y + 2 + HUD_TASK_ROWS, line, border_attr);

    // Info Heap
    for (int i = 0; i < HUD_W; i++) line[i] = ' ';
    const char* hlabel = " Heap: ";
    int pos = 0;
    while (hlabel[pos]) { line[pos] = hlabel[pos]; pos++; }
    char ubuf[16], fbuf[16];
    hud_fmt_u((uint32_t)memory_get_used(), ubuf, sizeof(ubuf));
    hud_fmt_u((uint32_t)memory_get_free(), fbuf, sizeof(fbuf));
    int u = 0; while (ubuf[u] && pos < HUD_W) line[pos++] = ubuf[u++];
    const char* mid = "B used | ";
    int m = 0; while (mid[m] && pos < HUD_W) line[pos++] = mid[m++];
    int f = 0; while (fbuf[f] && pos < HUD_W) line[pos++] = fbuf[f++];
    const char* end = "B free";
    int e = 0; while (end[e] && pos < HUD_W) line[pos++] = end[e++];
    line[HUD_W] = '\0';
    vga_write_overlay(HUD_X, HUD_Y + 3 + HUD_TASK_ROWS, line, stat_attr);

    // Footer / Petunjuk
    for (int i = 0; i < HUD_W; i++) line[i] = ' ';
    const char* foot = " [Tab]: Sembunyikan | Priv: ROOT ";
    for (int i = 0; foot[i] && i < HUD_W; i++) line[i] = foot[i];
    line[HUD_W] = '\0';
    vga_write_overlay(HUD_X, HUD_Y + 4 + HUD_TASK_ROWS, line, val_attr);
}

void cli_hud_toggle(void) {
    if (!shell_root) return;
    hud_active = !hud_active;
    if (hud_active) {
        vga_overlay_set(HUD_X, HUD_Y, HUD_W, HUD_H);
        vga_redraw();          /* bersihkan sisa overlay lama */
        hud_render();          /* isi snapshot overlay lewat vga_write_overlay */
    } else {
        vga_overlay_clear();
        vga_redraw();          /* satu-satunya jalan keluar: Tab ditekan lagi */
    }
}

/* Dipanggil dari IRQ0 (interrupt mati) beberapa kali per detik supaya data HUD
   tetap hidup selagi perintah berjalan, bukan hanya saat prompt kembali. */
void cli_hud_refresh(void) {
    if (hud_active && shell_root) hud_render();
}

int cli_hud_is_active(void) {
    return hud_active;
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

        if (c == KEY_TAB) {
            if (shell_root) {
                cli_hud_toggle();
            }
            continue;
        }

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

/* 2b. Baca password: karakter diganti '*', tidak disimpan ke riwayat */
void cli_read_password(char* buf, int max) {
    int len = 0;
    for (;;) {
        char c = kgetc();
        if (c == '\r' || c == '\n') { kputc('\n'); break; }
        if (c == KEY_CTRL_C) { kputs("^C\n"); len = 0; break; }
        if ((c == 8 || c == 127) && len > 0) {
            len--;
            kputs("\b \b");
            continue;
        }
        if (c < ' ') continue;
        if (len < max - 1) {
            buf[len++] = c;
            kputc('*');
        }
    }
    buf[len] = '\0';
}

// 3) Hapus cmd_su, cmd_exit_root, cmd_passwd lama. Ganti dengan:
static void cmd_login(int argc, char** argv) {
    if (shell_root) { kprintf("Sudah login sebagai admin.\n"); return; }
    if (argc == 2 && kstrcmp(argv[1], "admin") != 0) {
        kprintf("login: pengguna '%s' tidak dikenal. Gunakan: login admin\n", argv[1]);
        return;
    }
    char pw[32];
    kprintf("Password admin: ");
    cli_read_password(pw, 32);
    if (pw[0] == '\0') return;
    if (protect_check_root_password(pw) == E_OK) {
        /* Masih berstatus user: jalankan kode di CPL=3 untuk membuktikan
           privilege hardware, lalu #GP mengembalikan ke CPL=0. Baru setelah
           itu hak admin (label PRIV_KERNEL) diberikan. */
        user_switch_ring3();
        protect_grant_root();
        shell_root = 1;
        kprintf("Login berhasil. Anda telah menjadi root.\n");
        kprintf("Fitur admin aktif, ketik 'help' untuk daftar perintah.\n");
    } else {
        kprintf("login: password salah.\n");
    }
}

static void cmd_logout(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!shell_root) { kprintf("Anda sudah dalam mode kiosk.\n"); return; }
    if (hud_active) {
        hud_active = 0;
        vga_overlay_clear();
        vga_redraw();
    }
    protect_drop_root();
    user_switch_ring3();   /* transisi privilege nyata saat kembali ke user */
    shell_root = 0;
    kprintf("Logout berhasil. Anda telah kembali ke mode kiosk.\n");
}

/* passwd sekarang root_only, jadi cek "if (!shell_root)" di dalamnya dihapus */
static void cmd_passwd(int argc, char** argv) {
    (void)argc; (void)argv;
    char old_pw[32], new_pw[32], confirm[32];
    kprintf("Password lama: ");       cli_read_password(old_pw, 32);  if (old_pw[0] == '\0') return;
    kprintf("Password baru: ");       cli_read_password(new_pw, 32);  if (new_pw[0] == '\0') return;
    kprintf("Konfirmasi password baru: "); cli_read_password(confirm, 32); if (confirm[0] == '\0') return;
    if (kstrcmp(new_pw, confirm) != 0) { kprintf("passwd: konfirmasi tidak cocok.\n"); return; }
    if (protect_change_password(old_pw, new_pw) == E_OK) kprintf("passwd: password berhasil diganti.\n");
    else                                                  kprintf("passwd: password lama salah.\n");
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

    /* Shell mulai dalam mode user */
    int32_t tid = task_current() ? (int32_t)task_current()->id : 0;
    protect_set_priv(tid, PRIV_USER);
    shell_root = 0;

    for (;;) {
        if (shell_root) {
            hud_render();
            kprintf("admin@hexa> ");
        } else
        kprintf("kiosk@hexa> ");
        read_line(line, CLI_BUF_SIZE);
        hist_add(line);                   /* simpan dulu, sebelum parse merusak isi line */
        int argc = parse(line, argv);
        if (argc == 0) continue;
        if (argc < 0) { kprintf("too many arguments (max %d words)\n", CLI_MAX_ARGS); continue; }

        int found = 0;
        for (int i = 0; i < cmd_count; i++) {
            if (kstrcmp(argv[0], cmds[i].name) == 0) {
                if (cmds[i].root_only && !shell_root) {
                    /* [DEMO-POIN8] Screenshot baris ini untuk Bab III. */
                    kprintf("Akses Ditolak: Hanya Admin yang dapat menjalankan '%s'.\n", argv[0]);
                    kprintf("Ketik 'login' untuk masuk sebagai admin.\n");
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

// 5) cli_init: ganti seluruh isinya
void cli_init(void) {
    cmd_info_init();
    cmd_todo_init();
    cmd_system_init();
    cmd_diag_init();
    cli_register_args("login", "masuk sebagai admin (butuh password)", cmd_login, 1);
    cli_register     ("logout","keluar dari admin, kembali ke kiosk", cmd_logout);
    cli_register_root("passwd","ganti password admin",                  cmd_passwd, 0);
}