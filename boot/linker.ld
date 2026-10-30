/* linker.ld - tata letak kernel di memori (dimuat di 1 MB) */
ENTRY(_start)

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    /* header Multiboot harus berada di 8 KB pertama file */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /* awal memori bebas, dipakai alokator (memory.c) sebagai awal heap */
    . = ALIGN(4K);
    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}