#ifndef CLI_H
#define CLI_H

#define CLI_MAX_CMDS   32
#define CLI_BUF_SIZE   128
#define CLI_MAX_ARGS   8

/* Tipe fungsi perintah: terima jumlah argumen + daftar argumen */
typedef void (*cli_handler_t)(int argc, char** argv);

/* Satu entri perintah */
typedef struct {
    const char*    name;   // "ls", "memtest", ...
    const char*    help;   // deskripsi singkat untuk perintah `help`
    cli_handler_t  fn;
    int            core;    // 1 = perintah inti (ditampilkan paling atas di `help`)
    int            max_args; // jumlah argumen maksimum setelah nama perintah (-1 = bebas)
} cli_cmd_t;

/* Dipanggil dari main.c */
void cli_init(void);
void cli_run(void);        // loop utama: baca input, parse, jalankan

/* Dipanggil dari modul lain untuk mendaftarkan perintahnya */
/* cli_register / cli_register_core: perintah TANPA argumen. Kalau pengguna mengetik
   argumen tambahan (mis. "help xyz"), shell menolaknya dengan pesan error.
   cli_register_args: perintah yang menerima sampai max_args argumen (mis. "cat a.txt" -> 1). */
int  cli_register(const char* name, const char* help, cli_handler_t fn);
int  cli_register_core(const char* name, const char* help, cli_handler_t fn);
int  cli_register_args(const char* name, const char* help, cli_handler_t fn, int max_args);
/* Akses daftar perintah (dipakai cmd_help) */
int                cli_count(void);
const cli_cmd_t*   cli_get(int i);

/* Init tiap grup perintah (folder kernel/cmd), dipanggil dari cli_init() */
void cmd_system_init(void);
void cmd_info_init(void);
void cmd_vfs_init(void);

#endif