#include "../include/editor.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/input.h"
#include "../include/util.h"
#include "../include/mode.h"

/* ---- Editor state ---- */
static int  editor_file_index = -1;
static char editor_buffer[4096];
static int  editor_len = 0;
static int  editor_cursor_line = 0;
static int  editor_cursor_col = 0;
static int  editor_desired_col = 0;
static int  editor_scroll = 0;
static int  editor_modified = 0;
static int  editor_readonly = 0;

static char status_msg[80] = "Ready";
static u8   status_attr = ATTR_NORMAL;

/* Rows:
 * y=0           title bar
 * y=1..HEIGHT-3 content area
 * y=HEIGHT-2    shortcuts/help row
 * y=HEIGHT-1    status row
 */
#define VIEW_H (HEIGHT - 3)
#define TAB_SIZE 4

#define CTRL_S 19
#define CTRL_Q 17

/* ---- Buffer helpers ---- */
static int get_line_start(int line) {
    if (line <= 0) return 0;
    int off = 0;
    int l = 0;
    while (off < editor_len && l < line) {
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

static int get_total_lines(void) {
    int lines = 1;
    for (int i = 0; i < editor_len; ++i) if (editor_buffer[i] == '\n') lines++;
    return lines;
}

static int cursor_to_offset(void) {
    int off = get_line_start(editor_cursor_line);
    int ll = get_line_length_at_off(off);
    if (editor_cursor_col > ll) return off + ll;
    if (editor_cursor_col < 0) return off;
    return off + editor_cursor_col;
}

static void offset_to_cursor(int off, int* out_line, int* out_col) {
    int line = 0;
    int col = 0;
    if (off < 0) off = 0;
    if (off > editor_len) off = editor_len;

    for (int i = 0; i < off; ++i) {
        if (editor_buffer[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }

    *out_line = line;
    *out_col = col;
}

static void ensure_cursor_visible(void) {
    if (editor_cursor_line < editor_scroll) editor_scroll = editor_cursor_line;
    if (editor_cursor_line >= editor_scroll + VIEW_H) editor_scroll = editor_cursor_line - VIEW_H + 1;
    if (editor_scroll < 0) editor_scroll = 0;
}

static void set_status(const char* msg, u8 attr) {
    kstrncpy(status_msg, msg, (int)sizeof(status_msg));
    status_attr = attr;
}

static void insert_char_at(int off, char ch) {
    if (editor_len >= (int)sizeof(editor_buffer) - 1) {
        set_status("Buffer full", ATTR_ERROR);
        return;
    }

    for (int i = editor_len; i > off; --i) editor_buffer[i] = editor_buffer[i - 1];
    editor_buffer[off] = ch;
    editor_len++;
    editor_buffer[editor_len] = '\0';
    editor_modified = 1;
}

static void delete_char_at(int off) {
    if (off < 0 || off >= editor_len) return;
    for (int i = off; i < editor_len - 1; ++i) editor_buffer[i] = editor_buffer[i + 1];
    editor_len--;
    editor_buffer[editor_len] = '\0';
    editor_modified = 1;
}

static int editor_save(void) {
    if (editor_file_index < 0 || editor_file_index >= fs_count()) {
        set_status("No file open", ATTR_ERROR);
        return 0;
    }
    if (editor_readonly) {
        set_status("Read-only file: cannot save", ATTR_ERROR);
        return 0;
    }

    struct File* f = fs_get(editor_file_index);
    int r = fs_write(f->name, editor_buffer);
    if (r >= 0) {
        editor_modified = 0;
        set_status("Saved", ATTR_SUCCESS);
        return 1;
    }

    if (r == FS_ERR_RDONLY) set_status("Read-only file: cannot save", ATTR_ERROR);
    else set_status("Save failed", ATTR_ERROR);
    return 0;
}

static int prompt_discard_or_save(void) {
    if (!editor_modified) return 1;

    const char* prompt = "Unsaved changes: S=Save+Exit  D=Discard  C=Cancel";
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', ATTR_TITLE);
    for (int i = 0; prompt[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, HEIGHT - 1, prompt[i], ATTR_PROMPT);
    vga_flush();

    for (;;) {
        int k = wait_key();
        if (k == 's' || k == 'S' || k == CTRL_S) {
            if (editor_save()) return 1;
            return 0;
        }
        if (k == 'd' || k == 'D') return 1;
        if (k == 'c' || k == 'C' || k == K_ESC) return 0;
    }
}

/* ---- Drawing ---- */
void editor_draw(void) {
    vga_clear();

    /* Title */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', ATTR_TITLE);
    const char* title = "NoirOS Editor";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], ATTR_TITLE);

    /* Content area */
    for (int row = 0; row < VIEW_H; ++row) {
        int line_no = editor_scroll + row;
        int off = get_line_start(line_no);

        for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 1 + row, ' ', ATTR_NORMAL);
        if (off >= editor_len) continue;

        int max_off = off + get_line_length_at_off(off);
        int screen_col = 0;
        for (int i = off; i < max_off && screen_col < WIDTH; ++i) {
            char ch = editor_buffer[i];
            if (ch == '\t') {
                int spaces = TAB_SIZE - (screen_col % TAB_SIZE);
                while (spaces-- > 0 && screen_col < WIDTH) {
                    vga_putcell(screen_col, 1 + row, ' ', ATTR_NORMAL);
                    screen_col++;
                }
            } else if (ch >= 32 && ch <= 126) {
                vga_putcell(screen_col, 1 + row, ch, ATTR_NORMAL);
                screen_col++;
            } else {
                vga_putcell(screen_col, 1 + row, '.', ATTR_BORDER);
                screen_col++;
            }
        }
    }

    /* Cursor */
    int screen_line = editor_cursor_line - editor_scroll;
    if (screen_line >= 0 && screen_line < VIEW_H) {
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        int col = editor_cursor_col;
        if (col < 0) col = 0;
        if (col > ll) col = ll;

        int screen_col = 0;
        for (int i = 0; i < col; ++i) {
            char ch = editor_buffer[off + i];
            if (ch == '\t') screen_col += TAB_SIZE - (screen_col % TAB_SIZE);
            else screen_col++;
        }
        if (screen_col >= WIDTH) screen_col = WIDTH - 1;

        char cur_ch = (col < ll) ? editor_buffer[off + col] : ' ';
        if (cur_ch < 32 || cur_ch > 126) cur_ch = ' ';
        vga_putcell(screen_col, 1 + screen_line, cur_ch, ATTR_SELECTED);
    }

    /* Help row */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 2, ' ', ATTR_VIEWER_TITLE);
    const char* help = "Type to edit | Arrows/Home/End/PgUp/PgDn | F2/Ctrl+S Save | F3 Save+Exit | Esc Exit";
    for (int i = 0; help[i] && i < WIDTH - 1; ++i) vga_putcell(i, HEIGHT - 2, help[i], ATTR_VIEWER_TITLE);

    /* Status row */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, HEIGHT - 1, ' ', status_attr);

    const char* fname = "untitled";
    if (editor_file_index >= 0 && editor_file_index < fs_count()) fname = fs_get(editor_file_index)->name;

    char status[WIDTH + 1];
    int p = 0;
    if (p < WIDTH) status[p++] = '[';
    for (int i = 0; fname[i] && p < WIDTH; ++i) status[p++] = fname[i];
    if (p < WIDTH) status[p++] = ']';
    if (p < WIDTH) status[p++] = ' ';

    if (editor_readonly) {
        const char* ro = "RO ";
        for (int i = 0; ro[i] && p < WIDTH; ++i) status[p++] = ro[i];
    }
    if (editor_modified) {
        const char* m = "*modified* ";
        for (int i = 0; m[i] && p < WIDTH; ++i) status[p++] = m[i];
    }

    const char* ln = "Ln ";
    for (int i = 0; ln[i] && p < WIDTH; ++i) status[p++] = ln[i];
    p += int_to_dec(&status[p], editor_cursor_line + 1);
    if (p < WIDTH) status[p++] = ',';
    if (p < WIDTH) status[p++] = ' ';
    const char* col = "Col ";
    for (int i = 0; col[i] && p < WIDTH; ++i) status[p++] = col[i];
    p += int_to_dec(&status[p], editor_cursor_col + 1);

    if (p < WIDTH) {
        if (p < WIDTH) status[p++] = ' ';
        if (p < WIDTH) status[p++] = '|';
        if (p < WIDTH) status[p++] = ' ';
    }

    for (int i = 0; status_msg[i] && p < WIDTH; ++i) status[p++] = status_msg[i];
    while (p < WIDTH) status[p++] = ' ';

    for (int i = 0; i < WIDTH; ++i) vga_putcell(i, HEIGHT - 1, status[i], status_attr);
    vga_flush();
}

