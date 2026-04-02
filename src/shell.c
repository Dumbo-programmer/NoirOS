#include "../include/common.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/util.h"
#include "../include/editor.h"
#include "../include/game_snake.h"
#include "../include/noirc.h"
#include "../include/shell.h"
#include "../include/mode.h"
/* NOTE: <stddef.h> removed — this is a freestanding kernel with no hosted headers.
 * NULL is defined below for use as the command table sentinel. */
#define NULL ((void*)0)

/* Status-line row and usable column width — derived from screen constants so
 * they stay correct if WIDTH/HEIGHT ever change.  Do NOT hardcode 23 or 78. */
#define STATUS_ROW  (HEIGHT - 2)
#define STATUS_COLS (WIDTH - 2)

/* Command history ring buffer */
#define CMD_HISTORY_SIZE 10
#define MAX_CMD_LEN      64

static char cmd_history[CMD_HISTORY_SIZE][MAX_CMD_LEN];
static int  history_count = 0;

/* ---- Forward declarations for utility helpers used before they're defined ---- */
static void show_error(const char* message);
static void show_message(const char* message, unsigned char color);

/* ---- Command handler type ---- */
typedef struct {
    const char* name;
    const char* description;
    int (*handler)(const char* args, int* mode, int* explorer_sel);
} shell_command_t;

/* ================================================================
 * Command handlers
 * ================================================================ */

static int cmd_help(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode;
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, "help.txt") == 0) {
            ui_set_selected(i);
            *explorer_sel = i;
            ui_draw();
            return 1;
        }
    }
    /* Fallback: built-in help rendered directly */
    vga_clear();
    const char* help_text[] = {
        "NoirOS Shell Commands:",
        "",
        "help          - Show this help",
        "ls, dir       - List files in current directory",
        "cd <dir>|..|/ - Change directory",
        "mkdir <name>  - Create directory",
        "rmdir <name>  - Remove empty directory",
        "new <n> <t>   - Create file (t: 0=text 1=exe 2=game)",
        "del <name>    - Delete file",
        "cat <file>    - View file contents",
        "edit <file>   - Open text editor",
        "pwd           - Show current path",
        "snake         - Play snake game",
        "clear/cls     - Redraw screen",
        "info          - System information",
        "exit/quit     - Return to file browser",
        "",
        "Navigation: Arrow keys, Page Up/Down",
        "Press any key to continue..."
    };
    int num_lines = 19;
    for (int i = 0; i < num_lines; i++) {
        for (int j = 0; help_text[i][j]; j++)
            vga_putcell(2 + j, 2 + i, help_text[i][j], 0x0F);
    }
    wait_key();
    ui_draw();
    return 1;
}

static int cmd_list(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    ui_draw();
    return 1;
}

static int cmd_edit(const char* args, int* mode, int* explorer_sel) {
    (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: edit <filename>"); return 0; }  
    struct File* f = fs_find(args);
    if (f && f->readonly) {
        show_error("File is read-only and cannot be opened");
        return 0;
    }
    editor_open(args, mode);
    return 1;
}

static int cmd_cat(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: cat <filename>"); return 0; }

    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, args) == 0) {
            struct File* f = fs_get(i);
            vga_clear();

            /* Header bar */
            char title[WIDTH];
            int pos = 0;
            const char* prefix = "Viewing: ";
            for (int j = 0; prefix[j]; j++) title[pos++] = prefix[j];
            for (int j = 0; args[j] && pos < WIDTH - 2; j++) title[pos++] = args[j];
            title[pos] = '\0';
            for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', 0x1F);
            for (int j = 0; title[j] && j < WIDTH - 2; ++j)
                vga_putcell(1 + j, 0, title[j], 0x1F);

            /* Content */
            int line = 2, col = 1;
            for (int j = 0; j < f->length && line < HEIGHT - 2; j++) {
                char ch = f->content[j];
                if (ch == '\n') { line++; col = 1; }
                else if (ch == '\t') { col += 4; if (col >= WIDTH) col = WIDTH - 1; }
                else if (ch >= 32 && ch <= 126 && col < WIDTH - 1)
                    vga_putcell(col++, line, ch, 0x07);
            }

            /* Footer */
            const char* footer = "Press any key to return...";
            for (int j = 0; footer[j]; ++j)
                vga_putcell(1 + j, HEIGHT - 1, footer[j], 0x0E);

            wait_key();
            ui_draw();
            return 1;
        }
    }
    show_error("File not found");
    return 0;
}

static int cmd_snake(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)explorer_sel;
    *mode = MODE_GAME;
    snake_init();
    snake_draw();
    return 1;
}

