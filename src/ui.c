#include "../include/ui.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/util.h"
#include "../include/input.h"
#include "../include/viewer.h"

static Window explorer_win = {0, 1, 32, 20, " Explorer "};
/* UI window layout: explorer | viewer + controls | status bar.
 * All coordinates are in 80x25 VGA text-mode screen space. */
/* viewer is shrunk so the controls window fits below it */
static Window viewer_win   = {33, 1, 46, 14, " Viewer "};
static Window control_win  = {33, 15, 46, 6,  " Controls "};
static Window status_win   = {0, 22, WIDTH, 3,  " Status "};

static int explorer_sel = 0;
static int viewer_scroll = 0;
static int explorer_scroll = 0;
/* 0 = explorer panel has focus, 1 = viewer panel has focus */
static int active_panel = 0;

/* Button definitions for clickable controls */
typedef struct {
    int x, y, w, h;
    const char* text;
    unsigned char normal_attr;
    unsigned char pressed_attr;
    int ticks;
    void (*callback)(void);
} ui_button_t;

/* Power control buttons — declared here so ui_relayout() can reset the flag */
static ui_button_t power_buttons[3];
static int power_buttons_initialized = 0;

/* Button press duration */
#define UI_PRESS_TICKS 8

/* Forward declarations */
static void init_power_buttons(void);
static void draw_explorer_listing(int dir_count, int file_count, int total);
static void draw_viewer_content(int dir_count);

/* Set layout based on current runtime screen size (SCREEN_W/SCREEN_H). */
void ui_relayout(void) {
    /* Proportions derived from original 80x25 layout */
    int ew = SCREEN_W * 32 / 80; if (ew < 10) ew = 10;
    int vw = SCREEN_W - ew - 1; if (vw < 10) vw = SCREEN_W - ew;

    explorer_win.x = 0; explorer_win.y = 1; explorer_win.w = ew; explorer_win.h = SCREEN_H - 4;
    viewer_win.x = explorer_win.w + 1; viewer_win.y = 1; viewer_win.w = vw; viewer_win.h = (SCREEN_H - 4) * 3 / 5;
    control_win.x = viewer_win.x; control_win.y = viewer_win.y + viewer_win.h; control_win.w = viewer_win.w; control_win.h = (SCREEN_H - 4) - viewer_win.h;
    status_win.x = 0; status_win.y = SCREEN_H - 3; status_win.w = SCREEN_W; status_win.h = 3;

    /* Force button positions to be recalculated against the new window layout */
    power_buttons_initialized = 0;
}

/* -------- tiny helpers (no stdio) -------- */
static void append_str(char *buf, int *p, const char *s, int max) {
    while (*s && *p < max - 1) buf[(*p)++] = *s++;
    buf[*p] = '\0';
}
static void append_char(char *buf, int *p, char c, int max) {
    if (*p < max - 1) buf[(*p)++] = c;
    buf[*p] = '\0';
}


/* -------- selection & viewer helpers -------- */
void ui_set_selected(int sel) {
    int dir_count = fs_dir_count();
    int file_count = fs_count();
    int total = dir_count + file_count;
    if (total <= 0) { explorer_sel = 0; return; }
    if (sel < 0) sel = 0;
    if (sel >= total) sel = total - 1;
    explorer_sel = sel;

    /* Keep selected item inside the visible window */
    int e_lines = explorer_win.h - 2;
    if (explorer_sel < explorer_scroll)
        explorer_scroll = explorer_sel;
    if (explorer_sel >= explorer_scroll + e_lines)
        explorer_scroll = explorer_sel - e_lines + 1;
    if (explorer_scroll < 0) explorer_scroll = 0;

    /* Reset viewer scroll whenever selection changes */
    viewer_scroll = 0;
}
int ui_get_selected(void) { return explorer_sel; }

void ui_scroll_viewer(int delta) {
    if (viewer_scroll + delta < 0) viewer_scroll = 0;
    else viewer_scroll += delta;
}
int ui_selected_file_index(void) {
    int dir_count = fs_dir_count();
    if (explorer_sel < dir_count) return -1;
    return explorer_sel - dir_count;
}

/* Reset explorer scroll (called after changing directory) */
void ui_reset_explorer_scroll(void) {
    explorer_scroll = 0;
    viewer_scroll = 0;
}

/* Panel focus accessors */
int ui_get_active_panel(void) { return active_panel; }
void ui_toggle_active_panel(void) { active_panel = !active_panel; }

/* Page sizes used by kernel navigation */
int ui_explorer_page_size(void) { return explorer_win.h - 2; }
int ui_viewer_page_size(void) { return viewer_win.h - 2; }

/* Legacy callback setters (kept for compatibility) */
void ui_set_restart_callback(void (*cb)(void)) {
    if (power_buttons_initialized && cb) {
        power_buttons[0].callback = cb;
    }
}

void ui_set_shutdown_callback(void (*cb)(void)) {
    if (power_buttons_initialized && cb) {
        power_buttons[1].callback = cb;
    }
}

void ui_set_sleep_callback(void (*cb)(void)) {
    if (power_buttons_initialized && cb) {
        power_buttons[2].callback = cb;
    }
}


/* Called from shell_loop() immediately after read_key() so UI can react to Fn keys */
void ui_handle_key(int key) {
    /* you must have K_F1/K_F2/K_F3 defined in your input.h - used elsewhere already */
#ifdef K_F1
    if (key == K_F1) {
        power_buttons[0].ticks = UI_PRESS_TICKS;
        if (power_buttons[0].callback) power_buttons[0].callback();
        return;
    }
#endif
#ifdef K_F2
    if (key == K_F2) {
        power_buttons[1].ticks = UI_PRESS_TICKS;
        if (power_buttons[1].callback) power_buttons[1].callback();
        return;
    }
#endif
#ifdef K_F3
    if (key == K_F3) {
        power_buttons[2].ticks = UI_PRESS_TICKS;
        if (power_buttons[2].callback) power_buttons[2].callback();
        return;
    }
#endif
}

/* Mouse click handler for UI elements */
void ui_handle_mouse_click(int x, int y, int button) {
    if (button != 1) return; /* Only handle left clicks */
    
    init_power_buttons();
    
    /* Check power button clicks */
    for (int i = 0; i < 3; i++) {
        ui_button_t* btn = &power_buttons[i];
        if (x >= btn->x && x < btn->x + btn->w &&
            y >= btn->y && y < btn->y + btn->h) {
            /* Button clicked! */
            btn->ticks = UI_PRESS_TICKS;
            if (btn->callback) {
                btn->callback();
            }
            return;
        }
    }
    
    /* Check file explorer clicks */
    if (x >= explorer_win.x + 1 && x < explorer_win.x + explorer_win.w - 1 &&
        y >= explorer_win.y + 1 && y < explorer_win.y + explorer_win.h - 1) {
        
        int clicked_row = y - (explorer_win.y + 1);
        int dir_count = fs_dir_count();
        int file_count = fs_count();
        int total = dir_count + file_count;
        int clicked_index = explorer_scroll + clicked_row;
        if (clicked_index < total) {
            ui_set_selected(clicked_index);
            ui_draw(); /* Refresh to show new selection */
        }
        return;
    }
    
    /* Check viewer scroll area clicks */
    if (x >= viewer_win.x + 1 && x < viewer_win.x + viewer_win.w - 1 &&
        y >= viewer_win.y + 1 && y < viewer_win.y + viewer_win.h - 1) {
        
        /* Simple scroll: upper half scrolls up, lower half scrolls down */
        int mid_y = viewer_win.y + viewer_win.h / 2;
        if (y < mid_y) {
            ui_scroll_viewer(-1);
        } else {
            ui_scroll_viewer(1);
        }
        ui_draw(); /* Refresh to show scroll */
        return;
    }
}

