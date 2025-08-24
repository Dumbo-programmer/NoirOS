ENTRY(start)

SECTIONS
{
    /* Load kernel at 1MB (0x00100000) — standard GRUB load address */
    . = 0x00100000;

    /* Combine multiboot  text to ensure ordering */
    .text : {
        *(.multiboot)    /* Multiboot header  first */
        *(.text*)        /* text/code sections */
    }

    /* Read-only data (strings, consts) */
    .rodata ALIGN(4096) : {
        *(.rodata*)
    }

    /* Initialized data */
    .data ALIGN(4096) : {
        *(.data*)
    }

    /* Uninitialized data */
    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss*)
    }

    /* Mark end of kernel symbol */
    _kernel_end = .;
}