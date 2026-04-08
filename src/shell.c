#include "../include/common.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/util.h"
#include "../include/editor.h"
#include "../include/game_snake.h"
#include "../include/noirc.h"
#include "../include/games_extra.h"
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
#define CMD_PANEL_H      6
#define CMD_PANEL_Y      (HEIGHT - CMD_PANEL_H)
#define CMD_TEXT_ATTR    ATTR_PROMPT
#define CMD_HINT_ATTR    ATTR_BORDER

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
    (void)args; (void)mode; (void)explorer_sel;
    /* Built-in help rendered directly so it always matches active commands. */
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
        "calc <expr>   - Integer calculator",
        "touch <file>  - Create empty text file",
        "rm <name>     - Delete file (alias: del)",
        "cp <a> <b>    - Copy file in current directory",
        "mv <a> <b>    - Move/rename file in current directory",
        "echo <text>   - Print text on status line",
        "history       - Show recent commands",
        "uname         - Show system name",
        "whoami        - Show current user",
        "man           - Alias for help",
        "ll            - Alias for ls",
        "pwd           - Show current path",
        "snake         - Play snake game",
        "pong          - Play pong",
        "dodge         - Dodge falling stars",
        "catch         - Catch falling coins",
        "restart       - Restart the system",
        "reboot        - Alias for restart",
        "clear/cls     - Redraw screen",
        "info          - System information",
        "exit/quit     - Return to file browser",
        "",
        "Navigation: Arrow keys, Page Up/Down",
        "Press any key to continue..."
    };
    int num_lines = (int)(sizeof(help_text) / sizeof(help_text[0]));
    for (int i = 0; i < num_lines; i++) {
        for (int j = 0; help_text[i][j]; j++)
            vga_putcell(2 + j, 2 + i, help_text[i][j], ATTR_NORMAL);
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
            for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', ATTR_TITLE);
            for (int j = 0; title[j] && j < WIDTH - 2; ++j)
                vga_putcell(1 + j, 0, title[j], ATTR_TITLE);

            /* Content */
            int line = 2, col = 1;
            for (int j = 0; j < f->length && line < HEIGHT - 2; j++) {
                char ch = f->content[j];
                if (ch == '\n') { line++; col = 1; }
                else if (ch == '\t') { col += 4; if (col >= WIDTH) col = WIDTH - 1; }
                else if (ch >= 32 && ch <= 126 && col < WIDTH - 1)
                    vga_putcell(col++, line, ch, ATTR_NORMAL);
            }

            /* Footer */
            const char* footer = "Press any key to return...";
            for (int j = 0; footer[j]; ++j)
                vga_putcell(1 + j, HEIGHT - 1, footer[j], ATTR_PROMPT);

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

static int cmd_pong(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    game_pong_run();
    ui_draw();
    return 1;
}

static int cmd_dodge(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    game_dodge_run();
    ui_draw();
    return 1;
}

static int cmd_catch(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    game_catch_run();
    ui_draw();
    return 1;
}

typedef struct {
    const char* p;
    int error;
    const char* error_msg;
} calc_parser_t;

static void calc_skip_ws(calc_parser_t* c) {
    while (*c->p == ' ' || *c->p == '\t') c->p++;
}

static int calc_parse_expr(calc_parser_t* c);

static int calc_parse_factor(calc_parser_t* c) {
    calc_skip_ws(c);
    if (c->error) return 0;

    if (*c->p == '(') {
        c->p++;
        int v = calc_parse_expr(c);
        calc_skip_ws(c);
        if (*c->p != ')') {
            c->error = 1;
            c->error_msg = "Missing ')'";
            return 0;
        }
        c->p++;
        return v;
    }

    if (*c->p == '+' || *c->p == '-') {
        char sign = *c->p++;
        int v = calc_parse_factor(c);
        return (sign == '-') ? -v : v;
    }

    if (*c->p < '0' || *c->p > '9') {
        c->error = 1;
        c->error_msg = "Expected number";
        return 0;
    }

    int v = 0;
    while (*c->p >= '0' && *c->p <= '9') {
        v = v * 10 + (*c->p - '0');
        c->p++;
    }
    return v;
}

static int calc_parse_term(calc_parser_t* c) {
    int v = calc_parse_factor(c);
    while (!c->error) {
        calc_skip_ws(c);
        char op = *c->p;
        if (op != '*' && op != '/' && op != '%') break;
        c->p++;

        int rhs = calc_parse_factor(c);
        if (c->error) break;

        if ((op == '/' || op == '%') && rhs == 0) {
            c->error = 1;
            c->error_msg = "Division by zero";
            return 0;
        }

        if (op == '*') v *= rhs;
        else if (op == '/') v /= rhs;
        else v %= rhs;
    }
    return v;
}

static int calc_parse_expr(calc_parser_t* c) {
    int v = calc_parse_term(c);
    while (!c->error) {
        calc_skip_ws(c);
        char op = *c->p;
        if (op != '+' && op != '-') break;
        c->p++;

        int rhs = calc_parse_term(c);
        if (c->error) break;

        if (op == '+') v += rhs;
        else v -= rhs;
    }
    return v;
}

static int cmd_calc(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args || !args[0]) {
        show_error("Usage: calc <expr>");
        return 0;
    }

    calc_parser_t c;
    c.p = args;
    c.error = 0;
    c.error_msg = "Invalid expression";

    int result = calc_parse_expr(&c);
    calc_skip_ws(&c);

    if (!c.error && *c.p != '\0') {
        c.error = 1;
        c.error_msg = "Unexpected token";
    }

    if (c.error) {
        show_error(c.error_msg);
        return 0;
    }

    char msg[64];
    const char* prefix = "Result: ";
    int p = 0;
    for (int i = 0; prefix[i] && p < (int)sizeof(msg) - 1; ++i) msg[p++] = prefix[i];
    int_to_dec(&msg[p], result);
    show_message(msg, ATTR_SUCCESS);
    return 1;
}

/* Interactive keyboard tester: shows raw scancode and translated key. ESC to exit. */
static int cmd_kbdtest(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    vga_clear();
    const char* title = "Keyboard Tester - press keys (ESC to exit)";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], ATTR_TITLE);
    while (1) {
        int k = read_key();
        for (volatile int i = 0; i < 500000; ++i) asm volatile("nop");
        
        extern void vga_flush(void);
        vga_flush();
        
        int sc = input_get_last_scancode();
        int lk = input_get_last_key();
        /* Clear status lines */
        for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, 2, ' ', ATTR_NORMAL);
        for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, 3, ' ', ATTR_NORMAL);
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
        for (int i = 0; buf[i]; ++i) vga_putcell(1 + i, 2, buf[i], ATTR_PROMPT);

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
        for (int i = 0; buf2[i]; ++i) vga_putcell(1 + i, 3, buf2[i], ATTR_PROMPT);

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

static int cmd_restart(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    show_restart_screen();
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
        "  Text Editor (Type to edit, F2 save, Esc exit)",
        "  Snake Game",
        "  Pong / Dodge / Catch mini-games",
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
        unsigned char color = (i < 2) ? ATTR_PROMPT : ATTR_NORMAL;
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
    show_message("Returned to file browser", ATTR_SUCCESS);
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
    if (r == FS_OK) { show_message("Directory created", ATTR_SUCCESS); ui_draw(); return 1; }
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
    if (r == FS_OK) { show_message("Directory removed", ATTR_SUCCESS); ui_draw(); return 1; }
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
    if (r == FS_OK) { show_message("File created", ATTR_SUCCESS); ui_draw(); return 1; }
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
    if (r == FS_OK) { show_message("File deleted", ATTR_SUCCESS); ui_draw(); return 1; }
    if (r == FS_ERR_NOTFOUND) show_error("File not found");
    else if (r == FS_ERR_RDONLY) show_error("File is read-only");
    else show_error("Failed to delete file");
    return 0;
}

static int cmd_pwd(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    char path[128];
    fs_pwd(path, sizeof(path));
    show_message(path, ATTR_PROMPT);
    return 1;
}

static void parse_one_arg(const char* args, char* out, int outsz) {
    int i = 0;
    if (!args) { out[0] = '\0'; return; }
    while (args[i] == ' ' || args[i] == '\t') i++;
    int p = 0;
    while (args[i] && args[i] != ' ' && args[i] != '\t' && p < outsz - 1) out[p++] = args[i++];
    out[p] = '\0';
}

static int parse_two_args(const char* args, char* a, int asz, char* b, int bsz) {
    int i = 0;
    int p = 0;
    if (!args) return 0;

    while (args[i] == ' ' || args[i] == '\t') i++;
    while (args[i] && args[i] != ' ' && args[i] != '\t' && p < asz - 1) a[p++] = args[i++];
    a[p] = '\0';
    if (p == 0) return 0;

    while (args[i] == ' ' || args[i] == '\t') i++;
    p = 0;
    while (args[i] && args[i] != ' ' && args[i] != '\t' && p < bsz - 1) b[p++] = args[i++];
    b[p] = '\0';
    if (p == 0) return 0;

    return 1;
}

static int cmd_touch(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    char name[MAX_FILENAME];
    parse_one_arg(args, name, sizeof(name));
    if (!name[0]) { show_error("Usage: touch <file>"); return 0; }

    int r = fs_create(name, FILE_TEXT);
    if (r == FS_OK) { show_message("File created", ATTR_SUCCESS); ui_draw(); return 1; }
    if (r == FS_ERR_EXISTS) show_error("File already exists");
    else if (r == FS_ERR_NOSPACE) show_error("No space for file");
    else show_error("touch failed");
    return 0;
}

static int cmd_rm(const char* args, int* mode, int* explorer_sel) {
    return cmd_del(args, mode, explorer_sel);
}

static int cmd_cp(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    char src[MAX_FILENAME], dst[MAX_FILENAME];
    if (!parse_two_args(args, src, sizeof(src), dst, sizeof(dst))) {
        show_error("Usage: cp <source> <dest>");
        return 0;
    }

    struct File* s = fs_find(src);
    if (!s) { show_error("Source file not found"); return 0; }
    if (fs_find(dst)) { show_error("Destination exists"); return 0; }

    int r = fs_create(dst, s->type);
    if (r != FS_OK) { show_error("cp create failed"); return 0; }
    r = fs_write(dst, s->content);
    if (r < 0) { show_error("cp write failed"); return 0; }

    show_message("File copied", ATTR_SUCCESS);
    ui_draw();
    return 1;
}

static int cmd_mv(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    char src[MAX_FILENAME], dst[MAX_FILENAME];
    if (!parse_two_args(args, src, sizeof(src), dst, sizeof(dst))) {
        show_error("Usage: mv <source> <dest>");
        return 0;
    }

    struct File* s = fs_find(src);
    if (!s) { show_error("Source file not found"); return 0; }
    if (fs_find(dst)) { show_error("Destination exists"); return 0; }

    int r = fs_create(dst, s->type);
    if (r != FS_OK) { show_error("mv create failed"); return 0; }
    r = fs_write(dst, s->content);
    if (r < 0) { show_error("mv write failed"); return 0; }
    r = fs_delete(src);
    if (r != FS_OK) { show_error("mv delete failed"); return 0; }

    show_message("File moved", ATTR_SUCCESS);
    ui_draw();
    return 1;
}

static int cmd_echo(const char* args, int* mode, int* explorer_sel) {
    (void)mode; (void)explorer_sel;
    if (!args) { show_message("", ATTR_NORMAL); return 1; }
    show_message(args, ATTR_NORMAL);
    return 1;
}

static int cmd_show_history(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    vga_clear();
    const char* title = "Command History";
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], ATTR_TITLE);

    int first = (history_count > CMD_HISTORY_SIZE) ? history_count - CMD_HISTORY_SIZE : 0;
    int row = 2;
    for (int i = first; i < history_count && row < HEIGHT - 2; ++i) {
        char line[WIDTH];
        int p = 0;
        int_to_dec(line, i + 1);
        while (line[p]) p++;
        if (p < WIDTH - 2) line[p++] = ':';
        if (p < WIDTH - 2) line[p++] = ' ';
        const char* h = cmd_history[i % CMD_HISTORY_SIZE];
        for (int j = 0; h[j] && p < WIDTH - 2; ++j) line[p++] = h[j];
        line[p] = '\0';
        for (int j = 0; line[j] && j < WIDTH - 2; ++j) vga_putcell(1 + j, row, line[j], ATTR_NORMAL);
        row++;
    }

    if (history_count == 0) {
        const char* empty = "(no history)";
        for (int i = 0; empty[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 2, empty[i], ATTR_BORDER);
    }

    const char* footer = "Press any key to continue...";
    for (int i = 0; footer[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, HEIGHT - 1, footer[i], ATTR_PROMPT);
    wait_key();
    ui_draw();
    return 1;
}

static int cmd_uname(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    show_message("NoirOS i386", ATTR_PROMPT);
    return 1;
}

static int cmd_whoami(const char* args, int* mode, int* explorer_sel) {
    (void)args; (void)mode; (void)explorer_sel;
    show_message("root", ATTR_PROMPT);
    return 1;
}

static int cmd_man(const char* args, int* mode, int* explorer_sel) {
    return cmd_help(args, mode, explorer_sel);
}

static int cmd_ll(const char* args, int* mode, int* explorer_sel) {
    return cmd_list(args, mode, explorer_sel);
}

/* ---- Command table (NULL-terminated sentinel) ---- */
static const shell_command_t commands[] = {
    {"help",  "Show available commands",    cmd_help},
    {"ls",    "List files",                 cmd_list},
    {"ll",    "List files",                 cmd_ll},
    {"dir",   "List files",                 cmd_list},
    {"cd",    "Change directory",           cmd_cd},
    {"mkdir", "Make directory",             cmd_mkdir},
    {"rmdir", "Remove empty directory",     cmd_rmdir},
    {"touch", "Create empty text file",     cmd_touch},
    {"new",   "Create file",               cmd_new},
    {"rm",    "Delete file",               cmd_rm},
    {"del",   "Delete file",               cmd_del},
    {"cp",    "Copy file",                  cmd_cp},
    {"mv",    "Move/rename file",           cmd_mv},
    {"echo",  "Print text",                 cmd_echo},
    {"cat",   "View file contents",         cmd_cat},
    {"edit",  "Edit a file",               cmd_edit},
    {"calc",  "Integer calculator",         cmd_calc},
    {"history","Show command history",      cmd_show_history},
    {"uname", "System name",                cmd_uname},
    {"whoami", "Current user",              cmd_whoami},
    {"man",   "Alias for help",             cmd_man},
    {"pwd",   "Print working directory",    cmd_pwd},
    {"snake", "Play snake game",            cmd_snake},
    {"pong",  "Play pong",                  cmd_pong},
    {"dodge", "Dodge falling stars",        cmd_dodge},
    {"catch", "Catch falling coins",        cmd_catch},
    {"kbdtest","Interactive keyboard test", cmd_kbdtest},
    {"restart", "Restart system",           cmd_restart},
    {"reboot",  "Restart system",           cmd_restart},
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
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, STATUS_ROW, ' ', ATTR_NORMAL);
    for (int i = 0; message[i] && i < STATUS_COLS; i++) {
        vga_putcell(1 + i, STATUS_ROW, message[i], ATTR_ERROR);
    }
    wait_key();
}

static void show_message(const char* message, unsigned char color) {
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, STATUS_ROW, ' ', ATTR_NORMAL);
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

static void cmd_panel_write_line(int row, const char* text, u8 attr) {
    for (int x = 1; x < WIDTH - 1; ++x) vga_putcell(x, row, ' ', ATTR_STATUS);
    for (int i = 0; text[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, row, text[i], attr);
}

static void draw_cmd_panel(const char* prompt, const char* input) {
    char line[WIDTH];
    int p = 0;

    draw_box(0, CMD_PANEL_Y, WIDTH, CMD_PANEL_H, " Command ", ATTR_PROMPT, ATTR_BORDER, ATTR_STATUS);
    cmd_panel_write_line(CMD_PANEL_Y + 1, "Enter=run  Esc=cancel  Up/Down=history  Tab=toggle panel", CMD_HINT_ATTR);

    for (int i = 0; prompt[i] && p < WIDTH - 3; ++i) line[p++] = prompt[i];
    for (int i = 0; input[i] && p < WIDTH - 3; ++i) line[p++] = input[i];
    line[p] = '\0';
    cmd_panel_write_line(CMD_PANEL_Y + 2, line, CMD_TEXT_ATTR);

    if (history_count > 0) {
        char hline[WIDTH];
        int hp = 0;
        const char* prefix = "Recent: ";
        const char* h = cmd_history[(history_count - 1) % CMD_HISTORY_SIZE];
        for (int i = 0; prefix[i] && hp < WIDTH - 3; ++i) hline[hp++] = prefix[i];
        for (int i = 0; h[i] && hp < WIDTH - 3; ++i) hline[hp++] = h[i];
        hline[hp] = '\0';
        cmd_panel_write_line(CMD_PANEL_Y + 3, hline, CMD_HINT_ATTR);
    }

    if (history_count > 1) {
        char hline2[WIDTH];
        int hp2 = 0;
        const char* prefix2 = "Prev:   ";
        const char* h2 = cmd_history[(history_count - 2) % CMD_HISTORY_SIZE];
        for (int i = 0; prefix2[i] && hp2 < WIDTH - 3; ++i) hline2[hp2++] = prefix2[i];
        for (int i = 0; h2[i] && hp2 < WIDTH - 3; ++i) hline2[hp2++] = h2[i];
        hline2[hp2] = '\0';
        cmd_panel_write_line(CMD_PANEL_Y + 4, hline2, CMD_HINT_ATTR);
    }
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
    int ipos = 0;
    out[0] = '\0';

    /* hist_pos starts just past the newest entry; UP moves it backwards. */
    int hist_pos = history_count;

    int pk = first_key; /* preloaded key; 0 means none */

    while (1) {
        out[ipos] = '\0';
        draw_cmd_panel(prompt, out);
        vga_flush();

        int ch;
        if (pk) { ch = pk; pk = 0; }
        else ch = wait_key();

        if (ch == '\n' || ch == '\r') {
            out[ipos] = '\0';
            vga_flush();
            return 1;
        }

        if (ch == K_ESC) {
            out[0] = '\0';
            ui_draw();
            vga_flush();
            input_reset_modifiers();
            return 0;
        }

        if (ch == '\b') {
            if (ipos > 0) ipos--;
            continue;
        }

        if (ch == K_TAB) {
            ui_toggle_active_panel();
            ui_draw();
            continue;
        }

        if (ch == K_ARROW_UP) {
            if (history_count == 0) continue;
            /* Oldest reachable logical index in the ring */
            int min_hist = (history_count > CMD_HISTORY_SIZE)
                           ? history_count - CMD_HISTORY_SIZE : 0;
            if (hist_pos > min_hist) hist_pos--;
            const char* h = cmd_history[hist_pos % CMD_HISTORY_SIZE];
            ipos = 0;
            for (int i = 0; h[i] && ipos < outsz - 1; i++) {
                out[ipos++] = h[i];
            }
            out[ipos] = '\0';
            continue;
        }

        if (ch == K_ARROW_DOWN) {
            if (history_count == 0) continue;
            ipos = 0;
            if (hist_pos < history_count) hist_pos++;
            if (hist_pos < history_count) {
                const char* h = cmd_history[hist_pos % CMD_HISTORY_SIZE];
                for (int i = 0; h[i] && ipos < outsz - 1; i++) {
                    out[ipos++] = h[i];
                }
            }
            out[ipos] = '\0';
            /* else hist_pos == history_count → input left blank */
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            if (ipos < outsz - 1) {
                out[ipos++] = (char)ch;
            }
            out[ipos] = '\0';
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

static int is_cmd_token(const char* input, const char* name) {
    int cmd_len = 0;
    while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;
    return (kstrncmp(input, name, cmd_len) == 0 && kstrlen(name) == cmd_len);
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
                    if (kstrncmp(f->name, "pong", 4) == 0) {
                        execute_command("pong", mode, &explorer_sel);
                    } else if (kstrncmp(f->name, "dodge", 5) == 0) {
                        execute_command("dodge", mode, &explorer_sel);
                    } else if (kstrncmp(f->name, "catch", 5) == 0) {
                        execute_command("catch", mode, &explorer_sel);
                    } else {
                        execute_command("snake", mode, &explorer_sel);
                    }
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

    while (*mode == MODE_BROWSER) {
        char input[MAX_CMD_LEN];
        int ok = shell_readline_preloaded("cmd> ", input, sizeof(input), 0);
        if (!ok) break; /* ESC closes command panel */
        if (!input[0]) continue;

        add_to_history(input);
        execute_command(input, mode, &explorer_sel);

        /* Keep cmd open for repeated commands like ls; close on explicit exit. */
        if (is_cmd_token(input, "exit") || is_cmd_token(input, "quit")) break;
        if (*mode != MODE_BROWSER) break;
    }

    ui_draw();
    return explorer_sel;
}
