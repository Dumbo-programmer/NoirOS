#include "../include/noirc.h"
#include "../include/vga.h"
#include "../include/input.h"
#include "../include/fs.h"
#include "../include/util.h"
#include "../include/ui.h"

#define MAX_TOKENS   1024
#define MAX_EXPR     1024
#define MAX_STMTS    512
#define MAX_VARS     128
#define LOOP_GUARD   100000

enum {
    TOK_EOF = 0,
    TOK_ID,
    TOK_NUM,
    TOK_KW_INT,
    TOK_KW_PRINT,
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_WHILE,
    TOK_SYM
};

typedef struct {
    int type;
    char text[32];
    int value;
    char sym0;
    char sym1;
} token_t;

static token_t tokens[MAX_TOKENS];
static int tok_count;
static int tok_pos;

enum {
    EX_NUM = 1,
    EX_VAR,
    EX_UNARY,
    EX_BINARY
};

enum {
    OP_NEG = 1,
    OP_ADDR,
    OP_DEREF,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE
};

typedef struct {
    int kind;
    int op;
    int lhs;
    int rhs;
    int num;
    char name[32];
} expr_t;

static expr_t expr_nodes[MAX_EXPR];
static int expr_count;

enum {
    ST_DECL = 1,
    ST_ASSIGN,
    ST_DEREF_ASSIGN,
    ST_PRINT,
    ST_IF,
    ST_WHILE
};

typedef struct {
    int kind;
    int next;
    char name[32];
    int is_ptr;
    int expr;
    int then_first;
    int else_first;
    int body_first;
} stmt_t;

static stmt_t stmts[MAX_STMTS];
static int stmt_count;

typedef struct {
    int used;
    char name[32];
    int is_ptr;
    int value;
} var_t;

static var_t vars[MAX_VARS];

static int out_row;
static int runtime_errors;

static void noirc_print_line(const char* s, u8 attr) {
    if (out_row >= HEIGHT - 2) return;
    int col = 1;
    while (*s && col < WIDTH - 1) {
        vga_putcell(col++, out_row, *s++, attr);
    }
    out_row++;
}

static void noirc_print_num(int n) {
    char buf[32];
    int_to_dec(buf, n);
    noirc_print_line(buf, ATTR_NOIRC_OUTPUT);
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_alnum(char c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static void emit_token(int type, const char* text, int value, char s0, char s1) {
    if (tok_count >= MAX_TOKENS) return;
    token_t* t = &tokens[tok_count++];
    t->type = type;
    t->value = value;
    t->sym0 = s0;
    t->sym1 = s1;
    if (text) kstrncpy(t->text, text, (int)sizeof(t->text));
    else t->text[0] = '\0';
}

static void lex_source(const char* src) {
    tok_count = 0;
    int i = 0;

    while (src[i] && tok_count < MAX_TOKENS - 1) {
        char c = src[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            i++;
            continue;
        }

        if (c == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') i++;
            continue;
        }

        if (is_alpha(c)) {
            char buf[32];
            int p = 0;
            while (is_alnum(src[i]) && p < (int)sizeof(buf) - 1) {
                buf[p++] = src[i++];
            }
            buf[p] = '\0';

            if (kstrcmp(buf, "int") == 0) emit_token(TOK_KW_INT, buf, 0, 0, 0);
            else if (kstrcmp(buf, "print") == 0) emit_token(TOK_KW_PRINT, buf, 0, 0, 0);
            else if (kstrcmp(buf, "if") == 0) emit_token(TOK_KW_IF, buf, 0, 0, 0);
            else if (kstrcmp(buf, "else") == 0) emit_token(TOK_KW_ELSE, buf, 0, 0, 0);
            else if (kstrcmp(buf, "while") == 0) emit_token(TOK_KW_WHILE, buf, 0, 0, 0);
            else emit_token(TOK_ID, buf, 0, 0, 0);
            continue;
        }

        if (is_digit(c)) {
            int v = 0;
            while (is_digit(src[i])) {
                v = v * 10 + (src[i] - '0');
                i++;
            }
            emit_token(TOK_NUM, 0, v, 0, 0);
            continue;
        }

        if ((c == '=' && src[i + 1] == '=') || (c == '!' && src[i + 1] == '=') ||
            (c == '<' && src[i + 1] == '=') || (c == '>' && src[i + 1] == '=')) {
            emit_token(TOK_SYM, 0, 0, c, src[i + 1]);
            i += 2;
            continue;
        }

        emit_token(TOK_SYM, 0, 0, c, 0);
        i++;
    }

    emit_token(TOK_EOF, 0, 0, 0, 0);
}

static token_t* cur_tok(void) {
    if (tok_pos < 0 || tok_pos >= tok_count) return &tokens[tok_count - 1];
    return &tokens[tok_pos];
}

static int match_sym(char s0, char s1) {
    token_t* t = cur_tok();
    if (t->type != TOK_SYM) return 0;
    if (t->sym0 == s0 && t->sym1 == s1) {
        tok_pos++;
        return 1;
    }
    return 0;
}

static int match_kw(int kw) {
    if (cur_tok()->type == kw) {
        tok_pos++;
        return 1;
    }
    return 0;
}

static int new_expr(void) {
    if (expr_count >= MAX_EXPR) return -1;
    int idx = expr_count++;
    expr_nodes[idx].kind = 0;
    expr_nodes[idx].op = 0;
    expr_nodes[idx].lhs = -1;
    expr_nodes[idx].rhs = -1;
    expr_nodes[idx].num = 0;
    expr_nodes[idx].name[0] = '\0';
    return idx;
}

static int parse_expr(void);

static int parse_primary(void) {
    token_t* t = cur_tok();
    if (t->type == TOK_NUM) {
        int n = new_expr();
        if (n < 0) return -1;
        expr_nodes[n].kind = EX_NUM;
        expr_nodes[n].num = t->value;
        tok_pos++;
        return n;
    }

    if (t->type == TOK_ID) {
        int n = new_expr();
        if (n < 0) return -1;
        expr_nodes[n].kind = EX_VAR;
        kstrncpy(expr_nodes[n].name, t->text, (int)sizeof(expr_nodes[n].name));
        tok_pos++;
        return n;
    }

    if (match_sym('(', 0)) {
        int e = parse_expr();
        match_sym(')', 0);
        return e;
    }

    return -1;
}

static int parse_unary(void) {
    if (match_sym('-', 0)) {
        int rhs = parse_unary();
        int n = new_expr();
        if (n < 0) return -1;
        expr_nodes[n].kind = EX_UNARY;
        expr_nodes[n].op = OP_NEG;
        expr_nodes[n].rhs = rhs;
        return n;
    }
    if (match_sym('&', 0)) {
        int rhs = parse_unary();
        int n = new_expr();
        if (n < 0) return -1;
        expr_nodes[n].kind = EX_UNARY;
        expr_nodes[n].op = OP_ADDR;
        expr_nodes[n].rhs = rhs;
        return n;
    }
    if (match_sym('*', 0)) {
        int rhs = parse_unary();
        int n = new_expr();
        if (n < 0) return -1;
        expr_nodes[n].kind = EX_UNARY;
        expr_nodes[n].op = OP_DEREF;
        expr_nodes[n].rhs = rhs;
        return n;
    }
    return parse_primary();
}

static int parse_factor(void) {
    int lhs = parse_unary();
    while (1) {
        if (match_sym('*', 0)) {
            int rhs = parse_unary();
            int n = new_expr();
            if (n < 0) return lhs;
            expr_nodes[n].kind = EX_BINARY;
            expr_nodes[n].op = OP_MUL;
            expr_nodes[n].lhs = lhs;
            expr_nodes[n].rhs = rhs;
            lhs = n;
            continue;
        }
        if (match_sym('/', 0)) {
            int rhs = parse_unary();
            int n = new_expr();
            if (n < 0) return lhs;
            expr_nodes[n].kind = EX_BINARY;
            expr_nodes[n].op = OP_DIV;
            expr_nodes[n].lhs = lhs;
            expr_nodes[n].rhs = rhs;
            lhs = n;
            continue;
        }
        break;
    }
    return lhs;
}

static int parse_term(void) {
    int lhs = parse_factor();
    while (1) {
        if (match_sym('+', 0)) {
            int rhs = parse_factor();
            int n = new_expr();
            if (n < 0) return lhs;
            expr_nodes[n].kind = EX_BINARY;
            expr_nodes[n].op = OP_ADD;
            expr_nodes[n].lhs = lhs;
            expr_nodes[n].rhs = rhs;
            lhs = n;
            continue;
        }
        if (match_sym('-', 0)) {
            int rhs = parse_factor();
            int n = new_expr();
            if (n < 0) return lhs;
            expr_nodes[n].kind = EX_BINARY;
            expr_nodes[n].op = OP_SUB;
            expr_nodes[n].lhs = lhs;
            expr_nodes[n].rhs = rhs;
            lhs = n;
            continue;
        }
        break;
    }
    return lhs;
}

static int parse_cmp(void) {
    int lhs = parse_term();
    while (1) {
        int op = 0;
        if (match_sym('<', '=')) op = OP_LE;
        else if (match_sym('>', '=')) op = OP_GE;
        else if (match_sym('<', 0)) op = OP_LT;
        else if (match_sym('>', 0)) op = OP_GT;
        else break;

        int rhs = parse_term();
        int n = new_expr();
        if (n < 0) return lhs;
        expr_nodes[n].kind = EX_BINARY;
        expr_nodes[n].op = op;
        expr_nodes[n].lhs = lhs;
        expr_nodes[n].rhs = rhs;
        lhs = n;
    }
    return lhs;
}

static int parse_expr(void) {
    int lhs = parse_cmp();
    while (1) {
        int op = 0;
        if (match_sym('=', '=')) op = OP_EQ;
        else if (match_sym('!', '=')) op = OP_NE;
        else break;

        int rhs = parse_cmp();
        int n = new_expr();
        if (n < 0) return lhs;
        expr_nodes[n].kind = EX_BINARY;
        expr_nodes[n].op = op;
        expr_nodes[n].lhs = lhs;
        expr_nodes[n].rhs = rhs;
        lhs = n;
    }
    return lhs;
}

static int new_stmt(void) {
    if (stmt_count >= MAX_STMTS) return -1;
    int idx = stmt_count++;
    stmts[idx].kind = 0;
    stmts[idx].next = -1;
    stmts[idx].name[0] = '\0';
    stmts[idx].is_ptr = 0;
    stmts[idx].expr = -1;
    stmts[idx].then_first = -1;
    stmts[idx].else_first = -1;
    stmts[idx].body_first = -1;
    return idx;
}

static int parse_statement(void);

static int parse_block(void) {
    if (match_sym('{', 0)) {
        int first = -1;
        int last = -1;
        while (!match_sym('}', 0) && cur_tok()->type != TOK_EOF) {
            int s = parse_statement();
            if (s < 0) break;
            if (first < 0) first = s;
            if (last >= 0) stmts[last].next = s;
            last = s;
        }
        return first;
    }
    return parse_statement();
}

static int parse_statement(void) {
    if (match_kw(TOK_KW_INT)) {
        int is_ptr = match_sym('*', 0);
        token_t* id = cur_tok();
        if (id->type != TOK_ID) return -1;

        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_DECL;
        stmts[s].is_ptr = is_ptr;
        kstrncpy(stmts[s].name, id->text, (int)sizeof(stmts[s].name));
        tok_pos++;

        if (match_sym('=', 0)) stmts[s].expr = parse_expr();
        match_sym(';', 0);
        return s;
    }

    if (match_kw(TOK_KW_PRINT)) {
        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_PRINT;
        match_sym('(', 0);
        stmts[s].expr = parse_expr();
        match_sym(')', 0);
        match_sym(';', 0);
        return s;
    }

    if (match_kw(TOK_KW_IF)) {
        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_IF;
        match_sym('(', 0);
        stmts[s].expr = parse_expr();
        match_sym(')', 0);
        stmts[s].then_first = parse_block();
        if (match_kw(TOK_KW_ELSE)) stmts[s].else_first = parse_block();
        return s;
    }

    if (match_kw(TOK_KW_WHILE)) {
        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_WHILE;
        match_sym('(', 0);
        stmts[s].expr = parse_expr();
        match_sym(')', 0);
        stmts[s].body_first = parse_block();
        return s;
    }

    if (match_sym('*', 0)) {
        token_t* id = cur_tok();
        if (id->type != TOK_ID) return -1;

        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_DEREF_ASSIGN;
        kstrncpy(stmts[s].name, id->text, (int)sizeof(stmts[s].name));
        tok_pos++;
        match_sym('=', 0);
        stmts[s].expr = parse_expr();
        match_sym(';', 0);
        return s;
    }

    if (cur_tok()->type == TOK_ID) {
        int s = new_stmt();
        if (s < 0) return -1;
        stmts[s].kind = ST_ASSIGN;
        kstrncpy(stmts[s].name, cur_tok()->text, (int)sizeof(stmts[s].name));
        tok_pos++;
        match_sym('=', 0);
        stmts[s].expr = parse_expr();
        match_sym(';', 0);
        return s;
    }

    /* Recovery: consume one token so parser does not lock up on bad input. */
    if (cur_tok()->type != TOK_EOF) tok_pos++;
    return -1;
}

static int parse_program(void) {
    tok_pos = 0;
    expr_count = 0;
    stmt_count = 0;

    int first = -1;
    int last = -1;
    while (cur_tok()->type != TOK_EOF) {
        int s = parse_statement();
        if (s < 0) continue;
        if (first < 0) first = s;
        if (last >= 0) stmts[last].next = s;
        last = s;
    }
    return first;
}

typedef struct {
    int is_ptr;
    int v;
} value_t;

static int find_var(const char* name) {
    for (int i = 0; i < MAX_VARS; ++i) {
        if (vars[i].used && kstrcmp(vars[i].name, name) == 0) return i;
    }
    return -1;
}

static int create_var(const char* name, int is_ptr) {
    int existing = find_var(name);
    if (existing >= 0) {
        vars[existing].is_ptr = is_ptr;
        vars[existing].value = is_ptr ? -1 : 0;
        return existing;
    }

    for (int i = 0; i < MAX_VARS; ++i) {
        if (!vars[i].used) {
            vars[i].used = 1;
            vars[i].is_ptr = is_ptr;
            vars[i].value = is_ptr ? -1 : 0;
            kstrncpy(vars[i].name, name, (int)sizeof(vars[i].name));
            return i;
        }
    }

    runtime_errors++;
    noirc_print_line("[noirc] variable limit reached", ATTR_NOIRC_ERROR);
    return -1;
}

static value_t make_int(int v) {
    value_t x;
    x.is_ptr = 0;
    x.v = v;
    return x;
}

static value_t make_ptr(int idx) {
    value_t x;
    x.is_ptr = 1;
    x.v = idx;
    return x;
}

static int value_as_int(value_t v) {
    if (v.is_ptr) return v.v;
    return v.v;
}

static value_t eval_expr(int eidx) {
    if (eidx < 0 || eidx >= expr_count) return make_int(0);

    expr_t* e = &expr_nodes[eidx];

    if (e->kind == EX_NUM) return make_int(e->num);

    if (e->kind == EX_VAR) {
        int vi = find_var(e->name);
        if (vi < 0) return make_int(0);
        if (vars[vi].is_ptr) return make_ptr(vars[vi].value);
        return make_int(vars[vi].value);
    }

    if (e->kind == EX_UNARY) {
        if (e->op == OP_NEG) {
            value_t rhs = eval_expr(e->rhs);
            return make_int(-value_as_int(rhs));
        }
        if (e->op == OP_ADDR) {
            if (e->rhs >= 0 && e->rhs < expr_count && expr_nodes[e->rhs].kind == EX_VAR) {
                int vi = find_var(expr_nodes[e->rhs].name);
                if (vi >= 0) return make_ptr(vi);
            }
            runtime_errors++;
            noirc_print_line("[noirc] '&' expects a variable", ATTR_NOIRC_ERROR);
            return make_ptr(-1);
        }
        if (e->op == OP_DEREF) {
            value_t rhs = eval_expr(e->rhs);
            if (!rhs.is_ptr || rhs.v < 0 || rhs.v >= MAX_VARS || !vars[rhs.v].used) {
                runtime_errors++;
                noirc_print_line("[noirc] invalid pointer dereference", ATTR_NOIRC_ERROR);
                return make_int(0);
            }
            return make_int(vars[rhs.v].value);
        }
    }

    if (e->kind == EX_BINARY) {
        int l = value_as_int(eval_expr(e->lhs));
        int r = value_as_int(eval_expr(e->rhs));

        if (e->op == OP_ADD) return make_int(l + r);
        if (e->op == OP_SUB) return make_int(l - r);
        if (e->op == OP_MUL) return make_int(l * r);
        if (e->op == OP_DIV) {
            if (r == 0) {
                runtime_errors++;
                noirc_print_line("[noirc] division by zero", ATTR_NOIRC_ERROR);
                return make_int(0);
            }
            return make_int(l / r);
        }
        if (e->op == OP_EQ) return make_int(l == r);
        if (e->op == OP_NE) return make_int(l != r);
        if (e->op == OP_LT) return make_int(l < r);
        if (e->op == OP_GT) return make_int(l > r);
        if (e->op == OP_LE) return make_int(l <= r);
        if (e->op == OP_GE) return make_int(l >= r);
    }

    return make_int(0);
}

static void exec_chain(int first);

static void exec_stmt(int sidx) {
    if (sidx < 0 || sidx >= stmt_count) return;
    stmt_t* s = &stmts[sidx];

    if (s->kind == ST_DECL) {
        int vi = create_var(s->name, s->is_ptr);
        if (vi < 0) return;

        if (s->expr >= 0) {
            value_t v = eval_expr(s->expr);
            if (vars[vi].is_ptr) {
                vars[vi].value = v.is_ptr ? v.v : -1;
            } else {
                vars[vi].value = value_as_int(v);
            }
        }
        return;
    }

    if (s->kind == ST_ASSIGN) {
        int vi = find_var(s->name);
        if (vi < 0) {
            runtime_errors++;
            noirc_print_line("[noirc] assign to unknown variable", ATTR_NOIRC_ERROR);
            return;
        }
        value_t v = eval_expr(s->expr);
        if (vars[vi].is_ptr) vars[vi].value = v.is_ptr ? v.v : -1;
        else vars[vi].value = value_as_int(v);
        return;
    }

    if (s->kind == ST_DEREF_ASSIGN) {
        int p = find_var(s->name);
        if (p < 0 || !vars[p].is_ptr) {
            runtime_errors++;
            noirc_print_line("[noirc] '*' assignment needs pointer variable", ATTR_NOIRC_ERROR);
            return;
        }
        int target = vars[p].value;
        if (target < 0 || target >= MAX_VARS || !vars[target].used) {
            runtime_errors++;
            noirc_print_line("[noirc] pointer is null/invalid", ATTR_NOIRC_ERROR);
            return;
        }
        vars[target].value = value_as_int(eval_expr(s->expr));
        return;
    }

    if (s->kind == ST_PRINT) {
        value_t v = eval_expr(s->expr);
        noirc_print_num(value_as_int(v));
        return;
    }

    if (s->kind == ST_IF) {
        int cond = value_as_int(eval_expr(s->expr));
        if (cond) exec_chain(s->then_first);
        else exec_chain(s->else_first);
        return;
    }

    if (s->kind == ST_WHILE) {
        int guard = LOOP_GUARD;
        while (guard-- > 0) {
            int cond = value_as_int(eval_expr(s->expr));
            if (!cond) break;
            exec_chain(s->body_first);
        }
        if (guard <= 0) {
            runtime_errors++;
            noirc_print_line("[noirc] loop guard triggered", ATTR_NOIRC_ERROR);
        }
        return;
    }
}

static void exec_chain(int first) {
    int s = first;
    while (s >= 0 && s < stmt_count) {
        int next = stmts[s].next;
        exec_stmt(s);
        s = next;
    }
}

static void reset_runtime(void) {
    for (int i = 0; i < MAX_VARS; ++i) {
        vars[i].used = 0;
        vars[i].name[0] = '\0';
        vars[i].is_ptr = 0;
        vars[i].value = 0;
    }
    runtime_errors = 0;
}

static void noirc_append_str(char* buf, int* p, const char* s, int max) {
    while (*s && *p < max - 1) buf[(*p)++] = *s++;
    buf[*p] = '\0';
}

void noirc_run(struct File* f) {
    if (!f) return;
    vga_clear();

    char title[80];
    int tp = 0;
    noirc_append_str(title, &tp, "Running NoirC AST: ", (int)sizeof(title));
    noirc_append_str(title, &tp, f->name, (int)sizeof(title));

    for (int x = 0; x < WIDTH; ++x) vga_putcell(x, 0, ' ', VGA_ATTR(COL_WHITE, COL_BLACK));
    for (int i = 0; title[i] && i < WIDTH - 2; ++i) vga_putcell(1 + i, 0, title[i], VGA_ATTR(COL_WHITE, COL_BLACK));

    out_row = 2;
    noirc_print_line("--- Execution Output ---", ATTR_NOIRC_PROMPT);

    reset_runtime();
    lex_source(f->content);
    int program = parse_program();

    if (program >= 0) exec_chain(program);
    else noirc_print_line("[noirc] parse failed", ATTR_NOIRC_ERROR);

    if (!runtime_errors) noirc_print_line("[noirc] finished successfully", ATTR_NOIRC_OUTPUT);

    const char* footer = "Press any key to return";
    for (int i = 0; footer[i] && i < WIDTH - 2; ++i)
        vga_putcell(1 + i, HEIGHT - 2, footer[i], VGA_ATTR(COL_YELLOW, COL_BLACK));

    wait_key();
    ui_draw();
}