/* -------- Draw Controls window (buttons with colored backgrounds) -------- */
static void init_power_buttons(void) {
    if (power_buttons_initialized) return;
    
    int btn_w = 14;
    int spacing = 2;
    int start_x = control_win.x + 1 + ((control_win.w - 2 - (3 * btn_w + 2 * spacing)) / 2);
    int btn_y = control_win.y + 1 + ((control_win.h - 2) / 2);
    
    /* Restart button */
    power_buttons[0].x = start_x;
    power_buttons[0].y = btn_y;
    power_buttons[0].w = btn_w;
    power_buttons[0].h = 1;
    power_buttons[0].text = " Restart(F1) ";
    power_buttons[0].normal_attr = VGA_ATTR(COL_WHITE, COL_GREEN);
    power_buttons[0].pressed_attr = VGA_ATTR(COL_GREEN, COL_WHITE);
    power_buttons[0].ticks = 0;
    power_buttons[0].callback = show_restart_screen;
    
    /* Shutdown button */
    power_buttons[1].x = start_x + btn_w + spacing;
    power_buttons[1].y = btn_y;
    power_buttons[1].w = btn_w;
    power_buttons[1].h = 1;
    power_buttons[1].text = " Shut Down(F2) ";
    power_buttons[1].normal_attr = VGA_ATTR(COL_WHITE, COL_RED);
    power_buttons[1].pressed_attr = VGA_ATTR(COL_RED, COL_WHITE);
    power_buttons[1].ticks = 0;
    power_buttons[1].callback = show_shutdown_screen;
    
    /* Sleep button */
    power_buttons[2].x = start_x + 2 * (btn_w + spacing);
    power_buttons[2].y = btn_y;
    power_buttons[2].w = btn_w;
    power_buttons[2].h = 1;
    power_buttons[2].text = " Sleep(F3) ";
    power_buttons[2].normal_attr = VGA_ATTR(COL_WHITE, COL_BLUE);
    power_buttons[2].pressed_attr = VGA_ATTR(COL_BLUE, COL_WHITE);
    power_buttons[2].ticks = 0;
    power_buttons[2].callback = show_sleep_screen;
    
    power_buttons_initialized = 1;
}

static void draw_controls_window(void) {
    init_power_buttons();
    
    draw_box(control_win.x, control_win.y, control_win.w, control_win.h, control_win.title, ATTR_PROMPT, ATTR_STATUS, ATTR_NORMAL);

    int inner_x = control_win.x + 1;
    int inner_y = control_win.y + 1;
    int inner_w = control_win.w - 2;
    int inner_h = control_win.h - 2;

    /* clear inner area */
    for (int yy = 0; yy < inner_h; ++yy)
        for (int xx = 0; xx < inner_w; ++xx)
            vga_putcell(inner_x + xx, inner_y + yy, ' ', ATTR_STATUS);

    /* Draw buttons with proper click detection zones */
    for (int i = 0; i < 3; i++) {
        ui_button_t* btn = &power_buttons[i];
        
        /* Determine button color */
        unsigned char attr = (btn->ticks > 0) ? btn->pressed_attr : btn->normal_attr;
        
        /* Draw button background */
        for (int x = 0; x < btn->w; x++) {
            vga_putcell(btn->x + x, btn->y, ' ', attr);
        }
        
        /* Draw button text centered */
        int text_len = kstrlen(btn->text);
        int text_start = btn->x + (btn->w - text_len) / 2;
        for (int c = 0; c < text_len; c++) {
            vga_putcell(text_start + c, btn->y, btn->text[c], attr);
        }
        
        /* Draw button border for visual clarity */
        if (btn->x > control_win.x + 1) {
            vga_putcell(btn->x - 1, btn->y, '[', ATTR_BORDER);
        }
        if (btn->x + btn->w < control_win.x + control_win.w - 1) {
            vga_putcell(btn->x + btn->w, btn->y, ']', ATTR_BORDER);
        }
        
        /* Countdown pressed state */
        if (btn->ticks > 0) btn->ticks--;
    }
}

