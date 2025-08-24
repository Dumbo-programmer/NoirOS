#ifndef FS_H
#define FS_H
#include "common.h"

/* ---- Sizes ---- */
#define MAX_FILENAME 32
#define MAX_CONTENT  2048

/* Per-directory limits (memory footprint tight) */
#define MAX_FILES_PER_DIR  16
#define MAX_DIRS_PER_DIR   8

/* File types */
#define FILE_TEXT 0
#define FILE_EXE  1
#define FILE_GAME 2

/* Error codes */
#define FS_OK           0
#define FS_ERR_NOSPACE -1
#define FS_ERR_EXISTS  -2
#define FS_ERR_NOTFOUND -3
#define FS_ERR_RDONLY  -4
#define FS_ERR_INVALID -5
#define FS_ERR_NOTDIR  -6
#define FS_ERR_DIRNOTEMPTY -7

/* Forward decl */
struct Dir;

/* File visible to other modules */
struct File {
    char name[MAX_FILENAME];
    char content[MAX_CONTENT];
    int  length;
    u8   type;
    u8   readonly;
};

/* Directory node (tree) */
struct Dir {
    char name[MAX_FILENAME];
    struct Dir* parent;

    /* children dirs */
    struct Dir* subdirs[MAX_DIRS_PER_DIR];
    int subdir_count;

    /* files */
    struct File files[MAX_FILES_PER_DIR];
    int file_count;
};

/* ---------- Init / CWD ---------- */
void init_filesystem(void);
struct Dir* fs_root(void);
struct Dir* fs_cwd(void);
void fs_pwd(char* out, int out_len);        /* prints path like /, /docs, /docs/projects */

/* ---------- Directory ops ---------- */
int fs_mkdir(const char* name);              /* in CWD */
int fs_chdir(const char* name);              /* name | ".." | "/" (absolute root) */
int fs_rmdir(const char* name);              /* only if empty */
int fs_dir_count(void);                      /* #subdirs in CWD */
struct Dir* fs_dir_get(int idx);             /* idx < fs_dir_count() */
struct Dir* fs_find_dir(const char* name);   /* in CWD */

/* ---------- File ops in CWD (compat layer kept) ---------- */
int fs_count(void);                          /* files in CWD (kept for UI) */
struct File* fs_get(int idx);                /* file by index in CWD */
struct File* fs_find(const char* name);      /* file by name in CWD */
int fs_create(const char* name, u8 type);    /* create file in CWD */
int fs_delete(const char* name);             /* delete file in CWD (not readonly) */
int fs_write(const char* name, const char* data);
int fs_append(const char* name, const char* data);

/* ---------- Optional helpers ---------- */
void fs_list_counts(int* out_dirs, int* out_files);  /* both counts for UI */

#endif 
