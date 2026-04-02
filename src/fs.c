#include "../include/fs.h"
#include "../include/util.h"
#include "../include/ata.h"
#include "../include/fat.h"

/* -------- Internal storage -------- */
static struct Dir s_root;
static struct Dir* s_cwd = &s_root;

#define DIR_POOL_SIZE 64
static struct Dir pool[DIR_POOL_SIZE];
static int pool_used[DIR_POOL_SIZE];

static int persistence_ready = 0;
static const char SNAPSHOT_83[11] = {'N','O','I','R','O','S',' ',' ','D','A','T'};

#define SNAPSHOT_MAX (64 * 1024)
static u8 snapshot_buf[SNAPSHOT_MAX];

/* -------- Serialization helpers -------- */
typedef struct {
    u8* buf;
    int pos;
    int cap;
    int ok;
} ser_ctx_t;

typedef struct {
    const u8* buf;
    int pos;
    int len;
    int ok;
} de_ctx_t;

static void ser_u8(ser_ctx_t* c, u8 v) {
    if (!c->ok || c->pos >= c->cap) { c->ok = 0; return; }
    c->buf[c->pos++] = v;
}

static void ser_u16(ser_ctx_t* c, u16 v) {
    ser_u8(c, (u8)(v & 0xFF));
    ser_u8(c, (u8)((v >> 8) & 0xFF));
}

static void ser_bytes(ser_ctx_t* c, const char* s, int n) {
    for (int i = 0; i < n; ++i) ser_u8(c, (u8)s[i]);
}

static u8 de_u8(de_ctx_t* c) {
    if (!c->ok || c->pos >= c->len) { c->ok = 0; return 0; }
    return c->buf[c->pos++];
}

static u16 de_u16(de_ctx_t* c) {
    u16 lo = de_u8(c);
    u16 hi = de_u8(c);
    return (u16)(lo | (hi << 8));
}

static void de_bytes(de_ctx_t* c, char* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = (char)de_u8(c);
}

static struct Dir* alloc_dir(void) {
    for (int i = 0; i < DIR_POOL_SIZE; ++i) {
        if (!pool_used[i]) {
            pool_used[i] = 1;
            return &pool[i];
        }
    }
    return 0;
}

static void clear_dir(struct Dir* d) {
    if (!d) return;
    for (int i = 0; i < MAX_DIRS_PER_DIR; ++i) d->subdirs[i] = 0;
    d->subdir_count = 0;
    d->file_count = 0;
}

static void serialize_dir(ser_ctx_t* c, const struct Dir* d) {
    int nlen = kstrlen(d->name);
    if (nlen > 31) nlen = 31;

    ser_u8(c, (u8)nlen);
    ser_bytes(c, d->name, nlen);
    ser_u8(c, (u8)d->file_count);
    ser_u8(c, (u8)d->subdir_count);

    for (int i = 0; i < d->file_count; ++i) {
        const struct File* f = &d->files[i];
        int flen = kstrlen(f->name);
        if (flen > 31) flen = 31;

        ser_u8(c, (u8)flen);
        ser_bytes(c, f->name, flen);
        ser_u8(c, f->type);
        ser_u8(c, f->readonly);

        int data_len = f->length;
        if (data_len < 0) data_len = 0;
        if (data_len > MAX_CONTENT - 1) data_len = MAX_CONTENT - 1;
        ser_u16(c, (u16)data_len);
        ser_bytes(c, f->content, data_len);
    }

    for (int i = 0; i < d->subdir_count; ++i) {
        serialize_dir(c, d->subdirs[i]);
    }
}

static int deserialize_dir(de_ctx_t* c, struct Dir* d, struct Dir* parent) {
    clear_dir(d);
    d->parent = parent;

    int nlen = (int)de_u8(c);
    if (!c->ok || nlen <= 0 || nlen >= MAX_FILENAME) return 0;
    de_bytes(c, d->name, nlen);
    d->name[nlen] = '\0';

    int fcount = (int)de_u8(c);
    int dcount = (int)de_u8(c);
    if (!c->ok || fcount > MAX_FILES_PER_DIR || dcount > MAX_DIRS_PER_DIR) return 0;

    for (int i = 0; i < fcount; ++i) {
        struct File* f = &d->files[d->file_count++];
        int flen = (int)de_u8(c);
        if (!c->ok || flen <= 0 || flen >= MAX_FILENAME) return 0;
        de_bytes(c, f->name, flen);
        f->name[flen] = '\0';

        f->type = de_u8(c);
        f->readonly = de_u8(c);

        int data_len = (int)de_u16(c);
        if (!c->ok || data_len < 0 || data_len >= MAX_CONTENT) return 0;
        de_bytes(c, f->content, data_len);
        f->content[data_len] = '\0';
        f->length = data_len;
    }

    for (int i = 0; i < dcount; ++i) {
        struct Dir* child = alloc_dir();
        if (!child) return 0;
        d->subdirs[d->subdir_count++] = child;
        if (!deserialize_dir(c, child, d)) return 0;
    }

    return c->ok;
}

