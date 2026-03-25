#include "../include/common.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/util.h"
#include "../include/apps.h"
#include "../include/shell.h"
#include "../include/editor.h"
#include "../include/game_snake.h"

/* forward declarations of game/editor functions */
void snake_init(void);
void snake_update(void);
void snake_draw(void);
int snake_handle_key(int key);  /* returns 1 if ESC pressed */

enum { MODE_BROWSER = 0, MODE_EDITOR = 1, MODE_GAME = 2 };
static int current_mode = MODE_BROWSER;

/**
 * Read a command line from the status bar prompt and execute simple commands.
 * This updates UI selection or switches modes (editor/game) as needed.
 * @param explorer_sel current explorer selection index
 * @param mode pointer to current mode variable (may be modified)
 * @return updated explorer selection index
 */
static int handle_command_input(int explorer_sel, int *mode) {
    const int sy = 23;
    const int sx = 1;

    /* Clear command area */
    for (int x = sx; x < 78; ++x)
        vga_putcell(x, sy, ' ', 0x07);

    /* Show prompt */
    const char *prompt = "cmd> ";
    for (int i = 0; prompt[i]; ++i)
        vga_putcell(sx + i, sy, prompt[i], 0x0E);

    int cx = sx + 5;
    char input[64];
    int ipos = 0;

    /* Read line */
    while (1) {
        int ch = read_key();
        if (ch == '\n' || ch == '\r') { input[ipos] = 0; break; }
        if (ch == K_ESC) { ui_draw(); return explorer_sel; }
        if (ch == '\b') {
            if (ipos > 0) { ipos--; cx--; vga_putcell(cx, sy, ' ', 0x07); }
            continue;
        }
        if (ipos < 63 && ch >= 32 && ch <= 126) {
            input[ipos++] = (char)ch;
            vga_putcell(cx++, sy, (char)ch, 0x0F);
        }
    }

    if (input[0]) {
        /* mode change: mode <width> <height> */
        if (kstrncmp(input, "mode ", 5) == 0) {
            int w = 0, h = 0;
            /* simple parse: two integers separated by space */
            int i = 5;
            while (input[i] == ' ') i++;
            while (input[i] >= '0' && input[i] <= '9') { w = w * 10 + (input[i] - '0'); i++; }
            while (input[i] == ' ') i++;
            while (input[i] >= '0' && input[i] <= '9') { h = h * 10 + (input[i] - '0'); i++; }
            if (w > 0 && h > 0) {
                vga_set_mode(w, h);
                ui_relayout();
                ui_draw();
            }
            return explorer_sel;
        }
        if (kstrcmp(input, "help") == 0) {
            for (int i = 0; i < fs_count(); ++i) {
                if (kstrcmp(fs_get(i)->name, "help.txt") == 0) {
                    ui_set_selected(i);
                    explorer_sel = i;
                    break;
                }
            }
        } else if (kstrcmp(input, "snake") == 0) {
            *mode = MODE_GAME;
            snake_init();
            snake_draw();
        } else if (kstrcmp(input, "ls") == 0 || kstrcmp(input, "dir") == 0) {
            ui_draw();
        } else if (kstrncmp(input, "edit ", 5) == 0) {
            const char *fname = input + 5;
            int idx = -1;
            for (int i = 0; i < fs_count(); ++i) {
                if (kstrcmp(fs_get(i)->name, fname) == 0) { idx = i; break; }
            }
            if (idx >= 0) {
                *mode = MODE_EDITOR;
                editor_open(fs_get(idx)->name, &current_mode);
            }
        }
    }
    return explorer_sel;
}

/**
 * Kernel main loop: initialize subsystems and process events.
 * Handles navigation in browser mode, editor input and the snake game loop.
 */
void kernel_main(void) {
    init_filesystem();
    /* Ensure UI layout matches runtime VGA mode */
    ui_relayout();
    ui_draw();

    int explorer_sel = ui_get_selected();
    int game_timer = 0;
    const int game_speed = 15; /* frames between snake ticks */

    while (1) {
        int k = read_key();

        if (current_mode == MODE_BROWSER) {
            if (k == K_ARROW_UP || k == 'w' || k == 'W') {
                explorer_sel = (explorer_sel > 0) ? explorer_sel - 1 : 0;
                ui_set_selected(explorer_sel);
                ui_draw();
            } else if (k == K_ARROW_DOWN || k == 's' || k == 'S') {
                int total = fs_dir_count() + fs_count();
                int max = total - 1;
                if (max < 0) max = 0;
                if (explorer_sel < max) explorer_sel++;
                ui_set_selected(explorer_sel);
                ui_draw();
            } else if (k == K_F1) {
                show_restart_screen();
            } else if (k == K_F2) {
                show_shutdown_screen();
            } else if (k == K_F3) {
                show_sleep_screen();
            } else if (k == K_PAGE_UP) {
                ui_scroll_viewer(-3);
                ui_draw();
            } else if (k == K_PAGE_DOWN) {
                ui_scroll_viewer(3);
                ui_draw();
            } else if (k == '\n' || k == '\r') {
                explorer_sel = handle_command_input(explorer_sel, &current_mode);
            }
        } else if (current_mode == MODE_EDITOR) {
            editor_handle_key(k, &current_mode);
            if (current_mode != MODE_EDITOR) ui_draw();
        } else if (current_mode == MODE_GAME) {
            if (snake_handle_key(k)) {
                /* ESC pressed — return to browser */
                current_mode = MODE_BROWSER;
                game_timer   = 0;
                ui_draw();
            } else {
                game_timer++;
                if (game_timer >= game_speed) {
                    game_timer = 0;
                    snake_update();
                    snake_draw();
                }
            }
        }

        /* Small delay to keep input responsive */
        for (volatile int i = 0; i < 10000; ++i) {
            asm volatile("nop");
        }
    }
}
