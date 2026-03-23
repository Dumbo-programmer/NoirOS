#include "../include/input.h"
#include "../include/common.h"

static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* kb_read_scancode: read one byte from the keyboard controller.
 * Spins until the output-buffer-full bit (bit 0) of the status register
 * is set.  A timeout counter is added so a broken/absent keyboard cannot
 * stall the kernel indefinitely. */
u8 kb_read_scancode(void) {
    /* ~100 000 iterations is well above any real keyboard response time
     * at typical CPU speeds (tens of ms headroom). */
    for (volatile int timeout = 100000; timeout > 0; --timeout) {
        if (inb(0x64) & 0x01)
            return inb(0x60);
    }
    return 0; /* timeout — return a safe no-op value */
}

/* Keyboard state tracking */
static struct {
    u8 shift_pressed : 1;
    u8 ctrl_pressed  : 1;
    u8 alt_pressed   : 1;
    u8 caps_lock     : 1;
} kb_state = {0};

/* Scancode Set 1 maps (US QWERTY) */
static const char scancode_map_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',    /* 0x00-0x07 */
    '7', '8', '9', '0', '-', '=', '\b', '\t',  /* 0x08-0x0F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   /* 0x10-0x17 */
    'o', 'p', '[', ']', '\n', 0,  'a', 's',   /* 0x18-0x1F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x20-0x27 */
    '\'','`',  0,  '\\','z', 'x', 'c', 'v',   /* 0x28-0x2F */
    'b', 'n', 'm', ',', '.', '/', 0,   '*',   /* 0x30-0x37 */
    0,   ' ', 0,   0,   0,   0,   0,   0,     /* 0x38-0x3F */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x40-0x47 */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x48-0x4F */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x50-0x57 */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x58-0x5F */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x60-0x67 */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x68-0x6F */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x70-0x77 */
    0,   0,   0,   0,   0,   0,   0,   0,     /* 0x78-0x7F */
};

static const char scancode_map_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^',   /* 0x00-0x07 */
    '&', '*', '(', ')', '_', '+', '\b', '\t', /* 0x08-0x0F */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',  /* 0x10-0x17 */
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',  /* 0x18-0x1F */
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  /* 0x20-0x27 */
    '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',  /* 0x28-0x2F */
    'B', 'N', 'M', '<', '>', '?', 0,   '*',  /* 0x30-0x37 */
    0,   ' ', 0,   0,   0,   0,   0,   0,    /* 0x38-0x3F */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x40-0x47 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x48-0x4F */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x50-0x57 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x58-0x5F */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x60-0x67 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x68-0x6F */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x70-0x77 */
    0,   0,   0,   0,   0,   0,   0,   0,    /* 0x78-0x7F */
};

/* Scancode Set 1 definitions */
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36
#define SC_LCTRL     0x1D
#define SC_LALT      0x38
#define SC_CAPS_LOCK 0x3A
#define SC_SPACE     0x39

int read_key(void) {
    u8 sc = kb_read_scancode();

    /* ---- Extended (E0-prefixed) scancodes ---- */
    if (sc == 0xE0) {
        sc = kb_read_scancode();
        if (sc & 0x80) {
            /* Extended key release */
            u8 rel = sc & 0x7F;
            if (rel == 0x1D) kb_state.ctrl_pressed = 0; /* R-Ctrl */
            if (rel == 0x38) kb_state.alt_pressed  = 0; /* R-Alt  */
            return 0;
        }
        /* Extended key press — arrow/page keys appear here with E0 prefix.
         * These cases are handled exclusively in the E0 branch to avoid
         * duplicating them in the non-extended path below, which previously
         * caused the same code to fire for unrelated scancodes on some BIOSes. */
        if (sc == 0x48) return K_ARROW_UP;
        if (sc == 0x50) return K_ARROW_DOWN;
        if (sc == 0x4B) return K_ARROW_LEFT;
        if (sc == 0x4D) return K_ARROW_RIGHT;
        if (sc == 0x49) return K_PAGE_UP;
        if (sc == 0x51) return K_PAGE_DOWN;
        if (sc == 0x1D) { kb_state.ctrl_pressed = 1; return 0; } /* R-Ctrl */
        if (sc == 0x38) { kb_state.alt_pressed  = 1; return 0; } /* R-Alt  */
        return 0;
    }

    /* ---- Key release (high bit set) ---- */
    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc == SC_LSHIFT || sc == SC_RSHIFT) kb_state.shift_pressed = 0;
        if (sc == SC_LCTRL)                      kb_state.ctrl_pressed  = 0;
        if (sc == SC_LALT)                       kb_state.alt_pressed   = 0;
        return 0;
    }

    /* ---- Key press ---- */
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { kb_state.shift_pressed = 1; return 0; }
    if (sc == SC_LCTRL)                      { kb_state.ctrl_pressed  = 1; return 0; }
    if (sc == SC_LALT)                       { kb_state.alt_pressed   = 1; return 0; }
    if (sc == SC_CAPS_LOCK) { kb_state.caps_lock = !kb_state.caps_lock; return 0; }

    if (sc == 0x01)     return K_ESC;
    if (sc == SC_SPACE) return ' ';

    /* Function keys */
    if (sc == 0x3B) return K_F1;
    if (sc == 0x3C) return K_F2;
    if (sc == 0x3D) return K_F3;

    /* Note: arrow/page key scancodes (0x48, 0x4B, 0x4D, 0x49, 0x50, 0x51)
     * are intentionally NOT handled here.  On a PS/2 keyboard with Set 1,
     * these keys always send an E0 prefix first, so they are handled in the
     * E0 branch above.  Duplicating them here could match unrelated scancodes
     * on some systems and produce ghost key events. */

    /* Normal character keys */
    if (sc < 128) {
        int use_upper = kb_state.shift_pressed;
        /* Caps Lock inverts case only for letters */
        if ((sc >= 0x10 && sc <= 0x19) || /* Q-P row */
            (sc >= 0x1E && sc <= 0x26) || /* A-L row */
            (sc >= 0x2C && sc <= 0x32))   /* Z-M row */
            use_upper ^= kb_state.caps_lock;

        char ch = use_upper ? scancode_map_upper[sc] : scancode_map_lower[sc];

        /* Ctrl combinations */
        if (kb_state.ctrl_pressed) {
            if (ch >= 'a' && ch <= 'z') return ch - 'a' + 1;
            if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 1;
        }
        return ch;
    }

    return 0;
}

int is_shift_pressed(void) { return kb_state.shift_pressed; }
int is_ctrl_pressed(void)  { return kb_state.ctrl_pressed;  }
int is_alt_pressed(void)   { return kb_state.alt_pressed;   }
int is_caps_lock_on(void)  { return kb_state.caps_lock;     }

int input_readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        int ch = read_key();
        if (ch == '\n' || ch == '\r') { buf[len] = 0; return len; }
        if (ch == '\b') { if (len > 0) len--; continue; }
        if (ch >= 32 && ch <= 126 && len < max - 1) buf[len++] = (char)ch;
    }
}
