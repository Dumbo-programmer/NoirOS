#include "../include/editor.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/input.h"
#include "../include/util.h"
#include "../include/mode.h"  /* MODE_BROWSER / MODE_EDITOR */

/* ---- Editor state ---- */
static int  editor_file_index = -1;
static char editor_buffer[4096];
static int  editor_len      = 0;
static int  editor_cursor_x = 0;
static int  editor_cursor_y = 0;
static int  editor_scroll   = 0;
static int  editor_modified = 0;

/* Visible editing region (inside the border/title rows).
 * Title bar at y=0, content rows 1..(HEIGHT-2), status at y=HEIGHT-1.
 * VIEW_W excludes the 1-cell left border column. */
#define VIEW_W (WIDTH - 2)
#define VIEW_H (HEIGHT - 3)   /* rows between title (0) and status (HEIGHT-1) */

/* Maximum characters per on-screen line buffer (VIEW_W + NUL). */
#define LINEBUF_SIZE (VIEW_W + 1)

/* Ctrl key values: read_key() returns ch-'a'+1 for Ctrl+letter. */
#define CTRL_S 19   /* 's' - 'a' + 1 */
#define CTRL_X 24   /* 'x' - 'a' + 1 */

/* ---- Buffer helpers ---- */

/**
 * Return the buffer offset of the start of logical line `line`.
 * @param line line index (0-based)
 * @return buffer offset where the line begins
 */
static int get_line_start(int line) {
    int off = 0, l = 0;
    while (l < line && off < editor_len) {
        if (editor_buffer[off] == '\n') l++;
        off++;
    }
    return off;
}

/**
 * Return the number of characters on the line that starts at buffer offset `off`.
 * Does not include the terminating newline.
 */
static int get_line_length_at_off(int off) {
    int len = 0;
    while (off + len < editor_len && editor_buffer[off + len] != '\n') len++;
    return len;
}

/**
 * Convert the 2D cursor position (editor_cursor_x, editor_cursor_y)
 * to a 1D buffer offset within `editor_buffer`.
 */
static int cursor_to_offset(void) {
    int off      = get_line_start(editor_cursor_y);
    int line_len = get_line_length_at_off(off);
    if (editor_cursor_x > line_len) return off + line_len;
    return off + editor_cursor_x;
}

/**
 * Insert character `ch` at buffer offset `off`, shifting contents right.
 * Truncates if buffer is full.
 */
static void insert_char_at(int off, char ch) {
    if (editor_len >= (int)sizeof(editor_buffer) - 1) return;
    for (int i = editor_len; i > off; --i) editor_buffer[i] = editor_buffer[i - 1];
    editor_buffer[off]       = ch;
    editor_buffer[++editor_len] = '\0';
    editor_modified = 1;
}

/**
 * Delete the character immediately before offset `off` (backspace semantics).
 */
static void delete_char_before(int off) {
    if (off <= 0) return;
    for (int i = off - 1; i < editor_len - 1; ++i) editor_buffer[i] = editor_buffer[i + 1];
    editor_buffer[--editor_len] = '\0';
    editor_modified = 1;
}

/* ---- Drawing ---- */

/**
 * Redraw the editor UI: title bar, visible text window, cursor and status.
 */
