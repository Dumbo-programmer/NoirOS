---
layout: default
title: Filesystem Module
---

# Filesystem (`src/fs.c`)

The NoirOS filesystem is a lightweight tree structure with persistent snapshots.

## Design

- Root directory plus nested subdirectories.
- Fixed-size arrays for files and directories (predictable memory use).
- File typing (`text`, `markdown`, `html`, `game`, `noirc`, `exe`).
- Optional persistence via ATA + FAT snapshot file.

## Public API

### Initialization and Context

- `init_filesystem()`
- `fs_root()`
- `fs_cwd()`
- `fs_pwd(out, out_len)`

### Directory Operations

- `fs_mkdir(name)`
- `fs_chdir(name)` where `name` can be directory name, `..`, or `/`
- `fs_rmdir(name)` (empty directory only)
- `fs_dir_count()` / `fs_dir_get(idx)` / `fs_find_dir(name)`

### File Operations

- `fs_count()` / `fs_get(idx)` / `fs_find(name)`
- `fs_create(name, type)`
- `fs_delete(name)`
- `fs_write(name, data)`
- `fs_append(name, data)`

### Helpers

- `fs_list_counts(out_dirs, out_files)`
- `fs_type_from_name(name)`

## Error Codes

- `FS_OK`
- `FS_ERR_NOSPACE`
- `FS_ERR_EXISTS`
- `FS_ERR_NOTFOUND`
- `FS_ERR_RDONLY`
- `FS_ERR_INVALID`
- `FS_ERR_NOTDIR`
- `FS_ERR_DIRNOTEMPTY`

## Shell Mapping

- `mkdir` -> `fs_mkdir`
- `cd` -> `fs_chdir`
- `rmdir` -> `fs_rmdir`
- `touch`, `new` -> `fs_create`
- `rm`, `del` -> `fs_delete`
- `cp`, `mv` -> `fs_create` + `fs_write` (+ delete for `mv`)

## Notes

- In-OS docs (`README.txt`, `readme.md`, `help.txt`) are auto-refreshed at boot.
- Game launcher files are auto-created under `/games`.

Source: [fs.c on GitHub](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/fs.c)
