#ifndef CLI_H
#define CLI_H

#define CLI_MAX_CMDS   48
#define CLI_BUF_SIZE   128
#define CLI_MAX_ARGS   64      /* 127 karakter / 2 = maksimum 64 kata per baris */

typedef void (*cli_handler_t)(int argc, char** argv);

typedef struct {
    const char*    name;
    const char*    help;
    cli_handler_t  fn;
    int            core;
    int            max_args;   /* -1 = bebas */
    int            root_only;  /* 1 = hanya admin; dicek SATU tempat di cli_run */
} cli_cmd_t;

void cli_init(void);
void cli_run(void);
void todo_board_show(void);    /* cmd_todo.c, dipakai main.c saat boot */

int  cli_is_root(void);
void cli_read_password(char* buf, int max);
void cli_hud_toggle(void);
int  cli_hud_is_active(void);
void cli_hud_refresh(void);   /* dipanggil timer tick: update HUD secara realtime */

int  cli_register(const char* name, const char* help, cli_handler_t fn);
int  cli_register_core(const char* name, const char* help, cli_handler_t fn);
int  cli_register_args(const char* name, const char* help, cli_handler_t fn, int max_args);
int  cli_register_root(const char* name, const char* help, cli_handler_t fn, int max_args);

int                cli_count(void);
const cli_cmd_t*   cli_get(int i);

void cmd_system_init(void);
void cmd_info_init(void);
void cmd_todo_init(void);
void cmd_diag_init(void);

#endif