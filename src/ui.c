#include "../include/ui.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/util.h"
#include "../include/input.h"

static Window explorer_win = {0, 1, 32, 20, " Explorer "};
/* UI window layout: explorer | viewer + controls | status bar.
 * All coordinates are in 80x25 VGA text-mode screen space. */
/* viewer is shrunk so the controls window fits below it */
static Window viewer_win   = {33, 1, 46, 14, " Viewer "};
static Window control_win  = {33, 15, 46, 6,  " Controls "};
static Window status_win   = {0, 22, WIDTH, 3,  " Status "};

static int explorer_sel = 0;
static int viewer_scroll = 0;

/* Button definitions for clickable controls */
typedef struct {
    int x, y, w, h;
    const char* text;
    unsigned char normal_attr;
    unsigned char pressed_attr;
    int ticks;
    void (*callback)(void);
} ui_button_t;

/* Power control buttons */
static ui_button_t power_buttons[3];
static int power_buttons_initialized = 0;

/* Button press duration */
#define UI_PRESS_TICKS 8

/* Forward declaration */
static void init_power_buttons(void);

/* -------- tiny helpers (no stdio) -------- */
static void append_str(char *buf, int *p, const char *s, int max) {
    while (*s && *p < max - 1) buf[(*p)++] = *s++;
    buf[*p] = '\0';
}
static void append_char(char *buf, int *p, char c, int max) {
    if (*p < max - 1) buf[(*p)++] = c;
    buf[*p] = '\0';
}
static int int_to_dec(char *out, int val) {
    if (val == 0) { out[0] = '0'; out[1] = '\0'; return 1; }
    char tmp[16];
    int tp = 0;
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    while (val > 0 && tp < (int)sizeof(tmp)) {
        tmp[tp++] = '0' + (val % 10);
        val /= 10;
    }
    int pos = 0;
    if (neg) out[pos++] = '-';
    for (int i = tp - 1; i >= 0; --i) out[pos++] = tmp[i];
    out[pos] = '\0';
    return pos;
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
        
        int clicked_line = y - (explorer_win.y + 1);
        int dir_count = fs_dir_count();
        int file_count = fs_count();
        int total = dir_count + file_count;
        
        if (clicked_line < total) {
            explorer_sel = clicked_line;
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

/* small helper: compute pressed attr from original attr by inverting bg to bright background.
   original_attr examples: 0x2F (bg=2 fg=F) -> pressed_attr = 0xF2 */
static unsigned char pressed_attr_from(unsigned char orig) {
    unsigned char bg = (orig >> 4) & 0x0F;
    unsigned char pressed = (0xF << 4) | (bg & 0x0F);
    return pressed;
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
    power_buttons[0].normal_attr = 0x2F;
    power_buttons[0].pressed_attr = 0xF2;
    power_buttons[0].ticks = 0;
    power_buttons[0].callback = show_restart_screen;
    
    /* Shutdown button */
    power_buttons[1].x = start_x + btn_w + spacing;
    power_buttons[1].y = btn_y;
    power_buttons[1].w = btn_w;
    power_buttons[1].h = 1;
    power_buttons[1].text = " Shut Down(F2) ";
    power_buttons[1].normal_attr = 0x4F;
    power_buttons[1].pressed_attr = 0xF4;
    power_buttons[1].ticks = 0;
    power_buttons[1].callback = show_shutdown_screen;
    
    /* Sleep button */
    power_buttons[2].x = start_x + 2 * (btn_w + spacing);
    power_buttons[2].y = btn_y;
    power_buttons[2].w = btn_w;
    power_buttons[2].h = 1;
    power_buttons[2].text = " Sleep(F3) ";
    power_buttons[2].normal_attr = 0x1F;
    power_buttons[2].pressed_attr = 0xF1;
    power_buttons[2].ticks = 0;
    power_buttons[2].callback = show_sleep_screen;
    
    power_buttons_initialized = 1;
}

static void draw_controls_window(void) {
    init_power_buttons();
    
    draw_box(control_win.x, control_win.y, control_win.w, control_win.h, control_win.title, 0x0E, 0x70, 0x07);

    int inner_x = control_win.x + 1;
    int inner_y = control_win.y + 1;
    int inner_w = control_win.w - 2;
    int inner_h = control_win.h - 2;

    /* clear inner area */
    for (int yy = 0; yy < inner_h; ++yy)
        for (int xx = 0; xx < inner_w; ++xx)
            vga_putcell(inner_x + xx, inner_y + yy, ' ', 0x70);

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
            vga_putcell(btn->x - 1, btn->y, '[', 0x08);
        }
        if (btn->x + btn->w < control_win.x + control_win.w - 1) {
            vga_putcell(btn->x + btn->w, btn->y, ']', 0x08);
        }
        
        /* Countdown pressed state */
        if (btn->ticks > 0) btn->ticks--;
    }
}

