with open('src/idt.c', 'r', encoding='utf-8') as f:
    text = f.read()

bad_block = '''    for (u8 i = 0; i < 16; ++i) pic_set_mask(i);
    }
    pic_clear_mask(0); /* Enable PIT IRQ0 */
    for (int i=0; i<0; ++i) { /* stub to absorb closing parenthesis if any */'''

good_block = '''    for (u8 i = 0; i < 16; ++i) pic_set_mask(i);
    
    /* Config PIT Timer for 100hz */
    u32 divisor = 1193180 / 100;
    io_out8(0x43, 0x36);
    io_out8(0x40, (u8)(divisor & 0xFF));
    io_out8(0x40, (u8)((divisor >> 8) & 0xFF));
    
    pic_clear_mask(0); /* Enable PIT IRQ0 */'''

text = text.replace(bad_block, good_block)

with open('src/idt.c', 'w', encoding='utf-8') as f:
    f.write(text)
