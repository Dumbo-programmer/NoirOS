ENTRY(start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* Load kernel at 1MB (0x00100000) — standard GRUB load address */
    . = 0x00100000;

    /* Combine multiboot  text to ensure ordering */
    .text : {
        *(.multiboot)    /* Multiboot header  first */
        *(.text*)        /* text/code sections */
    } :text

    /* Read-only data (strings, consts) */
    .rodata ALIGN(4096) : {
        *(.rodata*)
    } :rodata

    /* Initialized data */
    .data ALIGN(4096) : {
        *(.data*)
    } :data

    /* Uninitialized data */
    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss*)
    } :data

    /* Mark end of kernel symbol */
    _kernel_end = .;
}