/* -------- Main draw function (dirs + files + controls) -------- */
void ui_draw(void) {
    vga_clear();
    /* Title */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', ATTR_TITLE);
    const char* title = "NoirOS";
    for (int i = 0; title[i] && i < WIDTH - 2; i++) vga_putcell(1 + i, 0, title[i], ATTR_TITLE);

    u8 exp_border = (active_panel == 0) ? ATTR_TITLE : ATTR_BORDER;
    u8 view_border = (active_panel == 1) ? ATTR_TITLE : ATTR_BORDER;
    draw_box(explorer_win.x, explorer_win.y, explorer_win.w, explorer_win.h, explorer_win.title, ATTR_PROMPT, exp_border, ATTR_NORMAL);
    draw_box(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, viewer_win.title, ATTR_PROMPT, view_border, ATTR_NORMAL);
    draw_controls_window();
    draw_box(status_win.x, status_win.y, status_win.w, status_win.h, status_win.title, ATTR_VIEWER_TITLE, ATTR_STATUS, ATTR_NORMAL);

    int dir_count = fs_dir_count();
    int file_count = fs_count();
    int total = dir_count + file_count;
    if (total == 0) explorer_sel = 0;
    else if (explorer_sel >= total) explorer_sel = total - 1;
    draw_explorer_listing(dir_count, file_count, total);
    draw_viewer_content(dir_count);

    /* Status */
    const char* fname = "none";
    static char fnamebuf[128];
    if (total == 0) {
        fname = "none";
    } else if (explorer_sel < dir_count) {
        struct Dir* d = fs_dir_get(explorer_sel);
        int fp = 0;
        append_str(fnamebuf, &fp, d->name, sizeof(fnamebuf));
        append_char(fnamebuf, &fp, '/', sizeof(fnamebuf));
        fname = fnamebuf;
    } else {
        struct File* f = fs_get(explorer_sel - dir_count);
        int fp = 0;
        append_str(fnamebuf, &fp, f->name, sizeof(fnamebuf));
        fname = fnamebuf;
    }

    /* Status line 1: current path */
    char cwd_buf[64];
    fs_pwd(cwd_buf, sizeof(cwd_buf));
    {
        const char* label = "Path: ";
        int p = 0;
        for (int i = 0; label[i] && p < WIDTH - 2; i++)
            vga_putcell(status_win.x + 1 + p, status_win.y + 1, label[i], ATTR_STATUS), p++;
        for (int i = 0; cwd_buf[i] && p < WIDTH - 2; i++)
            vga_putcell(status_win.x + 1 + p, status_win.y + 1, cwd_buf[i], ATTR_PROMPT), p++;
        for (; p < WIDTH - 2; p++) vga_putcell(status_win.x + 1 + p, status_win.y + 1, ' ', ATTR_STATUS);
    }

    /* Status line 2: selected item name and quick command hint */
    {
        const char* sel_label = "Sel:  ";
        const char* hint = " | Cmd: c/C";
        int p = 0;
        for (int i = 0; sel_label[i] && p < WIDTH - 2; i++)
            vga_putcell(status_win.x + 1 + p, status_win.y + 2, sel_label[i], ATTR_STATUS), p++;
        for (int i = 0; fname[i] && p < WIDTH - 2; i++)
            vga_putcell(status_win.x + 1 + p, status_win.y + 2, fname[i], ATTR_NORMAL), p++;
        for (int i = 0; hint[i] && p < WIDTH - 2; i++)
            vga_putcell(status_win.x + 1 + p, status_win.y + 2, hint[i], ATTR_PROMPT), p++;
        for (; p < WIDTH - 2; p++) vga_putcell(status_win.x + 1 + p, status_win.y + 2, ' ', ATTR_STATUS);
    }
}

void ui_clear(void) {
    vga_clear();
}

/* -------- Colorful Power Screens -------- */

/* ---- Shared helper: draw a centered string on a given screen row ---- */
/* All dialog coordinates are derived from WIDTH/HEIGHT so they remain correct
 * if the screen dimensions ever change.  Bare literals like 40, 25, 20, 60
 * have been replaced throughout. */

