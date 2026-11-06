# Makefile HEXA OS
# Baris perintah diawali ">" (bukan Tab), jadi aman di-copy-paste dari chat.
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

# Jendela QEMU (VGA) + terminal ini jadi input/output serial
run: $(KERNEL)
> $(QEMU) -kernel $(KERNEL) -serial stdio -m 64M

# Tanpa jendela: semua lewat terminal (keluar: Ctrl-A lalu X)
run-nox: $(KERNEL)
> $(QEMU) -kernel $(KERNEL) -nographic -m 64M

# ISO bootable: GRUB memuat kernel dari media (butuh grub, xorriso, mtools)
iso: $(ISO)

$(ISO): $(KERNEL) boot/grub/grub.cfg
> @mkdir -p $(ISO_DIR)/boot/grub
> cp $(KERNEL) $(ISO_DIR)/boot/minios.elf
> cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
> $(GRUB_MKRESCUE) -o $@ $(ISO_DIR)
> @echo "ISO OK -> $@"

# Jalankan lewat GRUB (bukan opsi -kernel)
run-iso: $(ISO)
> $(QEMU) -cdrom $(ISO) -serial stdio -m 64M

clean:
> rm -rf build