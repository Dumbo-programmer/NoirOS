.section .multiboot
    .align 4
    .long 0x1BADB002
    .long 0x00000000
    .long 0xE4524FFE

.section .text

.extern kernel_main
.extern isr_dispatch_c

.global start
start:
    cli
    mov $kernel_stack_end, %esp
    call kernel_main

.hang:
    hlt
    jmp .hang

.macro MAKE_IRQ_STUB name, vector
.global \name
\name:
    pushl $0
    pushl $\vector
    jmp isr_common
.endm

/* Default no-error-code interrupt stub.
 * This is used for all vectors we do not explicitly map.
 */
.global isr_stub_default
isr_stub_default:
    pushl $0
    pushl $255
    jmp isr_common

MAKE_IRQ_STUB irq1_stub, 33
MAKE_IRQ_STUB irq12_stub, 44

isr_common:
    pusha
    pushl %esp
    call isr_dispatch_c
    addl $4, %esp
    popa
    addl $8, %esp
    iret

.section .bss
    .lcomm kernel_stack, 8192
.global kernel_stack_end
kernel_stack_end:

.section .note.GNU-stack,"",@progbits
