#include "../include/input.h"
#include "../include/common.h"

static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

 u8 kb_read_scancode(void) {
    while (!(inb(0x64) & 1)) { /* spin */ }
    return inb(0x60);
}

/* Keyboard state tracking */
static struct {
    u8 shift_pressed : 1;
    u8 ctrl_pressed : 1;
    u8 alt_pressed : 1;
    u8 caps_lock : 1;
} kb_state = {0};

/* Enhanced scancode maps */
static const char scancode_map_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',     // 0x00-0x07
    '7', '8', '9', '0', '-', '=', '\b', '\t',   // 0x08-0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',    // 0x10-0x17
    'o', 'p', '[', ']', '\n', 0,  'a', 's',    // 0x18-0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    // 0x20-0x27
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',   // 0x28-0x2F
    'b', 'n', 'm', ',', '.', '/', 0,   '*',    // 0x30-0x37
    0,   ' ', 0,   0,   0,   0,   0,   0,      // 0x38-0x3F (space at 0x39)
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x40-0x47
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x48-0x4F 
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x50-0x57
};

static const char scancode_map_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^',     // 0x00-0x07
    '&', '*', '(', ')', '_', '+', '\b', '\t',   // 0x08-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',    // 0x10-0x17
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',    // 0x18-0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',    // 0x20-0x27
    '"', '~', 0,  '|',  'Z', 'X', 'C', 'V',    // 0x28-0x2F
    'B', 'N', 'M', '<', '>', '?', 0,   '*',    // 0x30-0x37
    0,   ' ', 0,   0,   0,   0,   0,   0,      // 0x38-0x3F (space at 0x39)
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x40-0x47
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x48-0x4F
    0,   0,   0,   0,   0,   0,   0,   0,      // 0x50-0x57
};

/* Scancode definitions */
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_LCTRL     0x1D
#define SC_LALT      0x38
#define SC_CAPS_LOCK 0x3A
#define SC_SPACE     0x39

/* Enhanced read_key function with full modifier support */
int read_key(void) {
    u8 sc = kb_read_scancode();
    
    /* Handle extended scancodes (0xE0 prefix) */
    if (sc == 0xE0) {
        sc = kb_read_scancode();
        /* if release, high bit will be set (e.g., 0xC8 for released Up Arrow) */
        if (sc & 0x80) {
            u8 sc_rel = sc & 0x7F;
            /* handle extended modifier releases */
            if (sc_rel == 0x1D) { /* Right Ctrl release */
                kb_state.ctrl_pressed = 0;
                return 0;
            }
            if (sc_rel == 0x38) { /* Right Alt release */
                kb_state.alt_pressed = 0;
                return 0;
            }
            /* other extended key releases: ignore for now */
            return 0;
        } else {
            /* handle extended key presses */
            if (sc == 0x48) return K_ARROW_UP;
            if (sc == 0x50) return K_ARROW_DOWN;
            if (sc == 0x4B) return K_ARROW_LEFT;
            if (sc == 0x4D) return K_ARROW_RIGHT;
            if (sc == 0x49) return K_PAGE_UP;
            if (sc == 0x51) return K_PAGE_DOWN;
            if (sc == 0x3B ) return K_F1;
            if (sc == 0x3C ) return K_F2;
            if (sc == 0x3D ) return K_F3;
            if (sc == 0x1D) { /* Right Ctrl press */
                kb_state.ctrl_pressed = 1;
                return 0;
            }
            if (sc == 0x38) { /* Right Alt press */
                kb_state.alt_pressed = 1;
                return 0;
            }
            return 0;
        }
    }
    
    /* Handle key release (high bit set) */
    if (sc & 0x80) {
        sc &= 0x7F; /* Clear release bit */
        
        /* Update modifier key states on release */
        if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
            kb_state.shift_pressed = 0;
        }
        if (sc == SC_LCTRL) {
            kb_state.ctrl_pressed = 0;
        }
        if (sc == SC_LALT) {
            kb_state.alt_pressed = 0;
        }
        return 0;
    }
    
    /* Handle key press */
    
    /* Update modifier key states on press */
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        kb_state.shift_pressed = 1;
        return 0;
    }
    if (sc == SC_LCTRL) {
        kb_state.ctrl_pressed = 1;
        return 0;
    }
    if (sc == SC_LALT) {
        kb_state.alt_pressed = 1;
        return 0;
    }
    if (sc == SC_CAPS_LOCK) {
        kb_state.caps_lock = !kb_state.caps_lock;
        return 0;
    }
    
    /* Special keys */
    if (sc == 0x01) return K_ESC;
    if (sc == SC_SPACE) return ' ';

    /* Function keys (Set 1 scancodes) */
    if (sc == 0x3B) return K_F1; /* F1 */
    if (sc == 0x3C) return K_F2; /* F2 */
    if (sc == 0x3D) return K_F3; /* F3 */
    
    /* Arrow keys (without E0 prefix) */
    if (sc == 0x48) return K_ARROW_UP;
    if (sc == 0x50) return K_ARROW_DOWN;
    if (sc == 0x4B) return K_ARROW_LEFT;
    if (sc == 0x4D) return K_ARROW_RIGHT;
    if (sc == 0x49) return K_PAGE_UP;
    if (sc == 0x51) return K_PAGE_DOWN;
    
    /* Normal character keys */
    if (sc < 128) {
        char ch;
        
        /* Determine if  uppercase */
        int use_upper = kb_state.shift_pressed;
        
        /* For letters, also consider caps lock */
        if (sc >= 0x10 && sc <= 0x19) { /* Q-P row */
            use_upper ^= kb_state.caps_lock;
        } else if (sc >= 0x1E && sc <= 0x26) { /* A-L row */
            use_upper ^= kb_state.caps_lock;
        } else if (sc >= 0x2C && sc <= 0x32) { /* Z-M row */
            use_upper ^= kb_state.caps_lock;
        }
        
        if (use_upper) {
            ch = scancode_map_upper[sc];
        } else {
            ch = scancode_map_lower[sc];
        }
        
        /* Handle Ctrl combinations */
        if (kb_state.ctrl_pressed && ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 1; /* Ctrl+A = 1, Ctrl+B = 2, etc. */
        }
        if (kb_state.ctrl_pressed && ch >= 'A' && ch <= 'Z') {
            return ch - 'A' + 1;
        }
        
        return ch;
    }
    
    return 0;
}

/* Helper functions to check modifier states */
int is_shift_pressed(void) {
    return kb_state.shift_pressed;
}

int is_ctrl_pressed(void) {
    return kb_state.ctrl_pressed;
}

int is_alt_pressed(void) {
    return kb_state.alt_pressed;
}

/* read_key and helpers */
int input_readline(char *buf, int max) {
    int len = 0;
    for (;;) {
        int ch = read_key();
        if (ch == '\n' || ch == '\r') {
            buf[len] = 0;
            return len;
        }
        if (ch == '\b') {
            if (len > 0) len--;
            continue;
        }
        if (ch >= 32 && ch <= 126) {
            if (len < max - 1) {
                buf[len++] = (char)ch;
            }
        }
    }
}

int is_caps_lock_on(void) {
    return kb_state.caps_lock;
}
