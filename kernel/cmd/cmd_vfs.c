#include "../include/cli.h"
#include "../include/vfs.h"
#include "../include/memory.h"
#include "../include/kernel.h"

/* Handler: touch <nama_file> */
static void fn_touch(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Penggunaan: touch <nama_file>\n");
        return;
    }
    int res = vfs_create(argv[1]);
    if (res == 0) {
        kprintf("File '%s' berhasil dibuat.\n", argv[1]);
    } else if (res == -1) {
        kprintf("Error: File '%s' sudah ada!\n", argv[1]);
    } else {
        kprintf("Error: RAMDisk penuh!\n");
    }
}

/* Handler: cat <nama_file> */
static void fn_cat(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Penggunaan: cat <nama_file>\n");
        return;
    }
    char* content = vfs_read(argv[1]);
    if (content) {
        kprintf("%s\n", content);
    } else {
        kprintf("Error: File '%s' tidak ditemukan atau kosong.\n", argv[1]);
    }
}

/* Handler: rm <nama_file> */
static void fn_rm(int argc, char** argv) {
    if (argc < 2) {
        kprintf("Penggunaan: rm <nama_file>\n");
        return;
    }
    if (vfs_delete(argv[1]) == 0) {
        kprintf("File '%s' berhasil dihapus.\n", argv[1]);
    } else {
        kprintf("Error: Gagal menghapus '%s' (file tidak ditemukan).\n", argv[1]);
    }
}

/* Handler: write <nama_file> <pesan...> */
/* Handler: write <nama_file> <teks...> */
static void fn_write(int argc, char** argv) {
    if (argc < 3) {
        kprintf("Penggunaan: write <nama_file> <teks>\n");
        return;
    }

    /* Buffer penampung seluruh kalimat teks */
    char full_text[256];
    full_text[0] = '\0';
    int cur_len = 0;

    /* Gabungkan semua argumen mulai dari argv[2] sampai argv[argc-1] */
    for (int i = 2; i < argc; i++) {
        const char* word = argv[i];
        
        /* Salin kata ke buffer */
        while (*word && cur_len < (int)sizeof(full_text) - 2) {
            full_text[cur_len++] = *word++;
        }

        /* Beri spasi antar kata jika belum di kata terakhir */
        if (i < argc - 1 && cur_len < (int)sizeof(full_text) - 1) {
            full_text[cur_len++] = ' ';
        }
    }
    full_text[cur_len] = '\0';

    /* Tulis teks lengkap ke file */
    int res = vfs_write(argv[1], full_text);
    if (res == 0) {
        kprintf("Data berhasil ditulis ke '%s'.\n", argv[1]);
    } else if (res == -3) {
        kprintf("Error: Alokasi memori gagal (OOM)!\n");
    } else {
        kprintf("Error: File '%s' tidak ditemukan.\n", argv[1]);
    }
}

/* Handler: ls */
static void fn_ls(int argc, char** argv) {
    (void)argc;
    (void)argv;
    vfs_list();
}

/* Handler: meminfo */
static void fn_meminfo(int argc, char** argv) {
    (void)argc;
    (void)argv;
    kprintf("Status Heap Kernel:\n");
    kprintf("  Digunakan : %u bytes\n", (unsigned int)memory_get_used());
    kprintf("  Tersedia  : %u bytes\n", (unsigned int)memory_get_free());
}

/* Handler: vfstest */
static void fn_vfstest(int argc, char** argv) {
    (void)argc;
    (void)argv;
    vfs_run_test();
}

/* Inisialisasi dan pendaftaran perintah ke CLI */
void cmd_vfs_init(void) {
    cli_register_args("touch",   "Buat berkas baru di RAMDisk",       fn_touch,   1);
    cli_register_args("cat",     "Tampilkan isi berkas teks",         fn_cat,     1);
    cli_register_args("rm",      "Hapus berkas dari RAMDisk",         fn_rm,      1);
    cli_register_args("write",   "Tulis teks ke berkas: write <f> <t>", fn_write, -1);
    cli_register     ("ls",      "Tampilkan daftar berkas RAMDisk",   fn_ls);
    cli_register     ("meminfo", "Tampilkan statistik alokasi heap",  fn_meminfo);
    cli_register     ("vfstest", "Jalankan pengujian modul VFS",      fn_vfstest);
}