# Panduan Demo HexaOS — Bahan Bab II & Bab III

Ringkasan singkat: **kode tidak perlu diubah untuk demo normal**. Yang perlu
kamu lakukan adalah menjalankan urutan perintah di bawah, mengambil screenshot,
lalu menjelaskan rantai kode yang disebut di tabel. Penanda `[DEMO-...]`,
`[UBAH]`, `[HAPUS]`, `[JANGAN]` sudah ditaruh langsung di source code.

---

## 1. Cara kerja Round Robin (kenapa "tidak kelihatan")

Rantainya nyata, bukan `printf` simulasi:

```
PIT 1193182/100 = 11931  ->  IRQ0 tiap 10 ms        (drivers/timer.c)
timer_irq()              ->  ticks++ ; callback()   (drivers/timer.c)
callback = task_tick()   ->  cpu_ticks++ ; slice++  (kernel/src/task.c)
slice >= TASK_QUANTUM(5) ->  schedule()             (kernel/src/task.c)
schedule()               ->  pick_next()            (kernel/src/task.c)
switch_context()         ->  tukar ESP + register   (arch/switch.asm)
iret di isr_common       ->  EIP thread baru jalan  (arch/isr.asm)
```

Kuantum = 5 tick x 10 ms = **50 ms**. `pick_next()` mulai dari slot setelah
thread sekarang (`(start + i) % TASK_MAX`), jadi sifatnya melingkar.

Kenapa layar seolah diam:

* Shell berhenti di `kgetc()` (polling keyboard) — tetap dapat `cpu_ticks`,
  tapi tidak mencetak apa-apa.
* Thread `summary` sengaja **silent**, hanya menulis ring buffer di `svc.c`.

Cara melihat RR tanpa mengubah kode:

1. `login` (password default `root`), tekan **Tab** → HUD `CPU` dan `SW`
   milik `summary` bertambah.
2. `sysinfo` dua kali dengan jeda → kolom `CPU(tick)` dan `GILIRAN` naik.
3. `diag preempt` → huruf A dan B bergantian **tanpa `task_yield()`**.

Kalau ingin lebih cepat terlihat: ubah `TASK_QUANTUM` di
`kernel/include/task.h` dari `5` jadi `2` (20 ms), lalu `make clean && make`.

---

## 2. Tabel demo per komponen PDF (bahan Bab III)

| PDF | Perintah | Yang harus terlihat | Screenshot / bukti |
|---|---|---|---|
| 1. Boot & CLI | boot, `help`, `add`, `list` | logo ASCII, prompt, 3 perintah jalan | layar boot + `help` |
| 2. Thread | `diag threads`, `purge` lalu `sysinfo` berulang | NEW → READY → RUNNING → WAITING → TERMINATED | tabel status thread |
| 3. Round Robin | `diag preempt`, HUD/Tab, `sysinfo` | A/B bergantian; `CPU`/`GILIRAN` bertambah | keluaran A/B + angka giliran |
| 4. Sinkronisasi | `diag race` | tanpa mutex `< 100000` (SALAH), dengan mutex `100000` (BENAR) | dua baris hasil |
| 5. Deadlock | `diag deadlock` | skenario 1 terblokir + timeout; skenario 2 selesai dengan lock ordering | dua skenario |
| 6. Memori | `add` > 64 kali, `diag mem` | "Penyimpanan penuh"; Free ≈ 0 saat OOM lalu balik lagi | keluaran `[MEM]` + `sysinfo` |
| 7. VFS | `add`, `list`, `edit`, `done`, `del` | CRUD `todo_<id>.txt` di RAMDisk; counter benar | papan tugas |
| 8. Proteksi | `del 1` sbg guest, `diag race`, `login`, `del 1`, `diag protect` | Akses Ditolak → prompt admin → berhasil; semua "SESUAI" | screenshot sebelum/sesudah login |

Urutan pakai dari task board:

```
guest  : add beli susu dan telur
         add kerjakan laporan
         done 1
         del 2            -> Akses Ditolak
         diag race        -> Akses Ditolak
         login            -> password: root
admin  : edit 2 kerjakan laporan SO
         del 1
         sysinfo
         diag race
         diag deadlock
         diag mem
         diag protect
         diag threads
         diag preempt
         purge
         logout
```

### Contoh log asli dari QEMU (boleh dipakai sebagai bukti Bab III)

```
admin@hexa> diag race
== Uji race condition: 2 thread x 50000 kenaikan ==
Tanpa mutex: 50000 (harapan 100000) -> SALAH (ada update hilang)
Dengan mutex: 100000 (harapan 100000) -> BENAR

admin@hexa> diag deadlock
== Skenario 1: deadlock (T1: A->B, T2: B->A) + deteksi timeout ==
[T2] dapat 'B', menunggu 'A'...
[T1] dapat 'A', menunggu 'B'...
[T2] DEADLOCK TERDETEKSI (timeout) -> melepas 'B'
[T1] dapat kedua lock
== Skenario 2: solusi lock ordering ==
[T1] memegang A dan B (urutan terurut), bekerja...
[T1] selesai tanpa deadlock
[T2] memegang A dan B (urutan terurut), bekerja...
[T2] selesai tanpa deadlock

admin@hexa> diag preempt
Dua thread loop tanpa yield, 1 huruf = 1 tick CPU (Ctrl+C = batal):
AAAAABBBBBAAAAABBBBBAAAAABBBBB...
giliran A=13, B=14 (ganti tiap 5 tick = 50 ms)
```

