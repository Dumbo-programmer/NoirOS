#include "../include/fs.h"
#include "../include/util.h"

/* Ensure you have kstrncpy in util.c and declared in util.h:
   void kstrncpy(char* d, const char* s, int n);  -- always NUL-terminates. */

/* -------- Internal storage -------- */
static struct Dir  s_root;
static struct Dir* s_cwd = &s_root;

/* -------- Internal helpers -------- */
static int name_invalid(const char* n) {
    if (!n) return 1;
    int L = kstrlen(n);
    if (L <= 0 || L >= MAX_FILENAME) return 1;
    /* forbid slashes inside names (single-component ops) */
    for (int i = 0; i < L; ++i) if (n[i] == '/' || n[i] == '\\') return 1;
    return 0;
}

static struct Dir* dir_find_child(struct Dir* d, const char* name) {
    if (!d || !name) return 0;
    for (int i = 0; i < d->subdir_count; ++i) {
        if (kstrcmp(d->subdirs[i]->name, name) == 0) return d->subdirs[i];
    }
    return 0;
}

static int dir_is_empty(struct Dir* d) {
    return d->file_count == 0 && d->subdir_count == 0;
}

/* -------- Init -------- */
void init_filesystem(void) {
    /* root dir */
    for (int i=0;i<MAX_DIRS_PER_DIR;i++) s_root.subdirs[i] = 0;
    s_root.subdir_count = 0;
    s_root.file_count   = 0;
    s_root.parent       = 0;
    kstrncpy(s_root.name, "/", MAX_FILENAME);
    s_cwd = &s_root;

    /* preload sample content in root */
    struct File* f;

    /* README */
    if (s_root.file_count < MAX_FILES_PER_DIR) {
        f = &s_root.files[s_root.file_count++];
        kstrncpy(f->name, "README.txt", MAX_FILENAME);
        kstrncpy(f->content,
                 "NoirOS\n"
                 "Use arrows/W-S to navigate, Enter for cmd.\n"
                 "Commands: ls, cd, mkdir, rmdir, touch/new, del, edit <file>, pwd\n",
                 MAX_CONTENT);
        f->length  = kstrlen(f->content);
        f->type    = FILE_TEXT;
        f->readonly= 1;
    }

    /* help */
    if (s_root.file_count < MAX_FILES_PER_DIR) {
        f = &s_root.files[s_root.file_count++];
        kstrncpy(f->name, "help.txt", MAX_FILENAME);
        kstrncpy(f->content,
                 "Help:\n"
                 " ls                 - list current folder\n"
                 " cd <dir>|..|/      - change directory\n"
                 " mkdir <name>       - make directory\n"
                 " rmdir <name>       - remove EMPTY directory\n"
                 " new <name> <type>  - create file (type: 0 text, 1 exe, 2 game)\n"
                 " del <name>         - delete file\n"
                 " edit <file>        - open editor\n"
                 " pwd                - show current path\n",
                 MAX_CONTENT);
        f->length  = kstrlen(f->content);
        f->type    = FILE_TEXT;
        f->readonly= 1;
    }

    /* sample editable file */
    if (s_root.file_count < MAX_FILES_PER_DIR) {
        f = &s_root.files[s_root.file_count++];
        kstrncpy(f->name, "notes.md", MAX_FILENAME);
        kstrncpy(f->content, "Editable notes.md\nTry: mkdir docs; cd docs; new todo.txt 0\n", MAX_CONTENT);
        f->length  = kstrlen(f->content);
        f->type    = FILE_TEXT;
        f->readonly= 0;
    }

    /* also create a sample subdir: docs/ with one file */
    if (s_root.subdir_count < MAX_DIRS_PER_DIR) {
        static struct Dir docs; /* static to keep lifetime */
        docs.parent = &s_root;
        for (int i=0;i<MAX_DIRS_PER_DIR;i++) docs.subdirs[i]=0;
        docs.subdir_count=0;
        docs.file_count=0;
        kstrncpy(docs.name, "docs", MAX_FILENAME);
        s_root.subdirs[s_root.subdir_count++] = &docs;

        if (docs.file_count < MAX_FILES_PER_DIR) {
            struct File* df = &docs.files[docs.file_count++];
            kstrncpy(df->name, "guide.txt", MAX_FILENAME);
            kstrncpy(df->content, "Welcome to /docs\n", MAX_CONTENT);
            df->length  = kstrlen(df->content);
            df->type    = FILE_TEXT;
            df->readonly= 0;
        }
    }
}

