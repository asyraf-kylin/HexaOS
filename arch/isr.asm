; isr.asm - stub interrupt (vektor 0-47) dan jalur masuk ke handler C.
; CPU memanggil stub -> stub menyimpan register -> isr_handler() (idt.c) ->
; register dipulihkan -> iret kembali ke kode yang diinterupsi.

[bits 32]
extern isr_handler

; Exception tanpa error code: dorong 0 supaya bentuk stack-nya seragam.
%macro ISR_NOERR 1
%assign v %1
isr %+ v:
    push dword 0            ; err_code palsu
    push dword v            ; int_no
    jmp  isr_common
%endmacro

; Exception yang sudah didorong error code oleh CPU (8,10-14,17,21,29,30).
%macro ISR_ERR 1
%assign v %1
isr %+ v:
    push dword v            ; int_no (err_code sudah ada dari CPU)
    jmp  isr_common
%endmacro

section .text

%assign n 0
%rep 48
  %if (n == 8) || (n >= 10 && n <= 14) || (n == 17) || (n == 21) || (n == 29) || (n == 30)
    ISR_ERR n
  %else
    ISR_NOERR n
  %endif
  %assign n n+1
%endrep

; Susunan stack di sini harus cocok dengan registers_t di kernel.h:
;   [ds][edi esi ebp esp ebx edx ecx eax][int_no][err_code][eip cs eflags ...]
isr_common:
    pusha                   ; eax ecx edx ebx esp ebp esi edi
    mov  ax, ds
    push eax                ; simpan segmen data yang lama
    mov  ax, 0x10           ; segmen data kernel
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    cld                     ; ABI C mewajibkan direction flag = 0
    push esp                ; argumen: pointer ke registers_t
    call isr_handler
    add  esp, 4
    pop  eax
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    popa
    add  esp, 8             ; buang int_no + err_code
    iret

section .data
global isr_stub_table
isr_stub_table:
%assign n 0
%rep 48
    dd isr %+ n
%assign n n+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits