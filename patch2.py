import re

with open('src/input.c', 'r') as f:
    text = f.read()

# Make kb_read_scancode non-blocking on extended scancodes
text = re.sub(
    r'int read_key\(void\) \{(.*?)\/\* ---- Debug dump keys ---- \*\/',
    r'''static u8 e0_pending = 0;

int read_key(void) {
    u8 sc = kb_read_scancode();

    if (!sc) return 0;

    /* record raw scancode for debug */
    last_scancode = sc;

    /* ---- Handle pending Extended (E0-prefixed) scancodes ---- */
    if (e0_pending) {
        e0_pending = 0;
        if (sc & 0x80) {
            /* Extended key release */
            u8 rel = sc & 0x7F;
            if (rel == 0x1D) kb_state.ctrl_pressed = 0; /* R-Ctrl */
            if (rel == 0x38) kb_state.alt_pressed  = 0; /* R-Alt  */
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
        /* Fake shifts (L-Shift pressed with navigation keys: E0 2A ... ) */    
        if (sc == 0x2A || sc == 0x36) return 0;
        return 0;
    }

    /* ---- Extended (E0-prefixed) scancodes ---- */
    if (sc == 0xE0) {
        e0_pending = 1;
        return 0;
    }

    /* ---- Debug dump keys ---- */''',
    text,
    flags=re.DOTALL
)

with open('src/input.c', 'w') as f:
    f.write(text)
