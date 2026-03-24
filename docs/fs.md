---
layout: default
title: Filesystem Module
---

# Filesystem (`src/fs.c`)

Overview and functions:

- `init_filesystem()` — Initialize in-memory filesystem and preload example files and a `/docs` directory.
- `fs_root()`, `fs_cwd()` — Get pointers to root and current working directory.
- `fs_pwd(out, out_len)` — Write current working directory path into `out`.
- `fs_mkdir(name)` — Create a new directory in CWD; returns `FS_OK` or error codes.
- `fs_chdir(name)` — Change current working directory.
- `fs_rmdir(name)` — Remove an empty subdirectory and free its pool slot.
- File operations: `fs_count()`, `fs_get(idx)`, `fs_find(name)`, `fs_create(name,type)`, `fs_delete(name)`, `fs_write(name,data)`, `fs_append(name,data)`.
- Utilities: `fs_dir_count()`, `fs_dir_get(idx)`, `fs_find_dir(name)`, `fs_list_counts(out_dirs,out_files)`.

Source: [src/fs.c](../src/fs.c)
