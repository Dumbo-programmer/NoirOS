import re

with open('src/start.S', 'r', encoding='utf-8') as f:
    text = f.read()

# Replace irq1_stub macro calls to include irq0
if 'irq0_stub' not in text:
    text = text.replace('MAKE_IRQ_STUB irq1_stub, 33', 'MAKE_IRQ_STUB irq0_stub, 32\nMAKE_IRQ_STUB irq1_stub, 33')

# Replace isr_common to use eax for esp
old_isr = '''isr_common:
    pusha
    pushl %esp
    call isr_dispatch_c
    addl , %esp
    popa
    addl , %esp
    iret'''

new_isr = '''isr_common:
    pusha
    pushl %esp
    call isr_dispatch_c
    movl %eax, %esp  /* Set stack strictly to returned value (allows context switch) */
    popa
    addl , %esp
    iret'''

text = text.replace(old_isr, new_isr)

with open('src/start.S', 'w', encoding='utf-8') as f:
    f.write(text)