/* -------- Main draw function (dirs + files + controls) -------- */
void ui_draw(void) {
    vga_clear();
    /* Title */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
    const char* title = "NoirOS";
    for (int i = 0; title[i] && i < WIDTH - 2; i++) vga_putcell(1 + i, 0, title[i], 0x1F);

    draw_box(explorer_win.x, explorer_win.y, explorer_win.w, explorer_win.h, explorer_win.title, 0x0E, 0x70, 0x07);
    draw_box(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, viewer_win.title, 0x0E, 0x70, 0x07);
    draw_controls_window();
    draw_box(status_win.x, status_win.y, status_win.w, status_win.h, status_win.title, 0x9F, 0x70, 0x07);

    int dir_count = fs_dir_count();
    int file_count = fs_count();
    int total = dir_count + file_count;
    if (total == 0) explorer_sel = 0;
    else if (explorer_sel >= total) explorer_sel = total - 1;
    draw_explorer_listing(dir_count, file_count, total);
    draw_viewer_content(dir_count, explorer_sel, viewer_scroll);
void draw_explorer_listing(int dir_count, int file_count, int total) {
    int e_lines = explorer_win.h - 2;
    for (int i = 0; i < total && i < e_lines; ++i) {
        u8 attr = (i == explorer_sel) ? 0x1F : 0x07;
        char display[40];
        int p = 0;
        if (i < dir_count) {
            struct Dir* d = fs_dir_get(i);
            append_char(display, &p, 'd', sizeof(display));
            append_char(display, &p, ' ', sizeof(display));
            append_str(display, &p, d->name, sizeof(display));
            append_char(display, &p, '/', sizeof(display));
        } else {
            struct File* f = fs_get(i - dir_count);
            char tc = (f->type == 1) ? '*' : (f->type == 2) ? '>' : (f->readonly ? ' ' : '+');
            append_char(display, &p, tc, sizeof(display));
            append_char(display, &p, ' ', sizeof(display));
            append_str(display, &p, f->name, sizeof(display));
        }
        draw_text_in_win(explorer_win.x, explorer_win.y, explorer_win.w, explorer_win.h, 0, i, display, attr);
    }
}

void draw_viewer_content(int dir_count, int explorer_sel, int viewer_scroll) {
    int file_count = fs_count();
    int total = dir_count + file_count;
    if (total == 0) {
        draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, 0, "(empty)", 0x07);
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
        draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, linebuf, 0x07);
        draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, "Use 'cd <name>' or press Enter to open", 0x07);
        for (int fi = 0; fi < d->subdir_count && line < viewer_win.h - 2; ++fi) {
            char buf[128]; int bp = 0;
            append_char(buf, &bp, 'd', sizeof(buf));
            append_char(buf, &bp, ' ', sizeof(buf));
            append_str(buf, &bp, d->subdirs[fi]->name, sizeof(buf));
            append_char(buf, &bp, '/', sizeof(buf));
            draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, buf, 0x07);
        }
        for (int fi = 0; fi < d->file_count && line < viewer_win.h - 2; ++fi) {
            char buf[128]; int bp = 0;
            struct File* ff = &d->files[fi];
            char tc = (ff->type == 1) ? '*' : (ff->type == 2) ? '>' : (ff->readonly ? ' ' : '+');
            append_char(buf, &bp, tc, sizeof(buf));
            append_char(buf, &bp, ' ', sizeof(buf));
            append_str(buf, &bp, ff->name, sizeof(buf));
            draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line++, buf, 0x07);
        }
    } else {
        struct File* f = fs_get(explorer_sel - dir_count);
        const char* p = f->content;
        char linebuf[200];
        int max_lines = viewer_win.h - 2;
        int skip = viewer_scroll;
        int line_no = 0;
        while (*p && line_no < skip + max_lines) {
            int lb = 0;
            while (*p && *p != '\n' && lb < (viewer_win.w - 3)) linebuf[lb++] = *p++;
            if (*p == '\n') p++;
            linebuf[lb] = '\0';
            if (line_no >= skip) draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, line_no - skip, linebuf, 0x07);
            line_no++;
        }
    }
}

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

    const char* user_info = "User: root | File: ";
    int pos = 0;
    for (int i = 0; user_info[i] && pos < 60; ++i) vga_putcell(status_win.x + 1 + pos, status_win.y + 1, user_info[i], 0x07), pos++;
    for (int i = 0; fname[i] && pos < 70; ++i) vga_putcell(status_win.x + 1 + pos, status_win.y + 1, fname[i], 0x0F), pos++;
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
        unsigned char attr = 0x20 + (y / 3);
        if (attr > 0x2F) attr = 0x2F;
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', attr);
    }

    /* Dialog box centered on screen */
    int bx = (WIDTH  - 30) / 2;
    int by = (HEIGHT - 10) / 2;
    draw_box(bx, by, 30, 10, " RESTARTING ", 0x2F, 0x2E, 0x20);

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
        draw_centered(text_start_row + i, lines[i], 0x2F);

    int ticker_row = by + num_lines + 1;
    if (ticker_row >= HEIGHT) ticker_row = HEIGHT - 2;

    for (int cd = 3; cd > 0; cd--)
        if (countdown_second(ticker_row, 0x2E, 0x2F, "Restarting in ", cd))
            { ui_draw(); return; }

    /* Commit — jump back to kernel entry point */
    vga_clear();
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', 0x0F);
    int bx2 = (WIDTH - 20) / 2, by2 = (HEIGHT - 5) / 2;
    draw_box(bx2, by2, 20, 5, " RESTARTING ", 0x4F, 0x4E, 0x40);
    draw_centered(by2 + 2, "Restarting kernel...", 0x4F);
    for (volatile int i = 0; i < 10000000; i++);

    /* Jump to kernel load address — matches linker.ld ENTRY address */
    void (*restart_kernel)(void) = (void (*)(void))0x100000;
    restart_kernel();
}

