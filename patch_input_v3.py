import re
with open('src/input.c', 'r') as f:
    text = f.read()

text = text.replace('#include "../include/common.h"', '#include "../include/common.h"\n#include "../include/mouse.h"')

new_kb = '''u8 kb_read_scancode(void) {
    for (volatile int timeout = 100000; timeout > 0; --timeout) {
        u8 status = inb(0x64);
        if (status & 0x01) {
            if (status & 0x20) {
                mouse_handler();
                continue;
            }
            u8 val = inb(0x60);
            last_scancode = val;
            return val;
        }
    }
    return 0;
}'''

text = re.sub(r'u8 kb_read_scancode\(void\) \{.*?\n\}', new_kb, text, flags=re.DOTALL)

new_read_key = '''static u8 e0_pending = 0;

int read_key(void) {
    u8 sc = kb_read_scancode();

    if (!sc) return 0;

    last_scancode = sc;

    if (e0_pending) {
        e0_pending = 0;
        if (sc & 0x80) {
            u8 rel = sc & 0x7F;
            if (rel == 0x1D) kb_state.ctrl_pressed = 0;
            if (rel == 0x38) kb_state.alt_pressed  = 0;
            return 0;
        }
        if (sc == 0x48) return K_ARROW_UP;
        if (sc == 0x50) return K_ARROW_DOWN;
        if (sc == 0x4B) return K_ARROW_LEFT;
        if (sc == 0x4D) return K_ARROW_RIGHT;
        if (sc == 0x49) return K_PAGE_UP;
        if (sc == 0x51) return K_PAGE_DOWN;
        if (sc == 0x47) return K_HOME;
        if (sc == 0x4F) return K_END;
        if (sc == 0x53) return K_DEL;
        if (sc == 0x1D) { kb_state.ctrl_pressed = 1; return 0; }
        if (sc == 0x38) { kb_state.alt_pressed  = 1; return 0; }
        if (sc == 0x2A || sc == 0x36) return 0;
        return 0;
    }

    if (sc == 0xE0) {
        e0_pending = 1;
        return 0;
    }'''

text = re.sub(r'int read_key\(void\) \{(.*?)\/\* ---- Debug dump keys ---- \*\/', new_read_key + r'\n\n    /* ---- Debug dump keys ---- */', text, flags=re.DOTALL)

with open('src/input.c', 'w') as f:
    f.write(text)
