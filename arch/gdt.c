/* gdt.c - Global Descriptor Table + TSS (x86 32-bit protected mode).
   Selector:  0x08 = kernel code, 0x10 = kernel data,
              0x18 = user code,   0x20 = user data,  0x28 = TSS.
   Semua segmen "flat" (base 0, limit 4 GB), jadi alamat logis = alamat fisik. */
#include "kernel.h"

#define GDT_ENTRIES 6

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;      /* present, ring, tipe segmen */
    uint8_t  gran;        /* flag granularitas + 4 bit atas limit */
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* TSS: untuk saat ini hanya dipakai CPU untuk tahu stack kernel (ss0:esp0)
   ketika terjadi perpindahan dari user mode ke kernel mode. */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0, ss0, esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtr;
static struct tss_entry tss;

static void gdt_set_gate(int i, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[i].base_low   = base & 0xFFFF;
    gdt[i].base_mid   = (base >> 16) & 0xFF;
    gdt[i].base_high  = (base >> 24) & 0xFF;
    gdt[i].limit_low  = limit & 0xFFFF;
    gdt[i].gran       = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access     = access;
}

void gdt_init(void) {
    gdt_set_gate(0, 0, 0, 0, 0);                    /* null descriptor (wajib) */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);        /* kernel code, ring 0 */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);        /* kernel data, ring 0 */
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);        /* user code,   ring 3 */
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);        /* user data,   ring 3 */

    memset(&tss, 0, sizeof(tss));
    tss.ss0        = 0x10;                          /* stack kernel = data kernel */
    tss.iomap_base = sizeof(tss);
    gdt_set_gate(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint32_t)&gdt;

    __asm__ volatile(
        "lgdt %0\n\t"
        "movw $0x10, %%ax\n\t"        /* muat ulang register data dengan 0x10 */
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"         /* far jump: muat ulang CS dengan 0x08 */
        "1:"
        : : "m"(gdtr) : "ax", "memory");

    __asm__ volatile("ltr %%ax" : : "a"((uint16_t)0x28));   /* aktifkan TSS */
}

/* Dipanggil scheduler/proteksi nanti: stack kernel milik thread yang jalan. */
void gdt_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}