/* -------- Shutdown screen -------- */
void show_shutdown_screen(void) {
    vga_clear();

    for (int y = 0; y < HEIGHT; y++) {
        unsigned char attr = 0x40 + (y / 3);
        if (attr > 0x4F) attr = 0x4F;
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', attr);
    }

    int bx = (WIDTH  - 30) / 2;
    int by = (HEIGHT - 12) / 2;
    draw_box(bx, by, 30, 12, " SHUTTING DOWN ", 0x4F, 0x4E, 0x40);

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
        draw_centered(text_start_row + i, lines[i], 0x4F);

    int ticker_row = by + num_lines + 1;
    if (ticker_row >= HEIGHT) ticker_row = HEIGHT - 2;

    for (int cd = 3; cd > 0; cd--)
        if (countdown_second(ticker_row, 0x4E, 0x4F, "Bye in ", cd))
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
        for (int x = 1; x < WIDTH - 1; x++) vga_putcell(x, stage_row, ' ', 0x4E);
        draw_centered(stage_row, stages[s], 0x4F);

        /* Progress bar */
        int progress = (s + 1) * bar_w / 5;
        for (int x = 0; x < bar_w; x++) {
            char ch   = (x < progress) ? '#' : '-';
            unsigned char at = (x < progress) ? 0x4C : 0x47;
            vga_putcell(bar_x + x, bar_row, ch, at);
        }
        for (volatile int i = 0; i < 5000000; i++);
    }

    /* Final halt screen */
    vga_clear();
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) vga_putcell(x, y, ' ', 0x00);
    int bx2 = (WIDTH - 30) / 2, by2 = (HEIGHT - 5) / 2;
    draw_box(bx2, by2, 30, 5, " GOODBYE! ", 0x70, 0x07, 0x00);
    draw_centered(by2 + 2, "Safe to power off now", 0x70);
    for (volatile int i = 0; i < 20000000; i++);

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
    draw_box(bx, by, 30, 10, " GOING TO SLEEP ", 0x1F, 0x1E, 0x10);

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
        draw_box(sbx, sby, 10, 3, "", 0x08, 0x08, 0x00);
        draw_centered(sby + 1, "Zzz", 0x08);
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