/* Interactive keyboard tester: shows raw scancode and translated key. ESC to exit. */
static int cmd_kbdtest(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    vga_clear();
    const char* title = "Keyboard Tester - press keys (ESC to exit)";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], 0x1F);
    while (1) {
        int k = read_key();
        for (volatile int i = 0; i < 500000; ++i) asm volatile("nop");
        
        extern void vga_flush(void);
        vga_flush();
        
        int sc = input_get_last_scancode();
        int lk = input_get_last_key();
        /* Clear status lines */
        for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, 2, ' ', 0x07);
        for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, 3, ' ', 0x07);
        char buf[64]; int p = 0;
        /* Show raw scancode */
        const char* s1 = "Last scancode: ";
        for (int i = 0; s1[i] && p < (int)sizeof(buf)-1; ++i) buf[p++] = s1[i];
        /* append decimal */
        int v = sc; if (v == 0) { buf[p++] = '0'; }
        else {
            char digs[12]; int d = 0;
            while (v > 0 && d < (int)sizeof(digs)) { digs[d++] = '0' + (v % 10); v /= 10; }
            for (int i = d - 1; i >= 0; --i) buf[p++] = digs[i];
        }
        buf[p] = '\0';
        for (int i = 0; buf[i]; ++i) vga_putcell(1 + i, 2, buf[i], 0x0F);

        /* Show translated key */
        char buf2[64]; int q = 0;
        const char* s2 = "Translated key: ";
        for (int i = 0; s2[i] && q < (int)sizeof(buf2)-1; ++i) buf2[q++] = s2[i];
        if (lk >= 32 && lk <= 126) buf2[q++] = (char)lk;
        else if (lk == K_ESC) { buf2[q++] = 'E'; buf2[q++] = 'S'; buf2[q++] = 'C'; }
        else if (lk == K_ARROW_UP) { buf2[q++] = '^'; buf2[q++] = 'U'; }
        else if (lk == K_ARROW_DOWN) { buf2[q++] = 'v'; buf2[q++] = 'D'; }
        else if (lk == 0) { buf2[q++] = '-'; }
        buf2[q] = '\0';
        for (int i = 0; buf2[i]; ++i) vga_putcell(1 + i, 3, buf2[i], 0x0F);

        if (k == K_ESC) break;
    }
    ui_draw();
    return 1;
}

static int cmd_clear(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    ui_draw();
    return 1;
}

static int cmd_info(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    vga_clear();
    const char* info_lines[] = {
        "NoirOS System Information",
        "=========================",
        "",
        "Version:      1.0.0",
        "Architecture: x86 (i386)",
        "Display:      80x25 VGA Text Mode",
        "",
        "Features:",
        "  File System Browser",
        "  Text Editor (Ctrl+S save, Ctrl+X exit)",
        "  Snake Game",
        "  Mouse Support (PS/2)",
        "  Command Shell with History",
        "",
        "Build: gcc -m32 -ffreestanding",
        "",
        "",
        "Press any key to continue..."
    };
    int num_lines = 18;
    for (int i = 0; i < num_lines; i++) {
        unsigned char color = (i < 2) ? 0x0E : 0x07;
        for (int j = 0; info_lines[i][j]; j++)
            vga_putcell(2 + j, 2 + i, info_lines[i][j], color);
    }
    wait_key();
    ui_draw();
    return 1;
}

static int cmd_exit(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    /* Return to file browser — no-op since shell_loop will redraw after return */
    show_message("Returned to file browser", 0x0A);
    return 1;
}

static int cmd_cd(const char* args, int* mode, int* explorer_sel) {
    (void)mode;
    if (!args || !args[0]) { show_error("Usage: cd <dir> | .. | /"); return 0; }
    int r = fs_chdir(args);
    if (r == FS_OK) {
        if (explorer_sel) *explorer_sel = 0;
        ui_reset_explorer_scroll();
        ui_set_selected(0);
        ui_draw();
        return 1;
    }
    if (r == FS_ERR_NOTFOUND) show_error("Directory not found");
    else show_error("Failed to change directory");
    return 0;
}

