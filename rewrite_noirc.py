with open('src/noirc.c', 'w') as f:
    f.write('''#include "../include/noirc.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/util.h"
#include "../include/ui.h"

/* Minimal Noir C interpreter
 * Supports: single letter variables (a-z)
 * Arithmetic: +, -, *, /
 * Statements: var = expr;, print(expr);, while(expr) { ... }, if(expr) { ... }
 */

static const char* src;
static int pos = 0;
static int vars[26] = {0};
static int out_row = 2; // lines for output

// Helper to print strings to the screen
static void noirc_print_str(const char* s) {
    if (out_row >= HEIGHT - 3) {
        // scroll? For simplicity, we just stop printing
        return;
    }
    int col = 1;
    while (*s && col < WIDTH - 2) {
        vga_putcell(col++, out_row, *s++, VGA_ATTR(COL_CYAN, COL_BLACK));
    }
    out_row++;
}

static void noirc_print_num(int n) {
    char buf[16];
    int p = 0;
    if (n == 0) { buf[p++] = '0'; }
    else {
        if (n < 0) { buf[p++] = '-'; n = -n; }
        int t = n, r = 0;
        while (t) { t /= 10; r++; }
        int rp = p + r;
        buf[rp] = 0;
        while (n) { buf[--rp] = (n % 10) + '0'; n /= 10; }
        p += r;
    }
    buf[p] = 0;
    noirc_print_str(buf);
}

// Lexer
static void skip_whitespace() {
    while (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r') {
        pos++;
    }
}

static int match(const char* m) {
    skip_whitespace();
    int i = 0;
    while (m[i]) {
        if (src[pos + i] != m[i]) return 0;
        i++;
    }
    pos += i;
    return 1;
}

static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_digit(char c) { return c >= '0' && c <= '9'; }

// Parser decls
static int parse_expr();
static void parse_statement();
static void parse_block();

static int parse_factor() {
    skip_whitespace();
    if (match("(")) {
        int v = parse_expr();
        match(")");
        return v;
    }
    int v = 0;
    if (is_digit(src[pos])) {
        while (is_digit(src[pos])) {
            v = v * 10 + (src[pos] - '0');
            pos++;
        }
        return v;
    }
    if (is_alpha(src[pos])) {
        char name = src[pos];
        pos++;
        if (name >= 'a' && name <= 'z') return vars[name - 'a'];
        else return 0;
    }
    return 0; // Error
}

static int parse_term() {
    int v = parse_factor();
    skip_whitespace();
    while (src[pos] == '*' || src[pos] == '/') {
        char op = src[pos++];
        int r = parse_factor();
        if (op == '*') v *= r;
        else if (r != 0) v /= r;
    }
    return v;
}

static int parse_expr() {
    int v = parse_term();
    skip_whitespace();
    while (src[pos] == '+' || src[pos] == '-' || src[pos] == '>' || src[pos] == '<' || src[pos] == '=') {
        if (match("==")) { v = (v == parse_term()); continue; }
        char op = src[pos++];
        int r = parse_term();
        if (op == '+') v += r;
        else if (op == '-') v -= r;
        else if (op == '>') v = (v > r);
        else if (op == '<') v = (v < r);
    }
    return v;
}

static void parse_statement() {
    skip_whitespace();
    if (match("print(")) {
        int v = parse_expr();
        match(")");
        match(";");
        noirc_print_num(v);
        return;
    }
    if (match("if(")) {
        int cond = parse_expr();
        match(")");
        if (cond) {
            parse_block();
        } else {
            // Skip block
            int brackets = 1;
            while (src[pos] != '{') pos++;
            pos++;
            while (brackets > 0 && src[pos]) {
                if (src[pos] == '{') brackets++;
                if (src[pos] == '}') brackets--;
                pos++;
            }
        }
        return;
    }
    if (match("while(")) {
        int cond_pos = pos;
        while (1) {
            pos = cond_pos;
            int cond = parse_expr();
            match(")");
            if (cond) {
                parse_block();
            } else {
                int brackets = 1;
                while (src[pos] != '{') pos++;
                pos++;
                while (brackets > 0 && src[pos]) {
                    if (src[pos] == '{') brackets++;
                    if (src[pos] == '}') brackets--;
                    pos++;
                }
                break;
            }
        }
        return;
    }
    // Assignment
    if (match("int ")) {
        // Just skip "int " and fall through to assignment if it's "int a = 5;"
        // Real C parsing is way harder, we assume "int a = ..." or "a = ..."
    }
    
    char name = src[pos];
    if (is_alpha(name)) {
        pos++;
        skip_whitespace();
        if (match("=")) {
            int v = parse_expr();
            match(";");
            if (name >= 'a' && name <= 'z') vars[name - 'a'] = v;
        }
    }
}

static void parse_block() {
    skip_whitespace();
    if (match("{")) {
        while (!match("}") && src[pos]) {
            parse_statement();
            skip_whitespace();
        }
    } else {
        parse_statement();
    }
}

static void noirc_append_str(char *buf, int *p, const char *s, int max) {
    while (*s && *p < max - 1) buf[(*p)++] = *s++;
    buf[*p] = '\0';
}

void noirc_run(struct File* f) {
    if (!f) return;
    vga_clear();

    /* Header */
    char title[80]; int tp = 0;
    noirc_append_str(title, &tp, "Running NoirC: ", sizeof(title));
    noirc_append_str(title, &tp, f->name, sizeof(title));
    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', VGA_ATTR(COL_WHITE, COL_BLACK));
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], VGA_ATTR(COL_WHITE, COL_BLACK));

    /* Initialize runtime */
    for(int i=0; i<26; i++) vars[i] = 0;
    src = f->content;
    pos = 0;
    out_row = 2;
    noirc_print_str("--- Execution Output ---");

    /* Parse all statements */
    while (src[pos]) {
        skip_whitespace();
        if (!src[pos]) break;
        parse_statement();
    }

    /* Execution End */
    const char* out = "--- Execution finished. Press any key. ---";
    for (int i = 0; out[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, HEIGHT - 2, out[i], VGA_ATTR(COL_YELLOW, COL_BLACK));

    /* Wait for key and return to UI */
    wait_key();
    ui_draw();
}
''')