void editor_draw(void) {
    ui_clear();

    /* Title bar */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
    const char *title = "NoirOS Editor  Ctrl+S save  Ctrl+X exit";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i)
        vga_putcell(1 + i, 0, title[i], 0x1F);

    /* Content: VIEW_H lines starting at editor_scroll */
    for (int ln = 0; ln < VIEW_H; ++ln) {
        int line_no = editor_scroll + ln;
        int off     = get_line_start(line_no);

        if (off >= editor_len) {
            /* Past end of file — blank line */
            draw_text_in_win(0, 1, WIDTH, HEIGHT - 1, 0, ln, "", 0x07);
            continue;
        }

        int llen = get_line_length_at_off(off);
        char linebuf[LINEBUF_SIZE];
        int copy_len = (llen < VIEW_W) ? llen : VIEW_W;
        for (int i = 0; i < copy_len; ++i) linebuf[i] = editor_buffer[off + i];
        linebuf[copy_len] = '\0';
        draw_text_in_win(0, 1, WIDTH, HEIGHT - 1, 0, ln, linebuf, 0x07);
    }

    /* Cursor: draw as a reversed cell */
    int cursor_screen_line = editor_cursor_y - editor_scroll;
    if (cursor_screen_line >= 0 && cursor_screen_line < VIEW_H) {
        int off      = get_line_start(editor_cursor_y);
        int col      = editor_cursor_x;
        int line_len = get_line_length_at_off(off);
        if (col > line_len) col = line_len;
        char cur_ch = (col < line_len) ? editor_buffer[off + col] : ' ';
        /* Content rows start at screen row 1 (row 0 is the title bar). */
        vga_putcell(1 + col, 1 + cursor_screen_line, cur_ch, 0x70);
    }

    /* Status bar: filename + modified flag */
    const char *fname = (editor_file_index >= 0 && editor_file_index < fs_count())
                        ? fs_get(editor_file_index)->name : "untitled";
    char status[WIDTH];
    int  p = 0;
    for (int i = 0; fname[i] && p < WIDTH - 3; ++i) status[p++] = fname[i];
    if (editor_modified && p < WIDTH - 1) status[p++] = '*';
    status[p] = '\0';
    /* Clear status row first */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', 0x07);
    for (int i = 0; status[i]; ++i)
        vga_putcell(1 + i, HEIGHT - 1, status[i], 0x0F);
}

/* ---- Open ---- */

/**
 * Open file `fname` into the editor buffer and switch mode to editor.
 * Copies up to the internal buffer size.
 * @param fname filename to open
 * @param mode pointer to mode variable (set to MODE_EDITOR)
 */
void editor_open(const char *fname, int *mode) {
    int idx = -1;
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, fname) == 0) { idx = i; break; }
    }
    if (idx == -1) return;

    editor_file_index = idx;
    struct File *f    = fs_get(idx);
    int max           = (int)sizeof(editor_buffer) - 1;
    int len           = (f->length < max) ? f->length : max;
    for (int i = 0; i < len; ++i) editor_buffer[i] = f->content[i];
    editor_len = len;
    editor_buffer[editor_len] = '\0';

    editor_cursor_x = editor_cursor_y = editor_scroll = 0;
    editor_modified = 0;

    *mode = MODE_EDITOR;
    editor_draw();
}

/* ---- Key handler ---- */

/**
 * Handle a single keypress while in editor mode.
 * Supports navigation, editing, and Ctrl+S/Ctrl+X shortcuts.
 * @param key key value from read_key()
 * @param mode pointer to mode variable (may be changed to MODE_BROWSER)
 */