Angka `50000`, `100000`, `giliran A=13, B=14` di atas bukan hasil hitung
manual — itu keluaran kernel yang sudah diverifikasi dengan
`make clean && make` lalu dijalankan di QEMU.

---

## 3. "Aku mau dapat masalahnya", bukan cuma panggil diag

Jujurnya: **race dan deadlock tidak akan muncul dengan sendirinya di alur
kiosk yang sudah benar**. Itu justru tanda sinkronisasinya bekerja. Untuk
mendapatkan masalah sungguhan harus sengaja memasukkan bug sementara.

### 3.1 Race Condition (poin 4)

Kenapa alur normal aman: `cached_active`/`cached_done` dikunci
`mutex_counter`, dan setiap `add/done/del` ditutup `todo_recount()` yang
menghitung ulang dari file VFS.

Eksperimen sementara (penanda `[DEMO-RACE]` di `kernel/src/todo.c`):

1. Di `todo_add()`, ganti `todo_recount(&cached_active, &cached_done);`
   dengan `mutex_lock(&mutex_counter); cached_active++; mutex_unlock(&mutex_counter);`
   (versi "dengan locking"), lalu hapus 3 baris mutex itu untuk versi "tanpa lock".
2. Di `kernel/src/svc.c`, jalankan thread kedua yang melakukan `cached_active++`
   tanpa mutex (atau pakai `race_worker` di `tests.c` sebagai acuan).
3. Ketik `add`/`done` bergantian cepat, lalu `sysinfo` vs `list` → angka
   Aktif/Selesai selisih dengan isi papan.
4. **Kembalikan kode setelah demo.**

Catatan penting: dengan `-O2`, `counter++` bisa jadi satu instruksi `incl`,
dan CPU tunggal tidak bisa memotongnya. Itu sebabnya `test_race()` sengaja
memisahkan baca–ubah–tulis dan memanggil `task_yield()` di antaranya.

### 3.2 Deadlock (poin 5)

Jangan dipaksa muncul di `add`/`del` produksi: `del` mengunci
`mutex_vfs → mutex_counter`, `summary` juga `vfs → counter`. Kalau kamu
membalik urutan di `todo_add()` (penanda `[DEMO-DEADLOCK]`), **seluruh kiosk
hang** sampai QEMU di-reset. Itu sebabnya task board menolak deadlock alami.

Yang dipakai `diag deadlock` tetap nyata: dua thread benar-benar saling
menunggu, `mutex_timedlock()` benar-benar timeout (50 tick = 500 ms), lalu
skenario 2 memakai `mutex_lock_ordered()` (urutan alamat mutex yang sama).
Jadi ini bukan simulasi teks — cuma dibuat terkendali supaya demo tidak gagal.

Kalau tetap ingin menunjukkan hang "asli": lakukan di QEMU, ambil screenshot,
lalu `reboot` / restart QEMU. Jangan lakukan di depan dosen tanpa timeout.

---

## 4. Peta file

**Jangan dihapus** (bagian dari syarat PDF):
`boot/boot.asm`, `boot/linker.ld`, `arch/{isr,switch}.asm`, `drivers/timer.c`,
`kernel/src/{task,tests,todo,svc,memory,vfs,protect,cli,main}.c`,
`kernel/logo/*`, `kernel/cmd/cmd_todo.c`, `kernel/cmd/cmd_info.c`,
`kernel/cmd/cmd_system.c`, `kernel/cmd/cmd_diag.c` (bukti poin 3–6 & 8).

**Sudah dibuang dari repo:**
`kernel/cmd/cmd_vfs.c`, `stubs.c`, `semaphore.c/.h` (dulu sudah tidak ada),
`kernel/src/sync.c` + `sync_init()` (bersama deklarasi di `main.c`, `kernel.h`,
`sync.h`), `mutex_modul_init()` (`mutex.c`/`mutex.h`), prototipe usang
`vfs_run_test()` di `vfs.h`, dan blok `#ifdef HEXAOS_HW_RING3` di `protect.c`.

`sync.h` tetap dipertahankan karena masih berisi prototipe `test_race`,
`test_deadlock`, dan `test_protect` yang dipakai `tests.c`/`todo.c`.
Mutex tidak perlu inisialisasi global: `mutex_init()` dipanggil per objek
di `todo.c` dan `tests.c`. PDF mengizinkan kontrol akses sederhana, jadi
penghapusan ring 3 tidak mengurangi pemenuhan poin 8.

**Boleh diubah untuk eksperimen:**
`kernel/include/task.h` (`TASK_QUANTUM`), `kernel/src/todo.c` (urutan lock /
counter / urutan id), `kernel/src/svc.c` (writer tanpa mutex),
`kernel/src/tests.c` (jendela race), `kernel/src/cli.c` (prompt guest/admin).

**Catatan ketidaksesuaian dengan task board** (sengaja tidak diubah):
prompt masih `kiosk@hexa>` / `admin@hexa>` (PDF board: `guest@hexa>` /
`admin@hexa#`), dan ID tugas mulai **0** serta dipakai ulang saat papan kosong
(PDF board: mulai **1**, tidak dipakai ulang). Penanda `[UBAH-SESUAIPDF]`
di `cli.c` dan `todo.c` menunjukkan barisnya. Setiap perubahan:
`make clean && make run`.
