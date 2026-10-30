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
} cli_cmd_t;

/* Dipanggil dari main.c */
void cli_init(void);
void cli_run(void);        // loop utama: baca input, parse, jalankan

/* Dipanggil dari modul lain untuk mendaftarkan perintahnya */
int  cli_register(const char* name, const char* help, cli_handler_t fn);

#endif