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
#include "../include/noirc.h"
#include "../include/serial.h"
#include "../include/idt.h"
#include "../include/memory.h"
#include "../include/mouse.h"

/* forward declarations of game/editor functions */
void snake_init(void);
void snake_update(void);
void snake_draw(void);
int snake_handle_key(int key);  /* returns 1 if ESC pressed, 2 if restarted, 0 otherwise */

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
    /* Initialize debug serial for logging (captured by QEMU -serial stdio) */
    serial_init();
    memory_init();
    idt_init();
    input_init();
    if (init_mouse()) {
        input_enable_mouse_irq();
    }
    init_filesystem();
    /* Ensure UI layout matches runtime VGA mode */
    ui_relayout();
    ui_show_boot_loader();
    ui_draw();

    int explorer_sel = ui_get_selected();
    int game_timer = 0;
    const int game_speed = 180; /* frames between snake ticks (higher is slower) */

    while (1) {
        int k = read_key();
        /* Global hotkeys - restricted to browser to prevent disrupting apps */
        if (k == K_TAB && current_mode == MODE_BROWSER) {
            ui_toggle_active_panel();
            ui_draw();
            continue;
        }

        if ((k == 'c' || k == 'C') && current_mode == MODE_BROWSER) {
            /* Open command prompt (Cmd) from browser mode */
            explorer_sel = shell_open_prompt(explorer_sel, &current_mode);
            input_reset_modifiers();
            if (current_mode == MODE_BROWSER) {
                ui_draw();
            }
            continue;
        }

        if (k == K_ESC) {
            /* ESC behaviour:
             * - If in a non-browser mode, return to browser.
             * - If already in browser and explorer panel active, go up one directory.
             */
            if (current_mode != MODE_BROWSER) {
                current_mode = MODE_BROWSER;
                input_reset_modifiers();
                ui_draw();
                continue;
            }

            /* In browser mode: if explorer has focus, attempt to go up a directory */
            if (ui_get_active_panel() == 0) {
                /* Go to parent directory (fs_chdir("..")) and update UI */
                fs_chdir("..");
                input_reset_modifiers();
                ui_reset_explorer_scroll();
                ui_set_selected(0);
                ui_draw();
                continue;
            }
            /* Otherwise, just ensure we're in browser mode */
            ui_draw();
            continue;
        }

        /* Debug toggle: 'd' or 'D' shows raw scancode/key overlay */
        if ((k == 'd' || k == 'D') && current_mode == MODE_BROWSER) {
            input_toggle_debug();
            ui_draw();
        }

        /* If debug overlay enabled, draw scancode/key at top-right */
        if (input_debug_enabled()) {
            int sc = input_get_last_scancode();
            int lk = input_get_last_key();
            /* Also emit to serial for headless capture */
            char sline[64]; int sp = 0;
            sline[sp++] = 'S'; sline[sp++] = 'C'; sline[sp++] = ':'; 
            int_to_dec(&sline[sp], sc);
            while (sline[sp]) sp++;
            sline[sp++] = ' '; sline[sp++] = 'K'; sline[sp++] = ':';
            int_to_dec(&sline[sp], lk);
            while (sline[sp]) sp++;
            sline[sp++] = '\n'; sline[sp] = '\0';
            serial_puts(sline);
            /* simple display: two small fields */
            char buf[32]; int p = 0;
            int x = WIDTH - 24;
            for (int i = 0; i < 20; ++i) vga_putcell(x + i, 0, ' ', ATTR_TITLE);
            p = 0;
            /* "SC:NN" */
            char t1[16];
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
            /* Delegate browser/key handling to shell_loop which consumes the key `k`. */
            explorer_sel = shell_loop(explorer_sel, &current_mode, k);
        } else if (current_mode == MODE_EDITOR) {
            editor_handle_key(k, &current_mode);
            if (current_mode != MODE_EDITOR) ui_draw();
        } else if (current_mode == MODE_GAME) {
            int sh = snake_handle_key(k);
            if (sh == 1) {
                /* ESC pressed — return to browser */
                current_mode = MODE_BROWSER;
                game_timer   = 0;
                input_reset_modifiers();
                ui_draw();
                continue;
            } else if (sh == 2) {
                /* Restart requested — reset timer and redraw immediately */
                game_timer = 0;
                snake_draw();
                continue;
            } else {
                game_timer++;
                if (game_timer >= game_speed) {
                    game_timer = 0;
                    snake_update();
                    snake_draw();
                }
            }
        }

        /* Short delay to keep input responsive but prevent tearing/super fast games */
        for (volatile int i = 0; i < 500000; ++i) {
            asm volatile("nop");
        }
        vga_flush();
    }
}