static int cmd_mkdir(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: mkdir <name>"); return 0; }
    char name[MAX_FILENAME]; int i = 0;
    while (args[i] && args[i] != ' ' && i < (int)sizeof(name) - 1) { name[i] = args[i]; i++; }
    name[i] = '\0';
    if (kstrlen(name) == 0) { show_error("Usage: mkdir <name>"); return 0; }
    int r = fs_mkdir(name);
    if (r == FS_OK) { show_message("Directory created", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_EXISTS)  show_error("Directory already exists");
    else if (r == FS_ERR_NOSPACE) show_error("No space for directory");
    else show_error("mkdir failed");
    return 0;
}

static int cmd_rmdir(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: rmdir <name>"); return 0; }
    char name[MAX_FILENAME]; int i = 0;
    while (args[i] && args[i] != ' ' && i < (int)sizeof(name) - 1) { name[i] = args[i]; i++; }
    name[i] = '\0';
    if (kstrlen(name) == 0) { show_error("Usage: rmdir <name>"); return 0; }
    int r = fs_rmdir(name);
    if (r == FS_OK) { show_message("Directory removed", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_DIRNOTEMPTY) show_error("Directory not empty");
    else if (r == FS_ERR_NOTFOUND) show_error("Directory not found");
    else show_error("rmdir failed");
    return 0;
}

static int cmd_new(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: new <name> <type:0-2>"); return 0; }
    char name[MAX_FILENAME];
    int t = -1, i = 0;
    while (args[i] && args[i] != ' ' && i < (int)sizeof(name) - 1)
        { name[i] = args[i]; i++; }
    name[i] = '\0';
    while (args[i] == ' ') i++;
    if (args[i] >= '0' && args[i] <= '3') t = args[i] - '0';
    if (kstrlen(name) == 0 || t < 0) { show_error("Usage: new <name> <type:0-2>"); return 0; }
    int r = fs_create(name, (u8)t);
    if (r == FS_OK) { show_message("File created", 0x0A); ui_draw(); return 1; }
    if (r == FS_ERR_EXISTS) show_error("File already exists");
    else if (r == FS_ERR_NOSPACE) show_error("No space for file");
    else show_error("Failed to create file");
    return 0;
}

static int cmd_run(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) { show_error("Usage: run <file.nc>"); return 0; }
    for (int i = 0; i < fs_count(); ++i) {
        if (kstrcmp(fs_get(i)->name, args) == 0) {
            struct File* f = fs_get(i);
            if (f->type != FILE_NOIRC) { show_error("Not a .nc file"); return 0; }
            noirc_run(f);
            return 1;
        }
    }
    show_error("File not found");
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

/* ---- Command table (NULL-terminated sentinel) ---- */
static const shell_command_t commands[] = {
    {"help",  "Show available commands",    cmd_help},
    {"ls",    "List files",                 cmd_list},
    {"dir",   "List files",                 cmd_list},
    {"cd",    "Change directory",           cmd_cd},
    {"mkdir", "Make directory",             cmd_mkdir},
    {"rmdir", "Remove empty directory",     cmd_rmdir},
    {"new",   "Create file",               cmd_new},
    {"del",   "Delete file",               cmd_del},
    {"cat",   "View file contents",         cmd_cat},
    {"edit",  "Edit a file",               cmd_edit},
    {"pwd",   "Print working directory",    cmd_pwd},
    {"snake", "Play snake game",            cmd_snake},
    {"kbdtest","Interactive keyboard test", cmd_kbdtest},
    {"clear", "Redraw screen",             cmd_clear},
    {"cls",   "Redraw screen",             cmd_clear},
    {"info",  "System information",         cmd_info},
    {"run",   "Run Noir C file",            cmd_run},
    {"exit",  "Return to browser",          cmd_exit},
    {"quit",  "Return to browser",          cmd_exit},
    {NULL, NULL, NULL}  /* sentinel — checked with commands[i].name != NULL */
};

/* ================================================================
 * Utility display helpers
 * ================================================================ */

static void show_error(const char* message) {
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, STATUS_ROW, ' ', 0x07);
    for (int i = 0; message[i] && i < STATUS_COLS; i++) {
        vga_putcell(1 + i, STATUS_ROW, message[i], 0x0C);
    }
    wait_key();
}

static void show_message(const char* message, unsigned char color) {
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, STATUS_ROW, ' ', 0x07);
    for (int i = 0; message[i] && i < STATUS_COLS; i++)
        vga_putcell(1 + i, STATUS_ROW, message[i], color);
}

/* ================================================================
 * History
 * ================================================================ */

static void add_to_history(const char* cmd) {
    if (!cmd[0]) return;
    /* Suppress consecutive duplicates */
    if (history_count > 0 &&
        kstrcmp(cmd_history[(history_count - 1) % CMD_HISTORY_SIZE], cmd) == 0)
        return;

    int index = history_count % CMD_HISTORY_SIZE;
    int len = 0;
    while (cmd[len] && len < MAX_CMD_LEN - 1)
        { cmd_history[index][len] = cmd[len]; len++; }
    cmd_history[index][len] = '\0';
    history_count++;
}

/* ================================================================
 * Line editor with history navigation
 * ================================================================ */