/* Center a string horizontally and draw it at row `row` with attribute `attr`. */
static void draw_centered(int row, const char* s, unsigned char attr) {
    int len     = kstrlen(s);
    int start_x = (WIDTH - len) / 2;
    if (start_x < 0) start_x = 0;
    /* Clear the row segment first to erase any previous text of different length */
    for (int x = start_x; x < start_x + len && x < WIDTH; x++)
        vga_putcell(x, row, ' ', attr);
    for (int i = 0; i < len && start_x + i < WIDTH; i++)
        vga_putcell(start_x + i, row, s[i], attr);
}
    /* ---- explorer/viewer implementations ---- */
    static void draw_explorer_listing(int dir_count, int file_count, int total) {
        (void)file_count; /* count available from total - dir_count; kept for API symmetry */
        int e_lines = explorer_win.h - 2;
        for (int row = 0; row < e_lines; ++row) {
            int idx = explorer_scroll + row;
            if (idx >= total) break;
            u8 attr = (idx == explorer_sel) ? ATTR_SELECTED : ATTR_NORMAL;
            char display[40];
            int p = 0;
            if (idx < dir_count) {
                struct Dir* d = fs_dir_get(idx);
                append_char(display, &p, 'd', sizeof(display));
                append_char(display, &p, ' ', sizeof(display));
                append_str(display, &p, d->name, sizeof(display));
                append_char(display, &p, '/', sizeof(display));
            } else {
                struct File* f = fs_get(idx - dir_count);
                char tc = (f->type == FILE_EXE) ? '*' : (f->type == FILE_GAME) ? '>' : (f->readonly ? ' ' : '+');
                append_char(display, &p, tc, sizeof(display));
                append_char(display, &p, ' ', sizeof(display));
                append_str(display, &p, f->name, sizeof(display));
            }
            draw_text_in_win(explorer_win.x, explorer_win.y, explorer_win.w, explorer_win.h, 0, row, display, attr);
        }
    }

    static void draw_viewer_content(int dir_count) {
        int file_count = fs_count();
        int total = dir_count + file_count;
        if (total == 0) {
            draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, 0, "(empty)", ATTR_NORMAL);
            return;
        }
        if (explorer_sel < dir_count) {
            struct Dir* d = fs_dir_get(explorer_sel);
            char linebuf[200];
            int line = 0;
            int hb = 0;
            append_str(linebuf, &hb, "Directory: ", sizeof(linebuf));
            append_str(linebuf, &hb, d->name, sizeof(linebuf));
            append_char(linebuf, &hb, '/', sizeof(linebuf));
            append_str(linebuf, &hb, "  (", sizeof(linebuf));
            char tmp[32];
            int_to_dec(tmp, d->file_count);
            append_str(linebuf, &hb, tmp, sizeof(linebuf));
            append_str(linebuf, &hb, " files, ", sizeof(linebuf));
            int_to_dec(tmp, d->subdir_count);
            append_str(linebuf, &hb, tmp, sizeof(linebuf));
            append_str(linebuf, &hb, " subdirs)", sizeof(linebuf));
            draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, linebuf, ATTR_NORMAL);
            draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, "Use 'cd <name>' or press Enter to open", ATTR_NORMAL);
            for (int fi = 0; fi < d->subdir_count && line < viewer_win.h - 2; ++fi) {
                char buf[128]; int bp = 0;
                append_char(buf, &bp, 'd', sizeof(buf));
                append_char(buf, &bp, ' ', sizeof(buf));
                append_str(buf, &bp, d->subdirs[fi]->name, sizeof(buf));
                append_char(buf, &bp, '/', sizeof(buf));
                draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, buf, ATTR_DIR);
            }
            for (int fi = 0; fi < d->file_count && line < viewer_win.h - 2; ++fi) {
                char buf[128]; int bp = 0;
                struct File* ff = &d->files[fi];
                char tc = (ff->type == FILE_EXE) ? '*' : (ff->type == FILE_GAME) ? '>' : (ff->readonly ? ' ' : '+');
                append_char(buf, &bp, tc, sizeof(buf));
                append_char(buf, &bp, ' ', sizeof(buf));
                append_str(buf, &bp, ff->name, sizeof(buf));
                draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, buf, ATTR_FILE_TEXT);
            }
        } else {
            struct File* f = fs_get(explorer_sel - dir_count);
            /* Delegate rendering to viewer module */
            viewer_draw(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, f, viewer_scroll);
        }
    }

    /* Draw a count-down ticker row and wait ~1 s, scanning ESC each ~100 ms.
 * Returns 1 if ESC was pressed, 0 otherwise.
 * `row` is the screen row for the countdown message.
 * `attr_bg`/`attr_fg` are the fill and text color attributes. */
