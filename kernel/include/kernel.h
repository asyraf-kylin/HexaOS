#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         i32;

#define NULL ((void*)0)

void vga_init(void);
void serial_init(void);
void kprintf(const char* fmt, ...);
void print_logo(void);

void mem_init(void);
void vfs_init(void);
void sched_init(void);

void kpanic(const char* msg);

#endif
