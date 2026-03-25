#include "../include/noirc.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/util.h"
#include "../include/ui.h"

/* Minimal Noir C interpreter stub: displays the source and a fake execution
 * output. This is intentionally lightweight — the full interpreter is a
 * larger task and can be implemented later. */
static void noirc_append_str(char *buf, int *p, const char *s, int max) {
    while (*s && *p < max - 1) buf[(*p)++] = *s++;
    buf[*p] = '\0';
}

void noirc_run(struct File* f) {
    if (!f) return;
    vga_clear();

    /* Header */
    char title[80]; int tp = 0;
    noirc_append_str(title, &tp, "Running: ", sizeof(title));
    noirc_append_str(title, &tp, f->name, sizeof(title));
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', VGA_ATTR(COL_WHITE, COL_BLACK));
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], VGA_ATTR(COL_WHITE, COL_BLACK));

    /* Show source lines in the main area */
    int line = 2;
    for (int i = 0; i < f->length && line < HEIGHT - 2; ++i) {
        char ch = f->content[i];
        if (ch == '\n') { line++; continue; }
        if (ch >= 32 && ch <= 126) vga_putcell(1 + ((i % (WIDTH-2))), line, ch, VGA_ATTR(COL_LIGHT_GREY, COL_BLACK));
    }

    /* Fake execution output area */
    const char* out = "[noirc] Execution finished (stub).";
    for (int i = 0; out[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, HEIGHT - 2, out[i], VGA_ATTR(COL_YELLOW, COL_BLACK));

    /* Wait for key and return to UI */
    read_key();
    ui_draw();
}
