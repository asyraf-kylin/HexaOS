; usermode_entry.asm - transisi privilege NYATA ring 0 -> ring 3 via iret.
; Dipanggil dari cmd_login()/cmd_logout() saat pergantian user.
;
; Rantai bukti PDF poin 8:
;   goto_user_mode() : iret dengan CS=0x1B / SS=0x23 (RPL=3) -> CPL=3
;   user_stub        : jalan di ring 3, data user di region 8 KB bisa diakses
;   hlt              : instruksi privileged -> #GP(0) dari HARDWARE (CPL=3)
;   IDT vektor 13    : handler menangkap #GP, arahkan iret kembali ke ring 0
;   ring3_gp_resume  : pulihkan ESP kernel, lanjut ke pemanggil di ring 0
;
; Base segmen user diisi gdt_set_user_region() sebelum transisi, sehingga
; [0x1000] di dalam stub berarti user_region + 0x1000.

[bits 32]

extern gdt_set_kernel_stack

global goto_user_mode
global ring3_gp_resume
global user_stub
global user_stub_end
global user_priv_ip
global ring3_gp_saved_esp

section .text

; Dipanggil dari C di ring 0. Tidak pernah "return" biasa: iret menuju
; user_stub. Kembali ke C lewat handler #GP -> ring3_gp_resume.
goto_user_mode:
    cli
    mov   eax, esp
    mov   [ring3_gp_saved_esp], eax   ; ESP ring 0 + return address di atasnya
    push  eax
    call  gdt_set_kernel_stack       ; TSS.esp0 -> stack kernel thread ini
    add   esp, 4

    ; DS/ES/FS/GS harus selector ring 3 sebelum iret. Kalau masih 0x10
    ; (DPL=0), akses data pertama di ring 3 langsung #GP.
    mov   ax, 0x23
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax

    ; Frame iret untuk turun ke ring 3 (urutan push terbalik dari pop CPU).
    push  dword 0x23                 ; SS  = user data, RPL 3
    push  dword 0x1800               ; ESP = offset 0x1800 di segmen user
    pushfd                           ; EFLAGS (IF=0 karena cli di atas)
    push  dword 0x1B                 ; CS  = user code, RPL 3
    push  dword 0                    ; EIP = offset 0 = user_stub
    iret

; Handler #GP mengubah frame menjadi return ke sini di ring 0.
; iret ke ring 0 TIDAK mem-pop SS:ESP yang didorong CPU saat masuk dari
; ring 3, sehingga ESP setelah iret = saved_esp - 8. Kita pulihkan dulu
; ESP ke saved_esp supaya `ret` kembali ke pemanggil goto_user_mode().
; DS sudah 0x10 (kernel) karena handler mengubah r->ds.
ring3_gp_resume:
    mov   esp, [ring3_gp_saved_esp]
    ret

; ---------------------------------------------------------------------
; Byte-byte ini disalin C ke user_region (base segmen user), lalu EIP=offset
; 0 memulai eksekusi di sana. Semua operasi di bawah berjalan di CPL=3.
; ---------------------------------------------------------------------
user_stub:
    mov   dword [0x1000], 0x0BADF00D ; DIIZINKAN: tulis ke data user
    mov   eax, [0x1000]              ; DIIZINKAN: baca data user
    mov   [0x1004], eax
    mov   dword [0x1008], 1          ; penanda titik legal sudah lewat
user_priv_ip:
    hlt                              ; HLT hanya boleh CPL=0 -> #GP hardware
user_stub_end:

section .data
ring3_gp_saved_esp: dd 0