static int countdown_second(int row, unsigned char attr_bg, unsigned char attr_fg,
                             const char* prefix, int digit) {
    char msg[64];
    int pos = 0;
    for (int i = 0; prefix[i] && pos < 60; i++) msg[pos++] = prefix[i];
    msg[pos++] = '0' + digit;
    msg[pos++] = '.'; msg[pos++] = '.'; msg[pos++] = '.';
    msg[pos]   = '\0';

    /* Clear and draw countdown row */
    int start_x = (WIDTH - pos) / 2;
    for (int x = 1; x < WIDTH - 1; x++) vga_putcell(x, row, ' ', attr_bg);
    for (int i = 0; i < pos && start_x + i < WIDTH; i++)
        vga_putcell(start_x + i, row, msg[i], attr_fg);

    /* Poll ESC ~10 times with ~100 ms delay each */
    for (int i = 0; i < 10; i++) {
        for (volatile int j = 0; j < 1000000; j++);
        /* kb_read_scancode has a timeout so it won't spin forever */
        u8 sc = kb_read_scancode();
        if (sc == 0x01) return 1;   /* ESC scancode */
    }
    return 0;
}

/* -------- Restart screen -------- */
void show_restart_screen(void) {
    vga_clear();

    /* Gradient background */
    for (int y = 0; y < HEIGHT; y++) {
        unsigned char attr = (unsigned char)(VGA_ATTR(COL_BLACK, COL_GREEN) + (y / 3));
        if (attr > VGA_ATTR(COL_WHITE, COL_GREEN)) attr = VGA_ATTR(COL_WHITE, COL_GREEN);
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', attr);
    }

    /* Dialog box centered on screen */
    int bx = (WIDTH  - 30) / 2;
    int by = (HEIGHT - 10) / 2;
    draw_box(bx, by, 30, 10, " RESTARTING ", VGA_ATTR(COL_WHITE, COL_GREEN), VGA_ATTR(COL_YELLOW, COL_GREEN), VGA_ATTR(COL_BLACK, COL_GREEN));

    const char* lines[] = {
        "Restarting NoirOS...", "",
        "Saving state",
        "Stopping processes",
        "Unmounting drives", "",
        "Restarting in 3 seconds",
        "Press ESC to cancel"
    };
    int num_lines = 8;
    int text_start_row = by + 1;
    for (int i = 0; i < num_lines && text_start_row + i < HEIGHT; i++)
        draw_centered(text_start_row + i, lines[i], VGA_ATTR(COL_WHITE, COL_GREEN));

    int ticker_row = by + num_lines + 1;
    if (ticker_row >= HEIGHT) ticker_row = HEIGHT - 2;

    for (int cd = 3; cd > 0; cd--)
        if (countdown_second(ticker_row, VGA_ATTR(COL_YELLOW, COL_GREEN), VGA_ATTR(COL_WHITE, COL_GREEN), "Restarting in ", cd))
            { ui_draw(); return; }

    /* Commit — jump back to kernel entry point */
    vga_clear();
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', VGA_ATTR(COL_WHITE, COL_BLACK));
    int bx2 = (WIDTH - 20) / 2, by2 = (HEIGHT - 5) / 2;
    draw_box(bx2, by2, 20, 5, " RESTARTING ", VGA_ATTR(COL_WHITE, COL_GREEN), VGA_ATTR(COL_YELLOW, COL_GREEN), VGA_ATTR(COL_BLACK, COL_GREEN));
    draw_centered(by2 + 2, "Restarting kernel...", VGA_ATTR(COL_WHITE, COL_GREEN));
    for (volatile int i = 0; i < 10000000; i++);

    /* Jump to kernel load address — matches linker.ld ENTRY address */
    /* Real reboot via 8042 keyboard controller */
    for (int i = 0; i < 100; i++) {
        unsigned char r;
        __asm__ volatile ("inb $0x64, %0" : "=a"(r));
        if ((r & 0x02) == 0) break;
    }
    __asm__ volatile ("outb %0, $0x64" : : "a"((unsigned char)0xFE));
    while (1) { __asm__ volatile ("cli; hlt"); }
}