/* ---- Open ---- */
void editor_open(const char* fname) {
    int idx = -1;
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, fname) == 0) { idx = i; break; }
    }
    if (idx < 0) return;

    editor_file_index = idx;
    struct File* f = fs_get(idx);

    int max = (int)sizeof(editor_buffer) - 1;
    int len = (f->length < max) ? f->length : max;
    for (int i = 0; i < len; ++i) editor_buffer[i] = f->content[i];
    editor_len = len;
    editor_buffer[editor_len] = '\0';

    editor_cursor_line = 0;
    editor_cursor_col = 0;
    editor_desired_col = 0;
    editor_scroll = 0;
    editor_modified = 0;
    editor_readonly = f->readonly ? 1 : 0;
    if (editor_readonly) set_status("Read-only file", ATTR_ERROR);
    else set_status("Ready", ATTR_PROMPT);

    editor_draw();
}

static void app_editor_init(const char* args) {
    editor_open(args);
}


/* ---- Key handler ---- */
int editor_handle_key(int key) {
    if (key == 0) return APP_STATUS_RUNNING;

    /* Save */
    if (key == CTRL_S || key == K_F2) {
        editor_save();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    /* Save + Exit */
    if (key == K_F3) {
        if (!editor_readonly && editor_modified) editor_save();
        if (!editor_modified || editor_readonly) {
            editor_file_index = -1;
            return APP_STATUS_EXIT;
        }
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    /* Exit */
    if (key == CTRL_Q || key == K_ESC) {
        if (prompt_discard_or_save()) {
            editor_file_index = -1;
            return APP_STATUS_EXIT;
        }
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    /* Navigation */
    if (key == K_ARROW_LEFT) {
        if (editor_cursor_col > 0) {
            editor_cursor_col--;
        } else if (editor_cursor_line > 0) {
            editor_cursor_line--;
            int off = get_line_start(editor_cursor_line);
            editor_cursor_col = get_line_length_at_off(off);
        }
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_ARROW_RIGHT) {
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        if (editor_cursor_col < ll) {
            editor_cursor_col++;
        } else if (editor_cursor_line < get_total_lines() - 1) {
            editor_cursor_line++;
            editor_cursor_col = 0;
        }
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_ARROW_UP) {
        if (editor_cursor_line > 0) editor_cursor_line--;
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        if (editor_desired_col > ll) editor_cursor_col = ll;
        else editor_cursor_col = editor_desired_col;
        ensure_cursor_visible();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_ARROW_DOWN) {
        int total = get_total_lines();
        if (editor_cursor_line < total - 1) editor_cursor_line++;
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        if (editor_desired_col > ll) editor_cursor_col = ll;
        else editor_cursor_col = editor_desired_col;
        ensure_cursor_visible();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_HOME) {
        editor_cursor_col = 0;
        editor_desired_col = 0;
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_END) {
        int off = get_line_start(editor_cursor_line);
        editor_cursor_col = get_line_length_at_off(off);
        editor_desired_col = editor_cursor_col;
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_PAGE_UP) {
        editor_scroll -= VIEW_H;
        if (editor_scroll < 0) editor_scroll = 0;
        editor_cursor_line = editor_scroll;
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        if (editor_cursor_col > ll) editor_cursor_col = ll;
        editor_desired_col = editor_cursor_col;
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_PAGE_DOWN) {
        int total = get_total_lines();
        editor_scroll += VIEW_H;
        if (editor_scroll > total - 1) editor_scroll = total - 1;
        if (editor_scroll < 0) editor_scroll = 0;
        editor_cursor_line = editor_scroll;
        int off = get_line_start(editor_cursor_line);
        int ll = get_line_length_at_off(off);
        if (editor_cursor_col > ll) editor_cursor_col = ll;
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    /* Read-only guard for editing actions */
    if (editor_readonly) {
        if (key == '\b' || key == K_DEL || key == '\n' || key == '\r' || key == '\t' || (key >= 32 && key <= 126)) {
            set_status("Read-only file", ATTR_ERROR);
            editor_draw();
            return APP_STATUS_RUNNING;
        }
    }

    /* Editing */
    if (key == '\b') {
        int off = cursor_to_offset();
        if (off > 0) {
            delete_char_at(off - 1);
            offset_to_cursor(off - 1, &editor_cursor_line, &editor_cursor_col);
            editor_desired_col = editor_cursor_col;
            ensure_cursor_visible();
            set_status("Edited", ATTR_PROMPT);
        }
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == K_DEL) {
        int off = cursor_to_offset();
        if (off < editor_len) {
            delete_char_at(off);
            set_status("Edited", ATTR_PROMPT);
        }
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == '\n' || key == '\r') {
        int off = cursor_to_offset();
        insert_char_at(off, '\n');
        offset_to_cursor(off + 1, &editor_cursor_line, &editor_cursor_col);
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        set_status("Edited", ATTR_PROMPT);
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key == '\t') {
        int off = cursor_to_offset();
        for (int i = 0; i < TAB_SIZE; ++i) insert_char_at(off + i, ' ');
        offset_to_cursor(off + TAB_SIZE, &editor_cursor_line, &editor_cursor_col);
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        set_status("Edited", ATTR_PROMPT);
        editor_draw();
        return APP_STATUS_RUNNING;
    }

    if (key >= 32 && key <= 126) {
        int off = cursor_to_offset();
        insert_char_at(off, (char)key);
        offset_to_cursor(off + 1, &editor_cursor_line, &editor_cursor_col);
        editor_desired_col = editor_cursor_col;
        ensure_cursor_visible();
        set_status("Edited", ATTR_PROMPT);
        editor_draw();
        return APP_STATUS_RUNNING;
    }
    return APP_STATUS_RUNNING;
}


#ifdef EDITOR_MOUSE_SUPPORT
void editor_set_cursor_pos(int x, int y) {
    if (y < 1 || y > HEIGHT - 3) return;
    if (x < 0) x = 0;
    if (x >= WIDTH) x = WIDTH - 1;

    editor_cursor_line = editor_scroll + (y - 1);
    if (editor_cursor_line < 0) editor_cursor_line = 0;

    int off = get_line_start(editor_cursor_line);
    int ll = get_line_length_at_off(off);
    editor_cursor_col = x;
    if (editor_cursor_col > ll) editor_cursor_col = ll;
    if (editor_cursor_col < 0) editor_cursor_col = 0;

    editor_desired_col = editor_cursor_col;
    ensure_cursor_visible();
    editor_draw();
}
#endif

sys_app_t app_editor = { "editor", app_editor_init, 0, editor_draw, editor_handle_key };
