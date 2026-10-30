; boot.asm - titik masuk kernel (Multiboot)
; GRUB / QEMU (-kernel) memuat kernel ke RAM, lalu melompat ke _start.
; Saat tiba di sini CPU sudah dalam protected mode 32-bit.

MBALIGN  equ 1 << 0              ; sejajarkan modul pada batas halaman
MEMINFO  equ 1 << 1              ; minta info memori dari bootloader
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002          ; tanda pengenal header Multiboot
CHECKSUM equ -(MAGIC + FLAGS)    ; MAGIC + FLAGS + CHECKSUM harus = 0

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384                   ; stack kernel 16 KB
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli                          ; matikan interrupt sampai IDT siap
    mov  esp, stack_top          ; siapkan stack sebelum memanggil C
    push ebx                     ; arg 2: alamat struktur info Multiboot
    push eax                     ; arg 1: magic (harus 0x2BADB002)
    call kernel_main

.hang:                           ; kernel_main tidak boleh kembali
    cli
    hlt
    jmp  .hang

section .note.GNU-stack noalloc noexec nowrite progbits