/* -------- Shutdown screen -------- */
void show_shutdown_screen(void) {
    vga_clear();

    for (int y = 0; y < HEIGHT; y++) {
        unsigned char attr = (unsigned char)(VGA_ATTR(COL_BLACK, COL_RED) + (y / 3));
        if (attr > VGA_ATTR(COL_WHITE, COL_RED)) attr = VGA_ATTR(COL_WHITE, COL_RED);
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', attr);
    }

    int bx = (WIDTH  - 30) / 2;
    int by = (HEIGHT - 12) / 2;
    draw_box(bx, by, 30, 12, " SHUTTING DOWN ", VGA_ATTR(COL_WHITE, COL_RED), VGA_ATTR(COL_LIGHT_RED, COL_RED), VGA_ATTR(COL_BLACK, COL_RED));

    const char* lines[] = {
        "Shutting down NoirOS...", "",
        "Saving files",
        "Stopping services",
        "Closing apps",
        "Unmounting drives", "",
        "Shutdown in 3 seconds",
        "Press ESC to cancel"
    };
    int num_lines = 9;
    int text_start_row = by + 1;
    for (int i = 0; i < num_lines && text_start_row + i < HEIGHT; i++)
        draw_centered(text_start_row + i, lines[i], VGA_ATTR(COL_WHITE, COL_RED));

    int ticker_row = by + num_lines + 1;
    if (ticker_row >= HEIGHT) ticker_row = HEIGHT - 2;

    for (int cd = 3; cd > 0; cd--)
        if (countdown_second(ticker_row, VGA_ATTR(COL_LIGHT_RED, COL_RED), VGA_ATTR(COL_WHITE, COL_RED), "Bye in ", cd))
            { ui_draw(); return; }

    /* Progress through shutdown stages */
    const char* stages[] = {
        "Killing processes...",
        "Saving state...",
        "Unmounting drives...",
        "Powering down...",
        "See ya!"
    };
    int stage_row  = by + 3;
    int bar_row    = by + 5;
    int bar_x      = (WIDTH - 20) / 2;
    int bar_w      = 20;

    for (int s = 0; s < 5; s++) {
        /* Clear and redraw stage text */
        for (int x = 1; x < WIDTH - 1; x++) vga_putcell(x, stage_row, ' ', VGA_ATTR(COL_YELLOW, COL_RED));
        draw_centered(stage_row, stages[s], VGA_ATTR(COL_WHITE, COL_RED));

        /* Progress bar */
        int progress = (s + 1) * bar_w / 5;
        for (int x = 0; x < bar_w; x++) {
            char ch   = (x < progress) ? '#' : '-';
            unsigned char at = (x < progress) ? VGA_ATTR(COL_LIGHT_RED, COL_RED) : VGA_ATTR(COL_LIGHT_GREY, COL_RED);
            vga_putcell(bar_x + x, bar_row, ch, at);
        }
        for (volatile int i = 0; i < 5000000; i++);
    }

    /* Final halt screen */
    vga_clear();
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', VGA_ATTR(COL_BLACK, COL_BLACK));
    int bx2 = (WIDTH - 30) / 2, by2 = (HEIGHT - 5) / 2;
    draw_box(bx2, by2, 30, 5, " GOODBYE! ", VGA_ATTR(COL_WHITE, COL_LIGHT_GREY), ATTR_NORMAL, VGA_ATTR(COL_BLACK, COL_BLACK));
    draw_centered(by2 + 2, "Safe to power off now", VGA_ATTR(COL_WHITE, COL_LIGHT_GREY));
    for (volatile int i = 0; i < 20000000; i++);

    /* QEMU / Bochs / VirtualBox shutdown via ACPI/APM ports */
    __asm__ volatile ("outw %%ax, %%dx" : : "a"((unsigned short)0x2000), "d"((unsigned short)0x604));
    __asm__ volatile ("outw %%ax, %%dx" : : "a"((unsigned short)0x2000), "d"((unsigned short)0xB004));

    /* Halt: disable interrupts and spin — correct x86 shutdown for bare metal/QEMU */
    while (1) { __asm__ volatile ("cli; hlt"); }
}

