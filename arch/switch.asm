
; switch.asm - context switch antar thread.
;   void switch_context(uint32_t* old_esp, uint32_t new_esp);
; Simpan register callee-saved thread lama di stack-nya, catat esp-nya,
; muat esp thread baru, pulihkan register, lalu "ret" ke alamat di stack baru.
;
; [DEMO-RR][JANGAN DIUBAH] Ini bagian inti bukti PDF poin 3.
;   - EBP/EBX/ESI/EDI + ESP disimpan; EAX/ECX/EDX/EAX adalah caller-saved.
;   - EIP/EFLAGS tidak disimpan di sini: kalau switch berasal dari IRQ0,
;     frame interrupt ikut berpindah bersama ESP, lalu `iret` di isr_common
;     yang memulihkan EIP thread yang baru dipilih schedule().
;   - Thread baru pertama kali "bangun" lewat task_trampoline() di task.c,
;     karena stack-nya disiapkan dengan alamat palsu + ret di task_create().
;   - Jangan menambah kprintf di sini (dipanggil dengan interrupt mati).

[bits 32]
section .text
global switch_context

switch_context:
    mov  eax, [esp + 4]     ; eax = &thread_lama->esp
    mov  edx, [esp + 8]     ; edx = esp thread baru
    push ebp
    push ebx
    push esi
    push edi
    mov  [eax], esp         ; simpan stack thread lama
    mov  esp, edx           ; pindah ke stack thread baru
    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits