# Makefile HexaOS
# [PENANDA DEMO] Semua file .c di kernel/src, kernel/cmd, kernel/logo, arch,
# dan drivers diambil otomatis oleh wildcard di bawah. Jadi:
#   - menghapus sebuah file .c = menghapusnya dari build (setelah make clean)
#   - menambah file .c baru = langsung ikut tercompile, TIDAK perlu edit Makefile
# File/kode yang sudah dibuang: cmd_vfs.c, stubs.c, semaphore.c/h, sync.c,
# mutex_modul_init(), prototipe vfs_run_test(), dan blok HEXAOS_HW_RING3.
# Kalau mengubah/menghapus file: `make clean && make` lalu `make run`.
# [OPSIONAL] tambahkan .gitignore berisi "build/" sebelum commit akhir.
.RECIPEPREFIX := >

CC      := gcc
AS      := nasm
LD      := ld
QEMU    := qemu-system-i386

CFLAGS  := -m32 -march=i686 -ffreestanding -fno-pie -fno-stack-protector \
           -fcf-protection=none -fno-asynchronous-unwind-tables \
           -fno-tree-loop-distribute-patterns -mno-sse -mno-mmx \
           -O2 -Wall -Wextra -Wno-unused-parameter -Ikernel/include
LDFLAGS := -m elf_i386 -T boot/linker.ld -nostdlib -z noexecstack --build-id=none

C_SRCS := $(wildcard kernel/src/*.c) $(wildcard kernel/logo/*.c) \
          $(wildcard kernel/cmd/*.c) \
          $(wildcard arch/*.c) $(wildcard drivers/*.c)
A_SRCS  := $(wildcard boot/*.asm) $(wildcard arch/*.asm)
OBJS    := $(patsubst %.asm,build/%.o,$(A_SRCS)) $(patsubst %.c,build/%.o,$(C_SRCS))
KERNEL  := build/minios.elf
ISO_DIR       := build/iso
ISO           := build/hexa.iso
GRUB_MKRESCUE ?= grub-mkrescue

.PHONY: all run run-nox iso run-iso clean

all: $(KERNEL)

$(KERNEL): $(OBJS) boot/linker.ld
> $(LD) $(LDFLAGS) -o $@ $(OBJS)
> @echo "Build OK -> $@"

build/%.o: %.asm
> @mkdir -p $(dir $@)
> $(AS) -f elf32 $< -o $@

build/%.o: %.c $(wildcard kernel/include/*.h)
> @mkdir -p $(dir $@)
> $(CC) $(CFLAGS) -c $< -o $@

run: $(KERNEL)
> $(QEMU) -kernel $(KERNEL) -serial stdio -m 64M

run-nox: $(KERNEL)
> $(QEMU) -kernel $(KERNEL) -nographic -m 64M

iso: $(ISO)

$(ISO): $(KERNEL) boot/grub/grub.cfg
> @mkdir -p $(ISO_DIR)/boot/grub
> cp $(KERNEL) $(ISO_DIR)/boot/minios.elf
> cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
> $(GRUB_MKRESCUE) -o $@ $(ISO_DIR)
> @echo "ISO OK -> $@"

run-iso: $(ISO)
> $(QEMU) -cdrom $(ISO) -serial stdio -m 64M

clean:
> rm -rf build