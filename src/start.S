.section .multiboot
    .align 4
    .long 0x1BADB002          # magic
    .long 0x00000000          # flags 
    .long 0xE4524FFE          # checksum = -(magic + flags) = -(0x1BADB002 + 0x00000000)

.section .text
.global start
start:
    cli
    mov $kernel_stack_end, %esp   # set stack pointer
    call kernel_main

.hang:
    hlt
    jmp .hang

.section .bss
    .lcomm kernel_stack, 8192
.global kernel_stack_end
kernel_stack_end: