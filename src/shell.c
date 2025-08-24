#include "../include/common.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/util.h"
#include "../include/editor.h"
#include "../include/game_snake.h"
#include "../include/shell.h"
#include "../include/mode.h"
#include <stddef.h> /* for NULL */

/* Command history */
#define CMD_HISTORY_SIZE 10
#define MAX_CMD_LEN 64

static char cmd_history[CMD_HISTORY_SIZE][MAX_CMD_LEN];
static int history_count = 0;
static int history_pos = 0;
static void show_error(const char* message);
static void show_message(const char* message, unsigned char color);
/* Command structure for better organization */
typedef struct {
    const char* name;
    const char* description;
    int (*handler)(const char* args, int* mode, int* explorer_sel);
} shell_command_t;

/* Command handlers */
static int cmd_help(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; /* unused parameters */
    
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, "help.txt") == 0) {
            ui_set_selected(i);
            *explorer_sel = i;
            ui_draw();
            return 1;
        }
    }
    
    /* If help.txt not found, show built-in help */
    vga_clear();
    const char* help_text[] = {
        "NoirOS Shell Commands:",
        "",
        "help          - Show this help",
        "ls, dir       - List files",
        "edit <file>   - Edit a file",
        "cat <file>    - View file contents", 
        "snake         - Play snake game",
        "clear, cls    - Clear screen",
        "info          - System information",
        "exit, quit    - Return to file browser",
        " cd <dir>|..|/      - change directory\n"
        " mkdir <name>       - make directory\n"
        " rmdir <name>       - remove EMPTY directory\n"
        " new <name> <type>  - create file (type: 0 text, 1 exe, 2 game)\n"
        " del <name>         - delete file\n",
        "info               - show system information\n"
        " pwd                - show current path\n",
        "",
        "Navigation: Arrow keys, Page Up/Down",
        "Press any key to continue..."
    };
    
    for (int i = 0; i < 13; i++) {
        for (int j = 0; help_text[i][j]; j++) {
            vga_putcell(2 + j, 2 + i, help_text[i][j], 0x0F);
        }
    }
    
    read_key();
    ui_draw();
    return 1;
}

static int cmd_list(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel; /* unused */
    ui_draw(); /* Just refresh the file listing */
    return 1;
}

static int cmd_edit(const char* args, int* mode, int* explorer_sel) {
    (void)explorer_sel; /* unused */
    
    if (!args || !args[0]) {
        show_error("Usage: edit <filename>");
        return 0;
    }
    
    editor_open(args, mode);
    return 1;
}

static int cmd_cat(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel; /* unused */
    
    if (!args || !args[0]) {
        show_error("Usage: cat <filename>");
        return 0;
    }
    
    /* Find and display file */
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, args) == 0) {
            struct File* f = fs_get(i);
            vga_clear();
            
            /* Title */
            char title[80];
            int pos = 0;
            const char* prefix = "Viewing: ";
            for (int j = 0; prefix[j]; j++) title[pos++] = prefix[j];
            for (int j = 0; args[j] && pos < 70; j++) title[pos++] = args[j];
            title[pos] = '\0';
            
            for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
            for (int j = 0; title[j] && j < WIDTH - 2; ++j) {
                vga_putcell(1 + j, 0, title[j], 0x1F);
            }
            
            /* Content */
            int line = 2, col = 1;
            for (int j = 0; j < f->length && line < HEIGHT - 2; j++) {
                char ch = f->content[j];
                if (ch == '\n') {
                    line++;
                    col = 1;
                } else if (ch >= 32 && ch <= 126) {
                    if (col < WIDTH - 1) {
                        vga_putcell(col, line, ch, 0x07);
                        col++;
                    }
                } else if (ch == '\t') {
                    col += 4;
                    if (col >= WIDTH) col = WIDTH - 1;
                }
            }
            
            /* Footer */
            const char* footer = "Press any key to return...";
            for (int j = 0; footer[j]; ++j) {
                vga_putcell(1 + j, HEIGHT - 1, footer[j], 0x0E);
            }
            
            read_key();
            ui_draw();
            return 1;
        }
    }
    
    show_error("File not found");
    return 0;
}

