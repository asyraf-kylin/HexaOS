#include "cli.h"
#include "kernel.h"
#include "todo.h"
#include "svc.h"

void todo_board_show(void) {
    char text[TODO_TEXT_MAX + 1];
    int id, done;
    int n = todo_count();

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    kprintf("================== LIST TUGAS ==================\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (n == 0) {
        kprintf("  (belum ada tugas)  ->  ketik: add <teks tugas>\n");
    } else {
        for (int i = 0; i < n; i++) {
            if (todo_at(i, &id, &done, text, (int)sizeof(text)) != TODO_OK) continue;
            vga_set_color(done ? VGA_DARK_GREY : VGA_WHITE, VGA_BLACK);
            kprintf("  %3d  [%c]  %s\n", id, done ? 'x' : ' ', text);
        }
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }

    int active, finished;
    todo_get_counts(&active, &finished);
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    kprintf("-------------------------------------------------\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("  Aktif: %d   Selesai: %d\n", active, finished);
}

static void report(int rc) {
    switch (rc) {
    case TODO_ERR_FULL:
    case TODO_ERR_OOM:   kprintf("Penyimpanan penuh! Hapus beberapa tugas terlebih dahulu.\n"); break;
    case TODO_ERR_NOID:  kprintf("Tugas dengan nomor itu tidak ditemukan.\n"); break;
    case TODO_ERR_EMPTY: kprintf("Teks tugas tidak boleh kosong.\n"); break;
    case TODO_ERR_NOMOD: kprintf("Modul penyimpanan tugas belum terpasang.\n"); break;
    default:             kprintf("Gagal (kode %d).\n", rc); break;
    }
}

static int parse_id(const char* s) {            /* "12" -> 12, selain itu -1 */
    if (*s == '\0') return -1;
    int v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return -1;
        v = v * 10 + (*s - '0');
        if (v > 999999) return -1;
    }
    return v >= 0 ? v : -1;              /* nomor 0 juga valid */
}

static int read_text(int argc, char** argv, int from, char* out, int max) {
    kjoin_args(argc, argv, from, out, (size_t)max);
    if (out[0] == '\0') { report(TODO_ERR_EMPTY); return -1; }
    if ((int)kstrlen(out) > TODO_TEXT_MAX) {
        kprintf("Teks terlalu panjang (maksimum %d karakter).\n", TODO_TEXT_MAX);
        return -1;
    }
    return 0;
}

static void cmd_add(int argc, char** argv) {
    if (argc < 2) { kprintf("Penggunaan: add <teks tugas>\n"); return; }
    char text[CLI_BUF_SIZE];
    if (read_text(argc, argv, 1, text, (int)sizeof(text)) != 0) return;
    int rc = todo_add(text);
    if (rc >= 0) { kprintf("Tugas #%d ditambahkan.\n", rc); }
    else        report(rc);
}

static void cmd_list(int argc, char** argv) { (void)argc; (void)argv; todo_board_show(); }

static void cmd_done(int argc, char** argv) {
    int id = (argc == 2) ? parse_id(argv[1]) : -1;
    if (id < 0) { kprintf("Penggunaan: done <no_tugas>\n"); return; }
    int rc = todo_done(id);
    if (rc == TODO_OK) { kprintf("Tugas #%d selesai.\n", id); }
    else               report(rc);
}

static void cmd_undone(int argc, char** argv) {
    int id = (argc == 2) ? parse_id(argv[1]) : -1;
    if (id < 0) { kprintf("Penggunaan: undone <no_tugas>\n"); return; }
    int rc = todo_undone(id);
    if (rc == TODO_OK) { kprintf("Tugas #%d kembali ke belum selesai.\n", id); }
    else               report(rc);
}

static void cmd_edit(int argc, char** argv) {
    int id = (argc >= 3) ? parse_id(argv[1]) : -1;
    if (id < 0) { kprintf("Penggunaan: edit <no_tugas> <teks baru>\n"); return; }
    char text[CLI_BUF_SIZE];
    if (read_text(argc, argv, 2, text, (int)sizeof(text)) != 0) return;
    int rc = todo_edit(id, text);
    if (rc == TODO_OK) { kprintf("Tugas #%d diubah.\n", id); }
    else               report(rc);
}

static void cmd_del(int argc, char** argv) {
    int id = (argc == 2) ? parse_id(argv[1]) : -1;
    if (id < 0) { kprintf("Penggunaan: del <no_tugas>\n"); return; }
    int rc = todo_del(id);
    if (rc == TODO_OK) { kprintf("Tugas #%d dihapus.\n", id); }
    else               report(rc);
}

static void cmd_purge(int argc, char** argv) {
    (void)argc; (void)argv;
    if (purge_start() < 0) { kprintf("purge: gagal membuat thread (slot penuh).\n"); return; }
    kprintf("Pembersihan tugas selesai dimulai (thread 'purge'). Ketik 'list' untuk melihat hasilnya.\n");
}

void cmd_todo_init(void) {
    cli_register_args("add",   "tambah tugas baru: add <teks tugas>",  cmd_add,  -1);
    cli_register     ("list",  "tampilkan papan tugas",                cmd_list);
    cli_register_args("done",  "tandai tugas selesai: done <no>",      cmd_done,  1);
    cli_register_args("undone","kembalikan tugas ke belum selesai: undone <no>", cmd_undone, 1);
    cli_register_root("edit",  "ubah teks tugas: edit <no> <teks>",    cmd_edit, -1);
    cli_register_root("del",   "hapus tugas: del <no>",                cmd_del,   1);
    cli_register_root("purge", "hapus semua tugas yang sudah selesai", cmd_purge, 0);
}