/* Read a line from the user on STATUS_ROW.
 * prompt: string displayed before the cursor.
 * out/outsz: output buffer.
 * first_key: if non-zero, process this key before blocking for more input
 *            (pass 0 for a normal blocking read from the start).
 * Returns 1 if confirmed with Enter, 0 if aborted with ESC.
 *
 * History ring-buffer note: history_count is the total number of commands
 * ever entered and grows without bound.  The ring stores at most
 * CMD_HISTORY_SIZE entries; the oldest reachable entry is therefore at
 * logical position (history_count - CMD_HISTORY_SIZE), clamped to 0.
 * hist_pos tracks a logical index into [min_hist, history_count].
 */
static int shell_readline_preloaded(const char* prompt, char* out, int outsz, int first_key) {
    /* Clear the status row */
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, STATUS_ROW, ' ', 0x07);

    /* Draw prompt */
    int pi = 0;
    for (; prompt[pi]; ++pi) vga_putcell(1 + pi, STATUS_ROW, prompt[pi], 0x0E);

    const int prompt_end = 1 + pi;
    int cx   = prompt_end;
    int ipos = 0;

    /* hist_pos starts just past the newest entry; UP moves it backwards. */
    int hist_pos = history_count;

    int pk = first_key; /* preloaded key; 0 means none */

    while (1) {
        int ch;
        if (pk) { ch = pk; pk = 0; }
        else ch = wait_key();

        if (ch == '\n' || ch == '\r') {
            out[ipos] = '\0';
            return 1;
        }

        if (ch == K_ESC) {
            out[0] = '\0';
            ui_draw();
            input_reset_modifiers();
            return 0;
        }

        if (ch == '\b') {
            if (ipos > 0) { ipos--; cx--; vga_putcell(cx, STATUS_ROW, ' ', 0x07); }
            continue;
        }

        if (ch == K_TAB) {
            ui_toggle_active_panel();
            ui_draw();
            cx = 1;
            for (int i = 0; prompt[i] && cx < WIDTH - 1; i++) {
                vga_putcell(cx++, STATUS_ROW, prompt[i], 0x0E);
            }
            for (int i = 0; i < ipos && cx < WIDTH - 1; i++) {
                vga_putcell(cx++, STATUS_ROW, out[i], 0x0F);
            }
            continue;
        }

        if (ch == K_ARROW_UP) {
            if (history_count == 0) continue;
            /* Oldest reachable logical index in the ring */
            int min_hist = (history_count > CMD_HISTORY_SIZE)
                           ? history_count - CMD_HISTORY_SIZE : 0;
            if (hist_pos > min_hist) hist_pos--;
            while (ipos > 0) { ipos--; cx--; vga_putcell(cx, STATUS_ROW, ' ', 0x07); }
            const char* h = cmd_history[hist_pos % CMD_HISTORY_SIZE];
            for (int i = 0; h[i] && ipos < outsz - 1; i++) {
                out[ipos++] = h[i];
                vga_putcell(cx++, STATUS_ROW, h[i], 0x0F);
            }
            continue;
        }

        if (ch == K_ARROW_DOWN) {
            if (history_count == 0) continue;
            /* Clear current input */
            while (ipos > 0) { ipos--; cx--; vga_putcell(cx, STATUS_ROW, ' ', 0x07); }
            if (hist_pos < history_count) hist_pos++;
            if (hist_pos < history_count) {
                const char* h = cmd_history[hist_pos % CMD_HISTORY_SIZE];
                for (int i = 0; h[i] && ipos < outsz - 1; i++) {
                    out[ipos++] = h[i];
                    vga_putcell(cx++, STATUS_ROW, h[i], 0x0F);
                }
            }
            /* else hist_pos == history_count → input left blank */
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            if (ipos < outsz - 1 && cx < WIDTH - 1) {
                out[ipos++] = (char)ch;
                vga_putcell(cx++, STATUS_ROW, (char)ch, 0x0F);
            }
        }
    }
}
/* ================================================================
 * Command dispatch
 * ================================================================ */

static int execute_command(const char* input, int* mode, int* explorer_sel) {
    if (!input[0]) return 1;

    /* Separate command token from arguments */
    int cmd_len = 0;
    while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;

    const char* args = "";
    if (input[cmd_len] == ' ') {
        args = &input[cmd_len + 1];
        while (*args == ' ') args++;  /* skip extra spaces */
    }

    /* Walk command table — use kstrncmp + length check so "lss" doesn't match "ls" */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (kstrncmp(input, commands[i].name, cmd_len) == 0 &&
            kstrlen(commands[i].name) == cmd_len)
            return commands[i].handler(args, mode, explorer_sel);
    }

    /* Unknown command */
    char err[WIDTH];
    int p = 0;
    const char* pfx = "Unknown command: ";
    for (int i = 0; pfx[i] && p < (int)sizeof(err) - 1; i++) err[p++] = pfx[i];
    for (int i = 0; i < cmd_len && p < (int)sizeof(err) - 1; i++) err[p++] = input[i];
    err[p] = '\0';
    show_error(err);
    return 0;
}

/* ================================================================
 * Public shell loop — called each iteration from kernel_main
 * ================================================================ */

int shell_loop(int explorer_sel_in, int* mode, int first_key) {
    int explorer_sel = explorer_sel_in;
    int k = first_key;

    ui_handle_key(k);   /* let UI react to Fn / button presses */

    /* Consume Fn keys — already handled by ui_handle_key */
    if (k == K_F1 || k == K_F2 || k == K_F3) {
        ui_draw();
        input_reset_modifiers();
        return ui_get_selected();
    }

    if (k == K_ARROW_UP || k == 'w' || k == 'W') {
        if (ui_get_active_panel() == 0) {
            int sel = ui_get_selected();
            int total = fs_dir_count() + fs_count();
            if (total <= 0) sel = 0;
            else sel = (sel > 0) ? sel - 1 : (total - 1);
            ui_set_selected(sel);
        } else {
            ui_scroll_viewer(-1);
        }
        ui_draw();
        input_reset_modifiers();
    } else if (k == K_ARROW_DOWN || k == 's' || k == 'S') {
        if (ui_get_active_panel() == 0) {
            int sel = ui_get_selected();
            int total = fs_dir_count() + fs_count();
            if (total <= 0) sel = 0;
            else { int max = total - 1; sel = (sel < max) ? sel + 1 : 0; }
            ui_set_selected(sel);
        } else {
            ui_scroll_viewer(1);
        }
        ui_draw();
        input_reset_modifiers();
    } else if (k == K_PAGE_UP) {
        ui_scroll_viewer(-3);
        ui_draw();
        input_reset_modifiers();
    } else if (k == K_PAGE_DOWN) {
        ui_scroll_viewer(3);
        ui_draw();
        input_reset_modifiers();
    } else if (k == '\n' || k == '\r') {
        if (ui_get_active_panel() == 0) {
            int sel = ui_get_selected();
            int dir_count = fs_dir_count();
            if (sel < dir_count) {
                struct Dir* d = fs_dir_get(sel);
                char cmd[MAX_CMD_LEN];
                int p = 0;
                const char* c = "cd ";
                while (*c) cmd[p++] = *c++;
                for (int i = 0; d->name[i] && p < MAX_CMD_LEN - 1; i++)
                    cmd[p++] = d->name[i];
                cmd[p] = '\0';
                execute_command(cmd, mode, &explorer_sel);
            } else {
                struct File* f = fs_get(sel - dir_count);
                int len = kstrlen(f->name);
                if (f->type == FILE_NOIRC || (len >= 3 && kstrcmp(f->name + len - 3, ".nc") == 0)) {
                    char cmd[MAX_CMD_LEN];
                    int p = 0;
                    const char* c = "run ";
                    while (*c) cmd[p++] = *c++;
                    for (int i = 0; f->name[i] && p < MAX_CMD_LEN - 1; i++)
                        cmd[p++] = f->name[i];
                    cmd[p] = '\0';
                    execute_command(cmd, mode, &explorer_sel);
                } else if (f->type == FILE_GAME) {
                    execute_command("snake", mode, &explorer_sel);
                } else {
                    char cmd[MAX_CMD_LEN];
                    int p = 0;
                    const char* c = "edit ";
                    while (*c) cmd[p++] = *c++;
                    for (int i = 0; f->name[i] && p < MAX_CMD_LEN - 1; i++)
                        cmd[p++] = f->name[i];
                    cmd[p] = '\0';
                    execute_command(cmd, mode, &explorer_sel);
                }
            }
        }
        if (*mode == MODE_BROWSER) {
            ui_draw();
        }
        input_reset_modifiers();
    }

    return ui_get_selected();
}

/* Public helper: open the command prompt immediately and execute one command. */
int shell_open_prompt(int explorer_sel_in, int* mode) {
    int explorer_sel = explorer_sel_in;
    char input[MAX_CMD_LEN];
    if (shell_readline_preloaded("cmd> ", input, sizeof(input), 0) && input[0]) {
        add_to_history(input);
        execute_command(input, mode, &explorer_sel);
    }
    ui_draw();
    return explorer_sel;
}