static void fs_save_snapshot(void) {
    if (!persistence_ready) return;

    ser_ctx_t c;
    c.buf = snapshot_buf;
    c.pos = 0;
    c.cap = SNAPSHOT_MAX;
    c.ok = 1;

    ser_u8(&c, 'N'); ser_u8(&c, 'F'); ser_u8(&c, 'S'); ser_u8(&c, '1');
    serialize_dir(&c, &s_root);
    if (!c.ok) return;

    fat_write_root_file_83(SNAPSHOT_83, snapshot_buf, (u32)c.pos);
}

static int fs_load_snapshot(void) {
    if (!persistence_ready) return 0;

    u32 out_len = 0;
    int r = fat_read_root_file_83(SNAPSHOT_83, snapshot_buf, SNAPSHOT_MAX, &out_len);
    if (r != FAT_OK || out_len < 8) return 0;

    de_ctx_t c;
    c.buf = snapshot_buf;
    c.pos = 0;
    c.len = (int)out_len;
    c.ok = 1;

    if (de_u8(&c) != 'N' || de_u8(&c) != 'F' || de_u8(&c) != 'S' || de_u8(&c) != '1') return 0;

    for (int i = 0; i < DIR_POOL_SIZE; ++i) pool_used[i] = 0;
    if (!deserialize_dir(&c, &s_root, 0)) return 0;

    s_cwd = &s_root;
    return 1;
}

/* -------- Internal helpers -------- */
static int name_invalid(const char* n) {
    if (!n) return 1;
    int L = kstrlen(n);
    if (L <= 0 || L >= MAX_FILENAME) return 1;
    for (int i = 0; i < L; ++i) {
        if (n[i] == '/' || n[i] == '\\') return 1;
    }
    return 0;
}

static struct Dir* dir_find_child(struct Dir* d, const char* name) {
    if (!d || !name) return 0;
    for (int i = 0; i < d->subdir_count; ++i)
        if (kstrcmp(d->subdirs[i]->name, name) == 0) return d->subdirs[i];
    return 0;
}

static int dir_is_empty(struct Dir* d) {
    return d->file_count == 0 && d->subdir_count == 0;
}

static int pool_index_of(struct Dir* d) {
    for (int i = 0; i < DIR_POOL_SIZE; ++i)
        if (&pool[i] == d) return i;
    return -1;
}

static void fs_make_default_tree(void) {
    clear_dir(&s_root);
    kstrncpy(s_root.name, "/", MAX_FILENAME);
    s_root.parent = 0;
    s_cwd = &s_root;

    struct File* f;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "README.txt", MAX_FILENAME);
    kstrncpy(f->content,
             "NoirOS\n"
             "Use arrows/W-S to navigate, Enter for cmd.\n"
             "Commands: ls, cd, mkdir, rmdir, touch/new, del, edit <file>, pwd\n",
             MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_TEXT;
    f->readonly = 1;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "help.txt", MAX_FILENAME);
    kstrncpy(f->content,
             "Help:\n"
             " ls                 - list current folder\n"
             " cd <dir>|..|/      - change directory\n"
             " mkdir <n>       - make directory\n"
             " rmdir <n>       - remove EMPTY directory\n"
             " new <n> <type>  - create file (type: 0 text, 1 exe, 2 game)\n"
             " del <n>         - delete file\n"
             " edit <file>        - open editor\n"
             " pwd                - show current path\n",
             MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_TEXT;
    f->readonly = 1;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "notes.md", MAX_FILENAME);
    kstrncpy(f->content, "Editable notes.md\nTry: mkdir docs; cd docs; new todo.txt 0\n", MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_TEXT;
    f->readonly = 0;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "index.html", MAX_FILENAME);
    kstrncpy(f->content, "<html><body><h1>NoirOS</h1><p>Welcome to NoirOS</p></body></html>", MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_HTML;
    f->readonly = 1;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "readme.md", MAX_FILENAME);
    kstrncpy(f->content, "# NoirOS\nThis is a sample Markdown file.\n", MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_MARKDOWN;
    f->readonly = 1;

    f = &s_root.files[s_root.file_count++];
    kstrncpy(f->name, "hello.nc", MAX_FILENAME);
    kstrncpy(f->content,
             "int x = 5;\n"
             "int *p = &x;\n"
             "while (x > 0) {\n"
             "  print(x);\n"
             "  *p = *p - 1;\n"
             "}\n",
             MAX_CONTENT);
    f->length = kstrlen(f->content);
    f->type = FILE_NOIRC;
    f->readonly = 0;

    if (s_root.subdir_count < MAX_DIRS_PER_DIR) {
        struct Dir* docs = alloc_dir();
        if (docs) {
            clear_dir(docs);
            docs->parent = &s_root;
            kstrncpy(docs->name, "docs", MAX_FILENAME);
            s_root.subdirs[s_root.subdir_count++] = docs;

            struct File* df = &docs->files[docs->file_count++];
            kstrncpy(df->name, "guide.txt", MAX_FILENAME);
            kstrncpy(df->content, "Welcome to /docs\n", MAX_CONTENT);
            df->length = kstrlen(df->content);
            df->type = FILE_TEXT;
            df->readonly = 0;
        }
    }
}

