#include "../include/common.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/util.h"
#include "../include/apps.h"
#include "../include/shell.h"
#include "../include/mode.h"
#include "../include/editor.h"
#include "../include/game_snake.h"

/* forward declarations of game/editor functions */
void snake_init(void);
void snake_update(void);
void snake_draw(void);
int snake_handle_key(int key);  /* returns 1 if ESC pressed */

static int current_mode = MODE_BROWSER;

/**
 * Read a command line from the status bar prompt and execute simple commands.
 * This updates UI selection or switches modes (editor/game) as needed.
 * @param explorer_sel current explorer selection index
 * @param mode pointer to current mode variable (may be modified)
 * @return updated explorer selection index
 */
/* Command input handling moved into shell module; kernel now delegates to shell_loop(). */

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

        /* Debug toggle: 'd' or 'D' shows raw scancode/key overlay */
        if (k == 'd' || k == 'D') {
            input_toggle_debug();
            ui_draw();
        }

        /* If debug overlay enabled, draw scancode/key at top-right */
        if (input_debug_enabled()) {
            int sc = input_get_last_scancode();
            int lk = input_get_last_key();
            /* simple display: two small fields */
            char buf[32]; int p = 0;
            int x = WIDTH - 24;
            for (int i = 0; i < 20; ++i) vga_putcell(x + i, 0, ' ', ATTR_TITLE);
            p = 0;
            /* "SC:NN" */
            char t1[16]; int tp = 0;
            int_to_dec(t1, sc);
            buf[p++] = 'S'; buf[p++] = 'C'; buf[p++] = ':'; 
            for (int i = 0; t1[i] && p < (int)sizeof(buf)-1; ++i) buf[p++] = t1[i];
            buf[p] = '\0';
            for (int i = 0; buf[i]; ++i) vga_putcell(x + i, 0, buf[i], ATTR_PROMPT);
            /* "K:NN" */
            char t2[16]; int_to_dec(t2, lk);
            int off = 8;
            vga_putcell(x + off - 1, 0, ' ', ATTR_PROMPT);
            vga_putcell(x + off + 0, 0, 'K', ATTR_PROMPT);
            vga_putcell(x + off + 1, 0, ':', ATTR_PROMPT);
            for (int i = 0; t2[i]; ++i) vga_putcell(x + off + 2 + i, 0, t2[i], ATTR_PROMPT);
        }

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
                /* Enter: activate selected item directly when possible.
                 * - If directory: change into it
                 * - If game file: start game
                 * - If Noir C file: run via interpreter
                 * Otherwise: fall back to shell prompt. */
                int sel = ui_get_selected();
                int dir_count = fs_dir_count();
                int file_count = fs_count();
                int total = dir_count + file_count;
                if (total > 0 && sel < total) {
                    if (sel < dir_count) {
                        /* directory */
                        struct Dir* d = fs_dir_get(sel);
                        if (d) {
                            if (fs_chdir(d->name) == FS_OK) {
                                ui_reset_explorer_scroll();
                                ui_set_selected(0);
                                ui_draw();
                            }
                        }
                    } else {
                        struct File* f = fs_get(sel - dir_count);
                        if (f) {
                            if (f->type == FILE_GAME) {
                                current_mode = MODE_GAME;
                                snake_init();
                                snake_draw();
                                /* game loop will take over */
                            } else if (f->type == FILE_NOIRC) {
                                /* run Noir C script */
                                noirc_run(f);
                            } else {
                                /* fallback: open shell prompt */
                                explorer_sel = shell_loop(explorer_sel, &current_mode, k);
                            }
                        }
                    }
                } else {
                    explorer_sel = shell_loop(explorer_sel, &current_mode, k);
                }
            } else if (k == 'c' || k == 'C') {
                /* Open command prompt (Cmd) without changing selection — 'c' key */
                explorer_sel = shell_open_prompt(explorer_sel, &current_mode);
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
