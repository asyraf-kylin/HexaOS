/* usermode.c - proteksi privilege saat pergantian user (login/logout).
   Dipanggil dari cmd_login()/cmd_logout() di cli.c, bukan demo terpisah.
   Alurnya: iret ke CS=0x1B/SS=0x23 (CPL=3), kode user berjalan, instruksi
   privileged HLT ditolak CPU dengan #GP(0), lalu handler IDT mengembalikan
   eksekusi ke ring 0 sebelum hak admin diberikan/dicabut. */

#include "kernel.h"

/* Wilayah user: 8 KB, base = alamat array ini. Descriptor ring 3 dibatasi
   oleh gdt_set_user_region(); stack user memakai offset 0x1800. */
#define USER_REGION_SIZE 0x2000
static uint8_t user_region[USER_REGION_SIZE] __attribute__((aligned(0x1000)));

extern uint8_t  user_stub[];
extern uint8_t  user_stub_end[];
extern uint8_t  user_priv_ip[];
extern uint32_t ring3_gp_saved_esp;
extern void     ring3_gp_resume(void);

static volatile int      gp_seen;
static volatile uint32_t expected_eip;

static void ring3_gp_handler(registers_t* r) {
    if (r->cs != 0x1B) {
        kpanic("#GP di luar pergantian user");
        return;
    }

    gp_seen = (r->eip == expected_eip);

    /* Alihkan iret kembali ke ring 0 lewat trampoline usermode_entry.asm.
       DS wajib 0x10; SS/useresp diabaikan CPU karena target iret ring 0. */
    r->ds  = 0x10;
    r->cs  = 0x08;
    r->eip = (uint32_t)ring3_gp_resume;
}

void user_switch_ring3(void) {
    memset(user_region, 0, sizeof(user_region));
    memcpy(user_region, user_stub, (size_t)(user_stub_end - user_stub));
    gdt_set_user_region((uint32_t)user_region, USER_REGION_SIZE);

    /* EIP yang didorong CPU adalah OFFSET di dalam CS, bukan alamat linear. */
    expected_eip = (uint32_t)(user_priv_ip - user_stub);
    gp_seen = 0;
    isr_register(13, ring3_gp_handler);

    kprintf_bg("[user] turun ke CPL=3 via iret (CS=0x1B, SS=0x23)\n");
    goto_user_mode();
    /* ring3_gp_resume mengembalikan ESP kernel ke sini, masih dengan IF=0 */

    interrupts_enable();  /* kembalikan interrupt yang dimatikan goto_user_mode */

    uint32_t val  = *(volatile uint32_t*)(user_region + 0x1004);
    uint32_t flag = *(volatile uint32_t*)(user_region + 0x1008);

    kprintf_bg("[user] kode CPL=3 akses data user = 0x%x (diizinkan), penanda = %u\n",
            val, flag);
    kprintf_bg("[user] %s\n",
            gp_seen ? "HLT di CPL=3 ditolak #GP hardware, kembali ke CPL=0 (CS=0x08)"
                    : "GAGAL: transisi privilege tidak terbukti");

    isr_register(13, NULL);   /* kembalikan #GP ke exception_fatal() */
}
