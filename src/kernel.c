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
void snake_handle_key(int key);

enum { MODE_BROWSER = 0, MODE_EDITOR = 1, MODE_GAME = 2 };
static int current_mode = MODE_BROWSER;

void kernel_main(void) {
            // Main entry point for NoirOS kernel. Initializes filesystem and UI, then enters main loop.
            // Handles mode switching, keyboard input, and command interface.
            // If you see this, congrats! You're running an OS that fits in your head (and maybe your heart).
        // Main entry point for NoirOS kernel. Initializes filesystem and UI, then enters main loop.
        // Handles mode switching, keyboard input, and command interface.
    init_filesystem();
    ui_draw();

    int explorer_sel = ui_get_selected();
    int game_timer = 0;
    const int game_speed = 15; /* Adjust for snake game speed */

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
                /* Command input mode */
                const int sy = 23; /* Status line */
                const int sx = 1;
                
                /* Clear command area */
                for (int x = sx; x < 78; ++x) {
                    vga_putcell(x, sy, ' ', 0x07);
                }
                
                /* Show prompt */
                const char* prompt = "cmd> ";
                for (int i = 0; prompt[i]; ++i) {
                    vga_putcell(sx + i, sy, prompt[i], 0x0E);
                }
                
                int cx = sx + 5;
                int explorer_sel = ui_get_selected();
                int game_timer = 0;
                const int game_speed = 15;

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
                    }
                    // ...existing code...
            int handle_command_input(int explorer_sel, int* current_mode) {
                const int sy = 23;
                const int sx = 1;
                for (int x = sx; x < 78; ++x) vga_putcell(x, sy, ' ', 0x07);
                const char* prompt = "cmd> ";
                for (int i = 0; prompt[i]; ++i) vga_putcell(sx + i, sy, prompt[i], 0x0E);
                int cx = sx + 5;
                char input[64];
                int ipos = 0;
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
                    if (kstrcmp(input, "help") == 0) {
                        for (int i = 0; i < fs_count(); ++i) {
                            if (kstrcmp(fs_get(i)->name, "help.txt") == 0) {
                                ui_set_selected(i);
                                explorer_sel = i;
                                break;
                            }
                        }
                    } else if (kstrcmp(input, "snake") == 0) {
                        *current_mode = MODE_GAME;
                        snake_init();
                        snake_draw();
                    } else if (kstrcmp(input, "ls") == 0 || kstrcmp(input, "dir") == 0) {
                        ui_draw();
                    } else if (kstrncmp(input, "edit ", 5) == 0) {
                        const char* fname = input + 5;
                        int idx = -1;
                        for (int i = 0; i < fs_count(); ++i) {
                            if (kstrcmp(fs_get(i)->name, fname) == 0) { idx = i; break; }
                        }
                        if (idx >= 0) {
                            *current_mode = MODE_EDITOR;
                            editor_open(fs_get(idx));
                        }
                    }
                }
                return explorer_sel;
            }
            }
        }

        /* Smaller delay to make input more responsive */
        for (volatile int i = 0; i < 10000; ++i) { 
            asm volatile("nop"); 
        }
    }
}