/* -------- Root/CWD/PWD -------- */
struct Dir* fs_root(void) { return &s_root; }
struct Dir* fs_cwd(void)  { return s_cwd;  }

void fs_pwd(char* out, int out_len) {
    if (!out || out_len <= 0) return;
    /* build path backwards up the tree, then reverse into out */
    const int TMP = 128;
    char stack[TMP];
    int sp = 0;
    struct Dir* d = s_cwd;

    if (d == &s_root) {
        kstrncpy(out, "/", out_len);
        return;
    }

    while (d && d != &s_root && sp < TMP-1) {
        int L = kstrlen(d->name);
        if (sp + L + 1 >= TMP) break;
        /* push '/name' in reverse (we’ll reverse later) */
        stack[sp++] = '/';
        for (int i = L-1; i >= 0; --i) stack[sp++] = d->name[i];
        d = d->parent;
    }
    /* now reverse stack into out */
    int w = 0;
    for (int i = sp-1; i >= 0 && w < out_len-1; --i) out[w++] = stack[i];
    out[w] = 0;
    if (w == 0) kstrncpy(out, "/", out_len);
}

/* -------- Directory ops -------- */
int fs_mkdir(const char* name) {
    if (name_invalid(name)) return FS_ERR_INVALID;
    if (s_cwd->subdir_count >= MAX_DIRS_PER_DIR) return FS_ERR_NOSPACE;
    if (dir_find_child(s_cwd, name)) return FS_ERR_EXISTS;

    /* find a free static slot to place new dir: use static array bank */
    /* approach: allocate from a static pool of directories */
    static struct Dir pool[32];
    static int pool_used[32] = {0};
    int slot = -1;
    for (int i = 0; i < 32; ++i) if (!pool_used[i]) { slot = i; break; }
    if (slot < 0) return FS_ERR_NOSPACE;

    pool_used[slot] = 1;
    struct Dir* nd = &pool[slot];
    nd->parent = s_cwd;
    nd->subdir_count = 0;
    nd->file_count = 0;
    for (int i=0;i<MAX_DIRS_PER_DIR;i++) nd->subdirs[i] = 0;
    kstrncpy(nd->name, name, MAX_FILENAME);

    s_cwd->subdirs[s_cwd->subdir_count++] = nd;
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
            /* remove by shifting */
            for (int j = i; j < s_cwd->subdir_count-1; ++j)
                s_cwd->subdirs[j] = s_cwd->subdirs[j+1];
            s_cwd->subdir_count--;
            return FS_OK;
        }
    }
    return FS_ERR_NOTFOUND;
}

int fs_dir_count(void) {
    return s_cwd->subdir_count;
}
struct Dir* fs_dir_get(int idx) {
    if (idx < 0 || idx >= s_cwd->subdir_count) return 0;
    return s_cwd->subdirs[idx];
}
struct Dir* fs_find_dir(const char* name) {
    return dir_find_child(s_cwd, name);
}

/* -------- Files (scoped to CWD) -------- */
int fs_count(void) { return s_cwd->file_count; }

struct File* fs_get(int idx) {
    if (idx < 0 || idx >= s_cwd->file_count) return 0;
    return &s_cwd->files[idx];
}

struct File* fs_find(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < s_cwd->file_count; ++i)
        if (kstrcmp(s_cwd->files[i].name, name) == 0) return &s_cwd->files[i];
    return 0;
}

int fs_create(const char* name, u8 type) {
    if (name_invalid(name)) return FS_ERR_INVALID;
    if (s_cwd->file_count >= MAX_FILES_PER_DIR) return FS_ERR_NOSPACE;
    if (fs_find(name)) return FS_ERR_EXISTS;

    struct File* f = &s_cwd->files[s_cwd->file_count++];
    kstrncpy(f->name, name, MAX_FILENAME);
    f->content[0] = '\0';
    f->length  = 0;
    f->type    = type;
    f->readonly= 0;
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

    /* clear and shift */
    s_cwd->files[idx].name[0] = '\0';
    s_cwd->files[idx].content[0] = '\0';
    s_cwd->files[idx].length = 0;

    for (int j = idx; j < s_cwd->file_count-1; ++j)
        s_cwd->files[j] = s_cwd->files[j+1];
    s_cwd->file_count--;
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
    return add;
}

/* Both counts (helpful for UI) */
void fs_list_counts(int* out_dirs, int* out_files) {
    if (out_dirs)  *out_dirs  = s_cwd->subdir_count;
    if (out_files) *out_files = s_cwd->file_count;
}
