#include "../include/editor.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/input.h"
#include "../include/util.h"
#include "../include/mode.h"   /* for MODE_BROWSER / MODE_EDITOR */

/* Buffer */
static int editor_file_index = -1;
static char editor_buffer[4096];
static int editor_len = 0;
static int editor_cursor_x = 0, editor_cursor_y = 0;
static int editor_scroll = 0;
static int editor_modified = 0;

static const int VIEW_W = 76; /* columns for editing region */
static const int VIEW_H = 20; /* lines visible */

static int get_line_start(int line) {
    int off = 0;
    int l = 0;
    while (l < line && off < editor_len) {
        if (editor_buffer[off] == '\n') l++;
        off++;
    }
    return off;
}

static int get_line_length_at_off(int off) {
    int len = 0;
    while (off + len < editor_len && editor_buffer[off + len] != '\n') len++;
    return len;
}

/* convert cursor (x,y) to buffer offset */
static int cursor_to_offset(void) {
    int off = get_line_start(editor_cursor_y);
    int line_len = get_line_length_at_off(off);
    if (editor_cursor_x > line_len) return off + line_len;
    return off + editor_cursor_x;
}

/* insert char at offset */
static void insert_char_at(int off, char ch) {
    if (editor_len >= (int)sizeof(editor_buffer) - 1) return;
    for (int i = editor_len; i > off; --i) editor_buffer[i] = editor_buffer[i - 1];
    editor_buffer[off] = ch;
    editor_len++;
    editor_buffer[editor_len] = '\0';
    editor_modified = 1;
}

/* delete char before offset (backspace) */
static void delete_char_before(int off) {
    if (off <= 0) return;
    for (int i = off - 1; i < editor_len - 1; ++i) editor_buffer[i] = editor_buffer[i + 1];
    editor_len--;
    editor_buffer[editor_len] = '\0';
    editor_modified = 1;
}

/* redraw editor view */
void editor_draw(void) {
    ui_clear();

    /* Title */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
    const char* title = "NoirOS Editor - Ctrl+S save, Ctrl+X exit";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], 0x1F);

    /* show lines from editor_buffer starting at editor_scroll */
    for (int ln = 0; ln < VIEW_H; ++ln) {
        int line_no = editor_scroll + ln;
        int off = get_line_start(line_no);
        if (off >= editor_len) {
            /* empty line: blank area */
            draw_text_in_win(0, 2, WIDTH, HEIGHT - 3, 0, ln, "", 0x07);
            continue;
        }
        int llen = get_line_length_at_off(off);
        #define LINEBUF_SIZE 77
        char linebuf[LINEBUF_SIZE];
        int copy_len = (llen < VIEW_W) ? llen : VIEW_W;
        for (int i = 0; i < copy_len; ++i) linebuf[i] = editor_buffer[off + i];
        linebuf[copy_len] = '\0';
        draw_text_in_win(0, 2, WIDTH, HEIGHT - 3, 0, ln, linebuf, 0x07);
    }

    /* draw cursor visually as inverted cell */
    int cursor_screen_line = editor_cursor_y - editor_scroll;
    if (cursor_screen_line >= 0 && cursor_screen_line < VIEW_H) {
        int off = get_line_start(editor_cursor_y);
        int col = editor_cursor_x;
        int line_len = get_line_length_at_off(off);
        if (col > line_len) col = line_len;
        vga_putcell(1 + col, 2 + cursor_screen_line, (col < line_len) ? editor_buffer[off + col] : ' ', 0x70);
    }

    /* status */
    const char* fname = (editor_file_index >= 0 && editor_file_index < fs_count()) ? fs_get(editor_file_index)->name : "untitled";
    char status[80];
    int p=0;
    for (int i=0; fname[i] && p < 60; ++i) status[p++]=fname[i];
    if (editor_modified && p < 78) { status[p++]='*'; }
    status[p]=0;
    for (int i = 0; status[i]; ++i) vga_putcell(1 + i, HEIGHT - 1, status[i], 0x0F);
}

/* open file */
void editor_open(const char *fname, int *mode) {
    int idx = -1;
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, fname) == 0) { idx = i; break; }
    }
    if (idx == -1) return;

    editor_file_index = idx;
    struct File* f = fs_get(idx);
    int len = (f->length < (int)(sizeof(editor_buffer)-1)) ? f->length : (int)(sizeof(editor_buffer)-1);
    for (int i = 0; i < len; ++i) editor_buffer[i] = f->content[i];
    editor_len = len;
    editor_buffer[editor_len] = 0;

    editor_cursor_x = editor_cursor_y = editor_scroll = 0;
    editor_modified = 0;
    
    /* Define MODE_EDITOR if not already defined */
    #ifndef MODE_EDITOR
    #define MODE_EDITOR 1
    #endif
    
    *mode = MODE_EDITOR;
    editor_draw();
}

void editor_handle_key(int key, int *mode) {
    if (is_ctrl_pressed()) {
        if (key == 19 && editor_file_index != -1) { /* Ctrl+S */
            struct File* f = fs_get(editor_file_index);
            fs_write(f->name, editor_buffer);
            editor_modified = 0;
            const char *msg = "Saved!";
            for (int i=0; msg[i]; ++i) vga_putcell(1+i,24,msg[i],0x0A);
            return;
        } else if (key == 24) { /* Ctrl+X exit */
            /* Switch back to browser mode and clear editor state */
            editor_file_index = -1;
            *mode = MODE_BROWSER;
            ui_draw(); /* redraw the explorer */
            return;
        } else {
            return;
        }
    }

    if (key == K_ARROW_UP) {
        if (editor_cursor_y > 0) editor_cursor_y--;
        if (editor_cursor_y < editor_scroll) editor_scroll--;
        /* clamp cursor_x to the new line length */
        {
            int off = get_line_start(editor_cursor_y);
            int line_len = get_line_length_at_off(off);
            if (editor_cursor_x > line_len) editor_cursor_x = line_len;
        }
    } else if (key == K_ARROW_DOWN) {
        editor_cursor_y++;
        if (editor_cursor_y >= editor_scroll + VIEW_H) editor_scroll++;
        /* clamp cursor_x */
        {
            int off = get_line_start(editor_cursor_y);
            int line_len = get_line_length_at_off(off);
            if (editor_cursor_x > line_len) editor_cursor_x = line_len;
        }
    } else if (key == K_ARROW_LEFT) {
        if (editor_cursor_x > 0) {
            editor_cursor_x--;
        } else if (editor_cursor_y > 0) {
            /* move to end of previous line */
            editor_cursor_y--;
            if (editor_cursor_y < editor_scroll) editor_scroll--;
            int off = get_line_start(editor_cursor_y);
            editor_cursor_x = get_line_length_at_off(off);
        }
    } else if (key == K_ARROW_RIGHT) {
        int off = get_line_start(editor_cursor_y);
        int line_len = get_line_length_at_off(off);
        if (editor_cursor_x < line_len) {
            editor_cursor_x++;
        } else {
            /* move to next line start */
            editor_cursor_y++;
            editor_cursor_x = 0;
            if (editor_cursor_y >= editor_scroll + VIEW_H) editor_scroll++;
        }
    } 
    /* page up/down, backspace, enter, printable chars remain same but ensure clamping after edits */
    else if (key == K_PAGE_UP) {
        editor_scroll -= VIEW_H;
        if (editor_scroll < 0) editor_scroll = 0;
        editor_cursor_y = editor_scroll;
        int off = get_line_start(editor_cursor_y);
        int len = get_line_length_at_off(off);
        if (editor_cursor_x > len) editor_cursor_x = len;
    } else if (key == K_PAGE_DOWN) {
        editor_scroll += VIEW_H;
        editor_cursor_y = editor_scroll;
        int off = get_line_start(editor_cursor_y);
        int len = get_line_length_at_off(off);
        if (editor_cursor_x > len) editor_cursor_x = len;
    } else if (key == '\b') {
        int off = cursor_to_offset();
        if (off > 0) {
            delete_char_before(off);
            /* move cursor back one position */
            if (editor_cursor_x > 0) editor_cursor_x--;
            else if (editor_cursor_y > 0) {
                editor_cursor_y--;
                editor_cursor_x = get_line_length_at_off(get_line_start(editor_cursor_y));
                if (editor_cursor_y < editor_scroll) editor_scroll--;
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
        if (editor_cursor_x > VIEW_W - 1) editor_cursor_x = VIEW_W - 1;
    }
}

/* Mouse support for editor (optional) */
#ifdef EDITOR_MOUSE_SUPPORT
void editor_set_cursor_pos(int x, int y) {
    /* Adjust for editor window bounds and scrolling */
    if (x >= 1 && x < 80 && y >= 2 && y < 22) {  /* Editor content area */
        /* Convert screen coordinates to editor coordinates */
        editor_cursor_x = x - 1;  /* Account for border */
        int screen_line = y - 2;  /* Account for title bar */
        editor_cursor_y = screen_line + editor_scroll;
        
        /* Clamp to actual line length */
        int off = get_line_start(editor_cursor_y);
        int line_len = get_line_length_at_off(off);
        if (editor_cursor_x > line_len) editor_cursor_x = line_len;
        
        /* Bounds checking */
        if (editor_cursor_x < 0) editor_cursor_x = 0;
        if (editor_cursor_y < 0) editor_cursor_y = 0;
    }
}
#endif /* EDITOR_MOUSE_SUPPORT */