static int cmd_snake(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)explorer_sel; /* unused */
    
    *mode = MODE_GAME;
    snake_init();
    snake_draw();
    return 1;
}

static int cmd_clear(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel; /* unused */
    
    ui_draw();
    return 1;
}

static int cmd_info(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel; /* unused */
    
    vga_clear();
    
    const char* info_lines[] = {
        "NoirOS System Information",
        "========================",
        "",
        "Version: 1.0.0",
        "Architecture: x86",
        "Display: 80x25 VGA Text Mode",
        "",
        "Features:",
        "- File System Browser",
        "- Text Editor with Syntax Support",
        "- Snake Game",
        "- Mouse Support",
        "- Command Shell",
        "",
        "Memory Usage:",
        "- Kernel: ~64KB",
        "- Available RAM: Detected at boot",
        "",
        "Press any key to continue..."
    };
    
    for (int i = 0; i < 19; i++) {
        unsigned char color = (i == 0 || i == 1) ? 0x0E : 0x07;
        for (int j = 0; info_lines[i][j]; j++) {
            vga_putcell(2 + j, 2 + i, info_lines[i][j], color);
        }
    }
    
    read_key();
    ui_draw();
    return 1;
}

static int cmd_exit(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel; /* unused */
    
    /* Just return to browser - no special action needed */
    show_message("Returned to file browser", 0x0A);
    return 1;
}
static int cmd_cd(const char* args, int* mode, int* explorer_sel) {
    (void)mode;
    if (!args || !args[0]) {
        show_error("Usage: cd <dir> | .. | /");
        return 0;
    }
    int r = fs_chdir(args);
    if (r == FS_OK) {
        /* reset selection to 0 so UI shows first item in folder */
        if (explorer_sel) *explorer_sel = 0;
        ui_set_selected(0);
        ui_draw();
        return 1;
    } else if (r == FS_ERR_NOTFOUND) {
        show_error("Directory not found");
    } else {
        show_error("Failed to change directory");
    }
    return 0;
}