/* -------- Sleep screen -------- */
void show_sleep_screen(void) {
    vga_clear();

    for (int y = 0; y < HEIGHT; y++) {
        unsigned char attr = 0x10 + (y / 5);
        if (attr > 0x1F) attr = 0x1F;
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', attr);
    }

    int bx = (WIDTH  - 30) / 2;
    int by = (HEIGHT - 10) / 2;
    draw_box(bx, by, 30, 10, " GOING TO SLEEP ", VGA_ATTR(COL_WHITE, COL_BLUE), VGA_ATTR(COL_YELLOW, COL_BLUE), VGA_ATTR(COL_BLACK, COL_BLUE));

    const char* lines[] = {
        "Time for a nap...", "",
        "Saving session",
        "Reducing power",
        "Entering sleep mode", "",
        "Sleeping in 2 seconds",
        "Press ESC to stay awake"
    };
    int num_lines = 8;
    int text_start_row = by + 1;
    for (int i = 0; i < num_lines && text_start_row + i < HEIGHT; i++)
        draw_centered(text_start_row + i, lines[i], 0x1F);

    int ticker_row = by + num_lines + 1;
    if (ticker_row >= HEIGHT) ticker_row = HEIGHT - 2;

    for (int cd = 2; cd > 0; cd--)
        if (countdown_second(ticker_row, 0x1E, 0x1F, "Sleeping in ", cd))
            { ui_draw(); return; }

    /* Fade out */
    for (int fade = 0; fade < 8; fade++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                unsigned char base = (unsigned char)(0x10 + (y / 5));
                if (base > 0x1F) base = 0x1F;
                unsigned char fg = base & 0x0F;
                fg = (fg > (unsigned char)fade) ? fg - (unsigned char)fade : 0;
                vga_putcell(x, y, ' ', (base & 0xF0) | fg);
            }
        }
        if (fade < 5) {
            int sbx = (WIDTH - 20) / 2, sby = HEIGHT / 2 - 1;
            draw_box(sbx, sby, 20, 3, " SLEEPY ", 0x1F - fade, 0x1E - fade, 0x10);
            draw_centered(sby + 1, "Zzz...", 0x1F - fade);
        }
        for (volatile int i = 0; i < 3000000; i++);
    }

    /* True sleep: blank screen then HLT — wake on any keypress */
    vga_clear();
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', 0x00);
    {
        int sbx = (WIDTH - 10) / 2, sby = HEIGHT / 2 - 1;
        draw_box(sbx, sby, 10, 3, "", ATTR_BORDER, ATTR_BORDER, VGA_ATTR(COL_BLACK, COL_BLACK));
        draw_centered(sby + 1, "Zzz", ATTR_BORDER);
    }

    /* HLT: CPU sleeps until the next interrupt (keyboard generates IRQ1).
     * We don't need to re-enable interrupts before HLT because GRUB leaves
     * them enabled; this is safe as long as the IDT is in place. */
    u8 wake_key = 0;
    while (!wake_key) {
        __asm__ volatile ("hlt");
        wake_key = kb_read_scancode();
    }

    /* Fade back in */
    for (int bright = 1; bright <= 5; bright++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                unsigned char attr = (unsigned char)(0x10 + (y / 5) + bright);
                if (attr > 0x1F) attr = 0x1F;
                vga_putcell(x, y, ' ', attr);
            }
        }
        int wbx = (WIDTH - 20) / 2, wby = (HEIGHT - 5) / 2;
        draw_box(wbx, wby, 20, 5, " WAKEY WAKEY ", 0x1F, 0x1E, 0x10);
        draw_centered(wby + 1, "Good morning!", 0x1F);
        draw_centered(wby + 2, "System waking up...", 0x1E);
        for (volatile int i = 0; i < 2000000; i++);
    }

    for (volatile int i = 0; i < 3000000; i++);
    ui_draw();
}
