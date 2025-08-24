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
#include "../include/mouse.h"

enum { MODE_BROWSER = 0, MODE_EDITOR = 1, MODE_GAME = 2 };
static int current_mode = MODE_BROWSER;

/* Mouse cursor state */
static char saved_char = ' ';
static unsigned char saved_attr = 0x07;
static int last_mouse_x = -1, last_mouse_y = -1;

/* Simple mouse cursor management */
static void update_mouse_cursor(void) {
    mouse_state_t* mouse = get_mouse_state();
    
    /* Restore previous position */
    if (last_mouse_x >= 0 && last_mouse_y >= 0) {
        vga_putcell(last_mouse_x, last_mouse_y, saved_char, saved_attr);
    }
    
    /* Save new position and draw cursor */
    if (mouse->x >= 0 && mouse->x < 80 && mouse->y >= 0 && mouse->y < 25) {
        saved_char = vga_getcell_char(mouse->x, mouse->y);
        saved_attr = vga_getcell_attr(mouse->x, mouse->y);
        
        /* Different cursor styles per mode */
        char cursor = (current_mode == MODE_BROWSER) ? '>' : 
                     (current_mode == MODE_EDITOR) ? '|' : '+';
        unsigned char color = (current_mode == MODE_GAME) ? 0x0C : 0x0F;
        
        vga_putcell(mouse->x, mouse->y, cursor, color);
        last_mouse_x = mouse->x;
        last_mouse_y = mouse->y;
    }
}

/* Handle mouse clicks in different modes */
static void handle_mouse_input(int *explorer_sel) {
    mouse_state_t* mouse = get_mouse_state();
    static unsigned char prev_buttons = 0;
    
    /* Left click - different behavior per mode */
    if ((mouse->buttons & 1) && !(prev_buttons & 1)) {
        if (current_mode == MODE_BROWSER && mouse->x < 40) {
            /* Click in file list area */
            int clicked_file = mouse->y - 2;
            if (clicked_file >= 0 && clicked_file < fs_count()) {
                *explorer_sel = clicked_file;
                ui_set_selected(*explorer_sel);
                ui_draw();
            }
        }
        else if (current_mode == MODE_EDITOR) {
            #ifdef EDITOR_MOUSE_SUPPORT
            editor_set_cursor_pos(mouse->x, mouse->y);
            editor_draw();
            #endif
        }
        else if (current_mode == MODE_GAME) {
            /* Convert mouse click to directional input */
            int center_x = 40, center_y = 12;
            int dx = mouse->x - center_x;
            int dy = mouse->y - center_y;
            
            if (dx > 0 && dx > (dy < 0 ? -dy : dy)) snake_handle_key(K_ARROW_RIGHT);
            else if (dx < 0 && -dx > (dy < 0 ? -dy : dy)) snake_handle_key(K_ARROW_LEFT);
            else if (dy > 0) snake_handle_key(K_ARROW_DOWN);
            else if (dy < 0) snake_handle_key(K_ARROW_UP);
        }
    }
    
    /* Right click - scroll or context actions */
    if ((mouse->buttons & 2) && !(prev_buttons & 2)) {
        if (current_mode == MODE_BROWSER && mouse->x >= 40) {
            ui_scroll_viewer(mouse->y < 12 ? -1 : 1);
            ui_draw();
        }
    }
    
    prev_buttons = mouse->buttons;
}

void kernel_main(void) {
    init_filesystem();
    init_mouse();
    ui_draw();

    int explorer_sel = ui_get_selected();
    int game_timer = 0;
    const int game_speed = 15;

    while (1) {
        int k = read_key();
        
        /* Update mouse cursor */
        update_mouse_cursor();
        
        /* Handle mouse input */
        handle_mouse_input(&explorer_sel);

        /* Handle keyboard input by mode */
        if (current_mode == MODE_BROWSER) {
            explorer_sel = shell_loop(explorer_sel, &current_mode);

        } else if (current_mode == MODE_EDITOR) {
            if (k == K_ESC) {
                current_mode = MODE_BROWSER;
                ui_draw();
            } else if (k != 0) {
                editor_handle_key(k, &current_mode);
                if (current_mode == MODE_BROWSER) {
                    ui_draw();
                } else {
                    editor_draw();
                }
            }

        } else if (current_mode == MODE_GAME) {
            if (k == K_ESC) { 
                current_mode = MODE_BROWSER; 
                ui_draw(); 
                continue; 
            }
            if (k != 0) snake_handle_key(k);

            game_timer++;
            if (game_timer >= game_speed) { 
                snake_update(); 
                snake_draw(); 
                game_timer = 0; 
            }
        }

        /* Small delay */
        for (volatile int i = 0; i < 10000; ++i) { 
            asm volatile("nop"); 
        }
    }
}