static int cmd_mkdir(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: mkdir <name>"); return 0; }
    int r = fs_mkdir(args);
    if (r == FS_OK) { show_message("Directory created", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_EXISTS) show_error("Directory already exists");
    else if (r == FS_ERR_NOSPACE) show_error("No space for directory");
    else show_error("mkdir failed");
    return 0;
}

static int cmd_rmdir(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: rmdir <name>"); return 0; }
    int r = fs_rmdir(args);
    if (r == FS_OK) { show_message("Directory removed", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_DIRNOTEMPTY) show_error("Directory not empty");
    else if (r == FS_ERR_NOTFOUND) show_error("Directory not found");
    else show_error("rmdir failed");
    return 0;
}

static int cmd_new(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    /* new <name> <type>  - type: 0 text, 1 exe, 2 game */
    if (!args || !args[0]) { show_error("Usage: new <name> <type>"); return 0; }
    /* parse name and type */
    char name[MAX_FILENAME];
    int t = -1;
    int i = 0;
    /* copy name token */
    while (args[i] && args[i] != ' ' && i < (int)sizeof(name)-1) { name[i] = args[i]; i++; }
    name[i] = '\0';
    while (args[i] == ' ') i++;
    if (args[i]) t = args[i] - '0';
    if (kstrlen(name) == 0 || (t < 0 || t > 2)) { show_error("Usage: new <name> <type:0-2>"); return 0; }

    int r = fs_create(name, (u8)t);
    if (r == FS_OK) { show_message("File created", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_EXISTS) show_error("File already exists");
    else if (r == FS_ERR_NOSPACE) show_error("No space for file");
    else show_error("Failed to create file");
    return 0;
}

static int cmd_del(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: del <name>"); return 0; }
    int r = fs_delete(args);
    if (r == FS_OK) { show_message("File deleted", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_NOTFOUND) show_error("File not found");
    else if (r == FS_ERR_RDONLY) show_error("File is read-only");
    else show_error("Failed to delete file");
    return 0;
}

static int cmd_pwd(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    char path[128];
    fs_pwd(path, sizeof(path));
    show_message(path, 0x0F);
    return 1;
}
/* Command table */
static const shell_command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"ls", "List files", cmd_list},
    {"dir", "List files", cmd_list},
    {"edit", "Edit a file", cmd_edit},
    {"cat", "View file contents", cmd_cat},
    {"snake", "Play snake game", cmd_snake},
    {"clear", "Clear screen", cmd_clear},
    {"cls", "Clear screen", cmd_clear},
    {"info", "System information", cmd_info},
    {"exit", "Return to browser", cmd_exit},
    {"quit", "Return to browser", cmd_exit},
    {"cd",   "Change directory",   cmd_cd},
    {"mkdir","Make directory",     cmd_mkdir},
    {"rmdir","Remove directory",   cmd_rmdir},
    {"new",  "Create file",        cmd_new},
    {"del",  "Delete file",        cmd_del},
    {"pwd",  "Print working dir",  cmd_pwd},

    {NULL, NULL, NULL} /* Terminator */
};

/* Utility functions */
static void show_error(const char* message) {
    for (int x = 1; x < 78; ++x) vga_putcell(x, 23, ' ', 0x07);
    for (int i = 0; message[i] && i < 70; i++) {
        vga_putcell(1 + i, 23, message[i], 0x0C);
    }
    read_key();
}

static void show_message(const char* message, unsigned char color) {
    for (int x = 1; x < 78; ++x) vga_putcell(x, 23, ' ', 0x07);
    for (int i = 0; message[i] && i < 70; i++) {
        vga_putcell(1 + i, 23, message[i], color);
    }
}

/* Add command to history */
static void add_to_history(const char* cmd) {
    if (!cmd[0]) return;
    
    /* Don't duplicates of the last command */
    if (history_count > 0 && 
        kstrcmp(cmd_history[(history_count - 1) % CMD_HISTORY_SIZE], cmd) == 0) {
        return;
    }
    
    int index = history_count % CMD_HISTORY_SIZE;
    int len = 0;
    while (cmd[len] && len < MAX_CMD_LEN - 1) {
        cmd_history[index][len] = cmd[len];
        len++;
    }
    cmd_history[index][len] = '\0';
    
    history_count++;
    history_pos = history_count;
}
/* history support starts here */
static int shell_readline_enhanced(const char *prompt, char *out, int outsz) {
    const int sy = 23;
    const int sx = 1;
    int cx;

    /* Clear status area */
    for (int x = sx; x < 78; ++x) vga_putcell(x, sy, ' ', 0x07);

    /* Draw prompt */
    int pi = 0;
    for (; prompt[pi]; ++pi) vga_putcell(sx + pi, sy, prompt[pi], 0x0E);

    cx = sx + pi;
    int ipos = 0;
    int local_history_pos = history_count;

    while (1) {
        int ch = read_key();

        if (ch == '\n' || ch == '\r') {
            out[ipos] = '\0';
            return 1;
        }

        if (ch == K_ESC) {
            out[0] = '\0';
            ui_draw();
            return 0;
        }

        if (ch == '\b') {
            if (ipos > 0) {
                ipos--;
                cx--;
                vga_putcell(cx, sy, ' ', 0x07);
            }
            continue;
        }
        
        /* History navigation */
        if (ch == K_ARROW_UP && history_count > 0) {
            if (local_history_pos > 0) local_history_pos--;
            
            /* Clear current input */
            while (ipos > 0) {
                ipos--;
                cx--;
                vga_putcell(cx, sy, ' ', 0x07);
            }
            
            /* Copy from history */
            const char* hist_cmd = cmd_history[local_history_pos % CMD_HISTORY_SIZE];
            for (int i = 0; hist_cmd[i] && ipos < outsz - 1; i++) {
                out[ipos] = hist_cmd[i];
                vga_putcell(cx++, sy, hist_cmd[i], 0x0F);
                ipos++;
            }
            continue;
        }
        
        if (ch == K_ARROW_DOWN && history_count > 0) {
            if (local_history_pos < history_count - 1) {
                local_history_pos++;
                
                /* Clear current input */
                while (ipos > 0) {
                    ipos--;
                    cx--;
                    vga_putcell(cx, sy, ' ', 0x07);
                }
                
                /* Copy from history */
                const char* hist_cmd = cmd_history[local_history_pos % CMD_HISTORY_SIZE];
                for (int i = 0; hist_cmd[i] && ipos < outsz - 1; i++) {
                    out[ipos] = hist_cmd[i];
                    vga_putcell(cx++, sy, hist_cmd[i], 0x0F);
                    ipos++;
                }
            }
            continue;
        }

        /* Regular character input */
        if (ch >= 32 && ch <= 126) {
            if (ipos < outsz - 1) {
                out[ipos++] = (char)ch;
                vga_putcell(cx++, sy, (char)ch, 0x0F);
            }
        }
    }
}

/* Parse and execute command */
static int execute_command(const char* input, int* mode, int* explorer_sel) {
    if (!input[0]) return 1;
    
    /* Find command separator */
    int cmd_len = 0;
    while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;
    
    /* Extract arguments */
    const char* args = "";
    if (input[cmd_len] == ' ') {
        args = &input[cmd_len + 1];
        /* Skip leading spaces in arguments */
        while (*args == ' ') args++;
    }
    
    /* Find and execute command */
    for (int i = 0; commands[i].name; i++) {
        if (kstrncmp(input, commands[i].name, cmd_len) == 0 && 
            kstrlen(commands[i].name) == cmd_len) {
            return commands[i].handler(args, mode, explorer_sel);
        }
    }
    
    /* Command not found */
    char error_msg[80];
    int pos = 0;
    const char* prefix = "Unknown command: ";
    for (int i = 0; prefix[i]; i++) error_msg[pos++] = prefix[i];
    for (int i = 0; i < cmd_len && pos < 70; i++) error_msg[pos++] = input[i];
    error_msg[pos] = '\0';
    
    show_error(error_msg);
    return 0;
}

/* Main shell loop function */
/* Replace your shell_loop() with this version */
int shell_loop(int explorer_sel_in, int *mode) {
    /* Note: explorer_sel is stored in UI; accept explorer_sel_in but use ui_get_selected() */
    int explorer_sel = explorer_sel_in;

    int k = read_key();

    /* Let UI see the key first (sets pressed states / invokes callbacks) */
    ui_handle_key(k);

    /* If F1/F2/F3 were pressed, handled by ui_handle_key and don't want to
       feed them to the rest of shell logic. Just redraw and return. */
#ifdef K_F1
    if (k == K_F1) { ui_draw(); return ui_get_selected(); }
#endif
#ifdef K_F2
    if (k == K_F2) { ui_draw(); return ui_get_selected(); }
#endif
#ifdef K_F3
    if (k == K_F3) { ui_draw(); return ui_get_selected(); }
#endif

    /* Navigation keys: use combined total (dirs + files) */
    int total = fs_dir_count() + fs_count();

    if (k == K_ARROW_UP || k == 'w' || k == 'W') {
        int sel = ui_get_selected();
        if (sel > 0) sel--;
        ui_set_selected(sel);
        ui_draw();
        return ui_get_selected();
    } else if (k == K_ARROW_DOWN || k == 's' || k == 'S') {
        int sel = ui_get_selected();
        int max = (total > 0) ? total - 1 : 0;
        if (sel < max) sel++;
        ui_set_selected(sel);
        ui_draw();
        return ui_get_selected();
    } else if (k == K_PAGE_UP) {
        ui_scroll_viewer(-3);
        ui_draw();
        return ui_get_selected();
    } else if (k == K_PAGE_DOWN) {
        ui_scroll_viewer(3);
        ui_draw();
        return ui_get_selected();
    } else if (k == '\n' || k == '\r') {
        /* CMD input mode */
        char input[MAX_CMD_LEN];
        if (!shell_readline_enhanced("cmd> ", input, sizeof(input))) {
            return ui_get_selected(); /* Aborted */
        }

        if (input[0]) {
            add_to_history(input);
            execute_command(input, mode, &explorer_sel); /* note: this expects explorer_sel index-by-ref */
        }

        ui_draw();
        return ui_get_selected();
    }

    /* Nothing handled: just return current selection */
    return ui_get_selected();
}

