#include "../include/ui.h"
#include "../include/vga.h"
#include "../include/fs.h"
#include "../include/util.h"

static Window explorer_win = {0, 1, 32, 20, " Explorer "};
/* viewer shrunk so controls window fits under it */
static Window viewer_win   = {33, 1, 46, 14, " Viewer "};
static Window control_win  = {33, 15, 46, 6,  " Controls "};
static Window status_win   = {0, 22, 80, 3,  " Status "};

static int explorer_sel = 0;
static int viewer_scroll = 0;

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

/* -------- Controls button pressed-state + callbacks -------- */

/* duration (in UI frames) to display the pressed state */
#define UI_PRESS_TICKS 8

static int restart_ticks = 0;
static int shutdown_ticks = 0;
static int sleep_ticks = 0;

/* user-assignable callbacks (register with setters below) */
static void (*cb_restart)(void) = 0;
static void (*cb_shutdown)(void) = 0;
static void (*cb_sleep)(void) = 0;

void ui_set_restart_callback(void (*cb)(void))  { cb_restart  = cb; }
void ui_set_shutdown_callback(void (*cb)(void)) { cb_shutdown = cb; }
void ui_set_sleep_callback(void (*cb)(void))    { cb_sleep    = cb; }


/* Called from shell_loop() immediately after read_key() so UI can react to Fn keys */
void ui_handle_key(int key) {
    /* you must have K_F1/K_F2/K_F3 defined in your input.h - used elsewhere already */
#ifdef K_F1
    if (key == K_F1) {
        restart_ticks = UI_PRESS_TICKS;
        if (cb_restart) cb_restart();
        return;
    }
#endif
#ifdef K_F2
    if (key == K_F2) {
        shutdown_ticks = UI_PRESS_TICKS;
        if (cb_shutdown) cb_shutdown();
        return;
    }
#endif
#ifdef K_F3
    if (key == K_F3) {
        sleep_ticks = UI_PRESS_TICKS;
        if (cb_sleep) cb_sleep();
        return;
    }
#endif
}

/* small helper: compute pressed attr from original attr by inverting bg to bright background.
   original_attr examples: 0x2F (bg=2 fg=F) -> pressed_attr = 0xF2 */
static unsigned char pressed_attr_from(unsigned char orig) {
    unsigned char bg = (orig >> 4) & 0x0F;
    unsigned char pressed = (0xF << 4) | (bg & 0x0F);
    return pressed;
}

/* -------- Draw Controls window (buttons with colored backgrounds) -------- */
static void draw_controls_window(void) {
    draw_box(control_win.x, control_win.y, control_win.w, control_win.h, control_win.title, 0x0E, 0x70, 0x07);

    int inner_x = control_win.x + 1;
    int inner_y = control_win.y + 1;
    int inner_w = control_win.w - 2;
    int inner_h = control_win.h - 2;

    /* clear inner area */
    for (int yy = 0; yy < inner_h; ++yy)
        for (int xx = 0; xx < inner_w; ++xx)
            vga_putcell(inner_x + xx, inner_y + yy, ' ', 0x70);

    /* labels with the exact desired text */
    const char *labels[] = { " Restart(F1) ", " Shut Down(F2) ", " Sleep(F3) " };
    const int nbtn = 3;
    int btn_w = 14;
    int spacing = 2;
    int total_btns_w = nbtn * btn_w + (nbtn - 1) * spacing;
    int start_x = control_win.x + 1 + ((inner_w - total_btns_w) / 2);
    int btn_y = inner_y + (inner_h / 2); /* one-line buttons */

    for (int i = 0; i < nbtn; ++i) {
        int bx = start_x + i * (btn_w + spacing);
        int by = btn_y;
        unsigned char attr;
        if (i == 0) attr = 0x2F; /* green bg, bright fg */
        else if (i == 1) attr = 0x4F; /* red-ish bg */
        else attr = 0x6F; /* yellow-ish bg */

        /* choose pressed attr if its ticks > 0 */
        unsigned char use_attr = attr;
        if ((i == 0 && restart_ticks > 0) || (i == 1 && shutdown_ticks > 0) || (i == 2 && sleep_ticks > 0)) {
            use_attr = pressed_attr_from(attr);
        }

        /* draw button background */
        for (int x = 0; x < btn_w; ++x) vga_putcell(bx + x, by, ' ', use_attr);

        /* draw label centered */
        const char *lab = labels[i];
        int lablen = 0; while (lab[lablen]) lablen++;
        int lab_start = bx + (btn_w - lablen) / 2;
        for (int c = 0; c < lablen; ++c) vga_putcell(lab_start + c, by, lab[c], use_attr);

        /* small border around button */
        vga_putcell(bx - 1, by, '[', 0x07);
        vga_putcell(bx + btn_w, by, ']', 0x07);
    }
}

/* -------- Main draw function (dirs + files + controls) -------- */
void ui_draw(void) {
    /* tick down pressed states (so they are transient) */
    if (restart_ticks > 0) restart_ticks--;
    if (shutdown_ticks > 0) shutdown_ticks--;
    if (sleep_ticks > 0) sleep_ticks--;

    vga_clear();
    /* Title */
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
    const char* title = "NoirOS";
    for (int i = 0; title[i] && i < WIDTH - 2; i++) vga_putcell(1 + i, 0, title[i], 0x1F);

    draw_box(explorer_win.x, explorer_win.y, explorer_win.w, explorer_win.h, explorer_win.title, 0x0E, 0x70, 0x07);
    draw_box(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, viewer_win.title, 0x0E, 0x70, 0x07);
    draw_controls_window();
    draw_box(status_win.x, status_win.y, status_win.w, status_win.h, status_win.title, 0x9F, 0x70, 0x07);

    /* Explorer listing (dirs first, then files) */
    int dir_count = fs_dir_count();
    int file_count = fs_count();
    int total = dir_count + file_count;
    if (total == 0) explorer_sel = 0;
    else if (explorer_sel >= total) explorer_sel = total - 1;

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

    /* Viewer content (shrunk) */
    if (total == 0) {
        draw_text_in_win(viewer_win.x, viewer_win.y, viewer_win.w, viewer_win.h, 0, 0, "(empty)", 0x07);
    } else if (explorer_sel < dir_count) {
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
