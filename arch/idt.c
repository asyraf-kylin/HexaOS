/* idt.c - Interrupt Descriptor Table, PIC 8259, dan dispatcher interrupt.
   Vektor 0-31  = exception CPU (error, breakpoint, page fault, dst.)
   Vektor 32-47 = IRQ hardware 0-15 (timer = IRQ0, keyboard = IRQ1, ...)
   Semua IRQ di-MASK dulu. Sebuah IRQ baru aktif setelah ada yang memanggil
   irq_register(), jadi menyalakan interrupt (sti) tidak akan mengagetkan. */
#include "kernel.h"

#define IDT_ENTRIES 256
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1
#define PIC_EOI  0x20

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;    /* segmen kode kernel (0x08) */
    uint8_t  zero;
    uint8_t  flags;       /* 0x8E = present, ring 0, interrupt gate 32-bit */
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Susunan harus persis sama dengan urutan push di isr.asm (64 byte) */
_Static_assert(sizeof(registers_t) == 64, "registers_t tidak cocok dengan isr.asm");

extern uint32_t isr_stub_table[48];     /* alamat stub, didefinisikan di isr.asm */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;
static isr_handler_t    handlers[IDT_ENTRIES];

static const char* const exception_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS",
    "Segment Not Present", "Stack-Segment Fault", "General Protection Fault",
    "Page Fault", "Reserved", "x87 Floating-Point", "Alignment Check",
    "Machine Check", "SIMD Floating-Point", "Virtualization", "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
};

static void idt_set_gate(int n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].selector  = sel;
    idt[n].zero      = 0;
    idt[n].flags     = flags;
}

/* ---------- PIC 8259 ---------- */
static void io_wait(void) { outb(0x80, 0); }

/* Geser IRQ 0-15 ke vektor 32-47 (default-nya 8-15 bentrok dengan exception CPU) */
static void pic_remap(void) {
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DAT, 0x20); io_wait();    /* master: vektor mulai 32 */
    outb(PIC2_DAT, 0x28); io_wait();    /* slave : vektor mulai 40 */
    outb(PIC1_DAT, 0x04); io_wait();    /* slave tersambung di IRQ2 */
    outb(PIC2_DAT, 0x02); io_wait();
    outb(PIC1_DAT, 0x01); io_wait();    /* mode 8086 */
    outb(PIC2_DAT, 0x01); io_wait();
    outb(PIC1_DAT, 0xFF);               /* mask semua IRQ */
    outb(PIC2_DAT, 0xFF);
}

static void irq_set_mask(int irq, int masked) {
    uint16_t port = (irq < 8) ? PIC1_DAT : PIC2_DAT;
    uint8_t  bit  = (uint8_t)(1u << (irq & 7));
    uint8_t  v    = inb(port);
    outb(port, masked ? (uint8_t)(v | bit) : (uint8_t)(v & ~bit));
}

static void pic_eoi(int irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* IRQ7/IRQ15 kadang "palsu" (spurious): jangan di-EOI dan jangan diproses. */
static int irq_is_spurious(int irq) {
    if (irq != 7 && irq != 15) return 0;
    uint16_t cmd = (irq == 7) ? PIC1_CMD : PIC2_CMD;
    outb(cmd, 0x0B);                    /* minta ISR register */
    if (inb(cmd) & 0x80) return 0;      /* asli */
    if (irq == 15) outb(PIC1_CMD, PIC_EOI);   /* master tetap perlu EOI */
    return 1;
}

/* ---------- registrasi handler ---------- */
void isr_register(int int_no, isr_handler_t h) {
    if (int_no >= 0 && int_no < IDT_ENTRIES) handlers[int_no] = h;
}

void irq_register(int irq, isr_handler_t h) {
    if (irq < 0 || irq > 15) return;
    handlers[32 + irq] = h;
    irq_set_mask(irq, 0);
    if (irq >= 8) irq_set_mask(2, 0);   /* jalur cascade ke slave harus terbuka */
}

void irq_unregister(int irq) {
    if (irq < 0 || irq > 15) return;
    irq_set_mask(irq, 1);
    handlers[32 + irq] = 0;
}

/* ---------- exception yang tidak ditangani = fatal ---------- */
static void exception_fatal(registers_t* r) {
    uint32_t cr2 = 0;
    if (r->int_no == 14) __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** EXCEPTION %u: %s ***\n", r->int_no, exception_names[r->int_no]);
    kprintf("EIP=%p  CS=%x  EFLAGS=%x  ERR=%x\n", (void*)r->eip, r->cs, r->eflags, r->err_code);
    if (r->int_no == 14) kprintf("Alamat yang diakses (CR2)=%p\n", (void*)cr2);
    kprintf("EAX=%x EBX=%x ECX=%x EDX=%x\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf("Sistem dihentikan.\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* Dipanggil dari isr_common (isr.asm) untuk semua vektor 0-47 */
void isr_handler(registers_t* r) {
    uint32_t n = r->int_no;

    if (n >= 32 && n < 48) {
        int irq = (int)n - 32;
        if (irq_is_spurious(irq)) return;
        pic_eoi(irq);       /* EOI sebelum handler: aman kalau handler pindah thread */
    }

    if (handlers[n]) { handlers[n](r); return; }
    if (n < 32) exception_fatal(r);
}

/* Breakpoint (int3) tidak fatal: cetak lalu lanjut. Dipakai untuk uji IDT. */
static void breakpoint_handler(registers_t* r) {
    kprintf("[int3] breakpoint tertangkap, EIP=%p\n", (void*)r->eip);
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    memset(handlers, 0, sizeof(handlers));

    for (int i = 0; i < 48; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idtr));

    pic_remap();
    isr_register(3, breakpoint_handler);
}