void init_filesystem(void) {
    for (int i = 0; i < DIR_POOL_SIZE; ++i) pool_used[i] = 0;

    fs_make_default_tree();

    if (ata_init() == 0) {
        /* Boot must stay responsive. If no valid FAT volume exists,
         * keep running with in-memory FS and skip expensive first-boot
         * full disk formatting. */
        if (fat_mount() == FAT_OK) {
            persistence_ready = 1;
            if (!fs_load_snapshot()) fs_save_snapshot();
        }
    }
}

struct Dir* fs_root(void) { return &s_root; }
struct Dir* fs_cwd(void)  { return s_cwd; }

void fs_pwd(char* out, int out_len) {
    if (!out || out_len <= 0) return;

    const int TMP = 128;
    char stack[TMP];
    int sp = 0;
    struct Dir* d = s_cwd;

    if (d == &s_root) {
        kstrncpy(out, "/", out_len);
        return;
    }

    while (d && d != &s_root && sp < TMP - 1) {
        int L = kstrlen(d->name);
        if (sp + L + 1 >= TMP) break;
        stack[sp++] = '/';
        for (int i = L - 1; i >= 0; --i) stack[sp++] = d->name[i];
        d = d->parent;
    }

    int w = 0;
    for (int i = sp - 1; i >= 0 && w < out_len - 1; --i) out[w++] = stack[i];
    out[w] = 0;
    if (w == 0) kstrncpy(out, "/", out_len);
}

int fs_mkdir(const char* name) {
    if (name_invalid(name)) return FS_ERR_INVALID;
    if (s_cwd->subdir_count >= MAX_DIRS_PER_DIR) return FS_ERR_NOSPACE;
    if (dir_find_child(s_cwd, name)) return FS_ERR_EXISTS;

    struct Dir* nd = alloc_dir();
    if (!nd) return FS_ERR_NOSPACE;

    clear_dir(nd);
    nd->parent = s_cwd;
    kstrncpy(nd->name, name, MAX_FILENAME);
    s_cwd->subdirs[s_cwd->subdir_count++] = nd;

    fs_save_snapshot();
    return FS_OK;
}

int fs_chdir(const char* name) {
    if (!name) return FS_ERR_INVALID;
    if (kstrcmp(name, "/") == 0) { s_cwd = &s_root; return FS_OK; }
    if (kstrcmp(name, "..") == 0) {
        if (s_cwd->parent) s_cwd = s_cwd->parent;
        return FS_OK;
    }

    struct Dir* d = dir_find_child(s_cwd, name);
    if (!d) return FS_ERR_NOTFOUND;
    s_cwd = d;
    return FS_OK;
}

int fs_rmdir(const char* name) {
    if (name_invalid(name)) return FS_ERR_INVALID;

    for (int i = 0; i < s_cwd->subdir_count; ++i) {
        struct Dir* d = s_cwd->subdirs[i];
        if (kstrcmp(d->name, name) == 0) {
            if (!dir_is_empty(d)) return FS_ERR_DIRNOTEMPTY;

            int pi = pool_index_of(d);
            if (pi >= 0) pool_used[pi] = 0;

            for (int j = i; j < s_cwd->subdir_count - 1; ++j)
                s_cwd->subdirs[j] = s_cwd->subdirs[j + 1];
            s_cwd->subdirs[s_cwd->subdir_count - 1] = 0;
            s_cwd->subdir_count--;

            fs_save_snapshot();
            return FS_OK;
        }
    }

    return FS_ERR_NOTFOUND;
}

