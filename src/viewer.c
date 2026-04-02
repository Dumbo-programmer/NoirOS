#include "../include/viewer.h"
#include "../include/vga.h"
#include "../include/util.h"
#include "../include/fs.h"
#include "../include/common.h"

/* Very small viewer: renders markdown headings and strips HTML tags.
 * Does not allocate memory; writes directly into the provided window using
 * draw_text_in_win. `scroll` skips that many output lines.
 */

/* Emit one visual line subject to scroll. `lineno` is the sequential output
 * line index; if it is before `scroll` the line is skipped, otherwise it is
 * drawn at position (lineno - scroll) inside the window. */
static void emit_line(int x, int y, int w, int h, int scroll,
                      const char* s, u8 attr, int lineno) {
    if (lineno < scroll) return;
    draw_text_in_win(x, y, w, h, 0, lineno - scroll, s, attr);
}

void viewer_draw(int x, int y, int w, int h, struct File* f, int scroll) {
    if (!f) {
        draw_text_in_win(x, y, w, h, 0, 0, "(no file)", ATTR_NORMAL);
        return;
    }

    const char* p = f->content;
    char line[512];
    int lineno = 0;

    if (f->type == FILE_HTML) {
        /* Strip HTML tags and emit plain lines */
        int li = 0;
        int in_tag = 0;
        while (*p) {
            if (*p == '<') { in_tag = 1; p++; continue; }
            if (*p == '>') { in_tag = 0; p++; continue; }
            if (in_tag) { p++; continue; }
            if (*p == '\n' || li >= w - 3) {
                line[li] = '\0';
                emit_line(x, y, w, h, scroll, line, ATTR_FILE_HTML, lineno++);
                li = 0;
                if (*p == '\n') p++;
                continue;
            }
            line[li++] = *p++;
        }
        if (li > 0) { line[li] = '\0'; emit_line(x, y, w, h, scroll, line, ATTR_FILE_HTML, lineno++); }
        return;
    }

    if (f->type == FILE_MARKDOWN) {
        /* Very simple markdown: treat lines starting with # as headings */
        while (*p) {
            int li = 0;
            /* read one source line */
            int hashes = 0;
            while (*p == '#') { hashes++; p++; }
            /* skip one optional space */
            if (*p == ' ') p++;
            while (*p && *p != '\n' && li < w - 3) line[li++] = *p++;
            if (*p == '\n') p++;
            line[li] = '\0';
            if (hashes > 0) emit_line(x, y, w, h, scroll, line, ATTR_FILE_MD, lineno++);
            else emit_line(x, y, w, h, scroll, line, ATTR_FILE_TEXT, lineno++);
        }
        return;
    }

    if (f->type == FILE_NOIRC) {
        /* Render Noir C source with dedicated color */
        while (*p) {
            int li = 0;
            while (*p && *p != '\n' && li < w - 3) line[li++] = *p++;
            if (*p == '\n') p++;
            line[li] = '\0';
            emit_line(x, y, w, h, scroll, line, ATTR_FILE_NOIRC, lineno++);
        }
        return;
    }

    /* Default text / game / exe: render as plain text */
    while (*p) {
        int li = 0;
        while (*p && *p != '\n' && li < w - 3) line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        emit_line(x, y, w, h, scroll, line, ATTR_FILE_TEXT, lineno++);
    }
}