void editor_handle_key(int key, int *mode) {
    /* Ctrl+S and Ctrl+X are detected by the numeric value read_key() returns
     * for Ctrl+letter (letter - 'a' + 1), NOT by checking is_ctrl_pressed()
     * separately, because read_key() already encodes both. */
    if (key == CTRL_S) {
        if (editor_file_index != -1) {
            struct File *f = fs_get(editor_file_index);
            fs_write(f->name, editor_buffer);
            editor_modified = 0;
            /* Briefly show "Saved!" on the status bar (no blocking read). */
            const char *msg = "Saved!";
            for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', 0x07);
            for (int i = 0; msg[i]; ++i)
                vga_putcell(1 + i, HEIGHT - 1, msg[i], 0x0A);
        }
        return;
    }

    if (key == CTRL_X) {
        editor_file_index = -1;
        *mode = MODE_BROWSER;
        return; /* caller (kernel_main) will call ui_draw() on mode change */
    }

    /* Movement */
    if (key == K_ARROW_UP) {
        if (editor_cursor_y > 0) editor_cursor_y--;
        if (editor_cursor_y < editor_scroll) editor_scroll--;
        /* Clamp cursor_x to new line */
        {
            int off = get_line_start(editor_cursor_y);
            int ll  = get_line_length_at_off(off);
            if (editor_cursor_x > ll) editor_cursor_x = ll;
        }
    } else if (key == K_ARROW_DOWN) {
        editor_cursor_y++;
        if (editor_cursor_y >= editor_scroll + VIEW_H) editor_scroll++;
        {
            int off = get_line_start(editor_cursor_y);
            int ll  = get_line_length_at_off(off);
            if (editor_cursor_x > ll) editor_cursor_x = ll;
        }
    } else if (key == K_ARROW_LEFT) {
        if (editor_cursor_x > 0) {
            editor_cursor_x--;
        } else if (editor_cursor_y > 0) {
            editor_cursor_y--;
            if (editor_cursor_y < editor_scroll) editor_scroll--;
            int off = get_line_start(editor_cursor_y);
            editor_cursor_x = get_line_length_at_off(off);
        }
    } else if (key == K_ARROW_RIGHT) {
        int off      = get_line_start(editor_cursor_y);
        int line_len = get_line_length_at_off(off);
        if (editor_cursor_x < line_len) {
            editor_cursor_x++;
        } else {
            editor_cursor_y++;
            editor_cursor_x = 0;
            if (editor_cursor_y >= editor_scroll + VIEW_H) editor_scroll++;
        }
    } else if (key == K_PAGE_UP) {
        editor_scroll -= VIEW_H;
        if (editor_scroll < 0) editor_scroll = 0;
        editor_cursor_y = editor_scroll;
        {
            int off = get_line_start(editor_cursor_y);
            int ll  = get_line_length_at_off(off);
            if (editor_cursor_x > ll) editor_cursor_x = ll;
        }
    } else if (key == K_PAGE_DOWN) {
        editor_scroll += VIEW_H;
        editor_cursor_y = editor_scroll;
        {
            int off = get_line_start(editor_cursor_y);
            int ll  = get_line_length_at_off(off);
            if (editor_cursor_x > ll) editor_cursor_x = ll;
        }

    /* Editing */
    } else if (key == '\b') {
        int off = cursor_to_offset();
        if (off > 0) {
            delete_char_before(off);
            if (editor_cursor_x > 0) {
                editor_cursor_x--;
            } else if (editor_cursor_y > 0) {
                editor_cursor_y--;
                if (editor_cursor_y < editor_scroll) editor_scroll--;
                editor_cursor_x = get_line_length_at_off(get_line_start(editor_cursor_y));
            }
        }
    } else if (key == '\n' || key == '\r') {
        int off = cursor_to_offset();
        insert_char_at(off, '\n');
        editor_cursor_y++;
        editor_cursor_x = 0;
        if (editor_cursor_y >= editor_scroll + VIEW_H) editor_scroll++;
    } else if (key >= 32 && key <= 126) {
        int off = cursor_to_offset();
        insert_char_at(off, (char)key);
        editor_cursor_x++;
        /* Clamp to column boundary */
        if (editor_cursor_x > VIEW_W - 1) editor_cursor_x = VIEW_W - 1;
    }

    editor_draw();
}

/* ---- Mouse support (optional) ---- */
#ifdef EDITOR_MOUSE_SUPPORT
/**
 * Set the editor cursor position from a mouse click (screen coordinates).
 * Converts screen x/y into logical editor coordinates and clamps to bounds.
 */
void editor_set_cursor_pos(int x, int y) {
    /* Content area: x in [1, WIDTH-2], y in [1, VIEW_H] (row 0 is title). */
    if (x < 1 || x >= WIDTH - 1) return;
    if (y < 1 || y > VIEW_H)     return;

    editor_cursor_x = x - 1;               /* strip left border column */
    editor_cursor_y = (y - 1) + editor_scroll; /* screen row → logical line */

    /* Clamp x to actual line length */
    int off      = get_line_start(editor_cursor_y);
    int line_len = get_line_length_at_off(off);
    if (editor_cursor_x < 0)        editor_cursor_x = 0;
    if (editor_cursor_x > line_len)  editor_cursor_x = line_len;
    if (editor_cursor_y < 0)        editor_cursor_y = 0;

    editor_draw();
}
#endif /* EDITOR_MOUSE_SUPPORT */