int fs_dir_count(void) { return s_cwd->subdir_count; }

struct Dir* fs_dir_get(int idx) {
    if (idx < 0 || idx >= s_cwd->subdir_count) return 0;
    return s_cwd->subdirs[idx];
}

struct Dir* fs_find_dir(const char* name) { return dir_find_child(s_cwd, name); }

int fs_count(void) { return s_cwd->file_count; }

struct File* fs_get(int idx) {
    if (idx < 0 || idx >= s_cwd->file_count) return 0;
    return &s_cwd->files[idx];
}

struct File* fs_find(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < s_cwd->file_count; ++i) {
        if (kstrcmp(s_cwd->files[i].name, name) == 0) return &s_cwd->files[i];
    }
    return 0;
}

int fs_create(const char* name, u8 type) {
    if (name_invalid(name)) return FS_ERR_INVALID;
    if (s_cwd->file_count >= MAX_FILES_PER_DIR) return FS_ERR_NOSPACE;
    if (fs_find(name)) return FS_ERR_EXISTS;

    struct File* f = &s_cwd->files[s_cwd->file_count++];
    kstrncpy(f->name, name, MAX_FILENAME);
    f->content[0] = '\0';
    f->length = 0;
    f->type = type;
    f->readonly = 0;

    fs_save_snapshot();
    return FS_OK;
}

int fs_delete(const char* name) {
    if (!name) return FS_ERR_INVALID;

    int idx = -1;
    for (int i = 0; i < s_cwd->file_count; ++i) {
        if (kstrcmp(s_cwd->files[i].name, name) == 0) { idx = i; break; }
    }
    if (idx < 0) return FS_ERR_NOTFOUND;
    if (s_cwd->files[idx].readonly) return FS_ERR_RDONLY;

    s_cwd->files[idx].name[0] = '\0';
    s_cwd->files[idx].content[0] = '\0';
    s_cwd->files[idx].length = 0;

    for (int j = idx; j < s_cwd->file_count - 1; ++j)
        s_cwd->files[j] = s_cwd->files[j + 1];
    s_cwd->file_count--;

    fs_save_snapshot();
    return FS_OK;
}

int fs_write(const char* name, const char* data) {
    struct File* f = fs_find(name);
    if (!f) return FS_ERR_NOTFOUND;
    if (f->readonly) return FS_ERR_RDONLY;
    if (!data) return FS_ERR_INVALID;

    int len = kstrlen(data);
    if (len >= MAX_CONTENT) len = MAX_CONTENT - 1;

    kstrncpy(f->content, data, MAX_CONTENT);
    f->content[len] = '\0';
    f->length = len;

    fs_save_snapshot();
    return len;
}

int fs_append(const char* name, const char* data) {
    struct File* f = fs_find(name);
    if (!f) return FS_ERR_NOTFOUND;
    if (f->readonly) return FS_ERR_RDONLY;
    if (!data) return FS_ERR_INVALID;

    int old = f->length;
    int add = kstrlen(data);
    if (old + add >= MAX_CONTENT) add = MAX_CONTENT - 1 - old;

    for (int i = 0; i < add; ++i) f->content[old + i] = data[i];
    f->content[old + add] = '\0';
    f->length = old + add;

    fs_save_snapshot();
    return add;
}

void fs_list_counts(int* out_dirs, int* out_files) {
    if (out_dirs) *out_dirs = s_cwd->subdir_count;
    if (out_files) *out_files = s_cwd->file_count;
}

u8 fs_type_from_name(const char* name) {
    if (!name) return FILE_TEXT;
    int L = kstrlen(name);
    if (L <= 0) return FILE_TEXT;

    int dot = -1;
    for (int i = L - 1; i >= 0; --i) {
        if (name[i] == '.') { dot = i; break; }
        if (name[i] == '/' || name[i] == '\\') break;
    }
    if (dot < 0) return FILE_TEXT;

    const char* ext = &name[dot + 1];
    if (kstrcmp(ext, "txt") == 0) return FILE_TEXT;
    if (kstrcmp(ext, "md") == 0) return FILE_MARKDOWN;
    if (kstrcmp(ext, "html") == 0) return FILE_HTML;
    if (kstrcmp(ext, "htm") == 0) return FILE_HTML;
    if (kstrcmp(ext, "nc") == 0) return FILE_NOIRC;
    if (kstrcmp(ext, "sav") == 0) return FILE_GAME;
    if (kstrcmp(ext, "exe") == 0) return FILE_EXE;
    return FILE_TEXT;
}
