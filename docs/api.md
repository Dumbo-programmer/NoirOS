---
layout: default
title: API Reference
---

# NoirOS API Reference

This page maps the main public APIs and modules used across NoirOS.

## Source Files

- [input.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/input.c)
- [shell.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/shell.c)
- [fs.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/fs.c)
- [kernel.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/kernel.c)
- [ui.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/ui.c)
- [vga.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/vga.c)
- [game_snake.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/game_snake.c)
- [games_extra.c](https://github.com/Dumbo-programmer/NoirOS/blob/main/src/games_extra.c)

## Core System APIs

### Kernel and Modes

- `kernel_main()` initializes subsystems and runs the main event loop.
- Mode switching uses `MODE_BROWSER`, `MODE_EDITOR`, and `MODE_GAME`.
- Startup sequence includes `ui_show_boot_loader()` before `ui_draw()`.

### Input

- `read_key()` translates keyboard scancodes into logical keys.
- `wait_key()` blocks until a key is available.
- Modifier and debug helpers support shell/editor/game behavior.

### Filesystem

- Tree-based FS with directories and typed files.
- Public API: `fs_mkdir`, `fs_chdir`, `fs_rmdir`, `fs_create`, `fs_delete`, `fs_write`, `fs_append`.
- Boot-time snapshot load/save keeps persistent state across runs.

### Shell

- Persistent command panel with history and aliases.
- Command dispatch table maps command names to handlers.
- Includes utilities (`calc`, `history`, `uname`, `whoami`) and file ops (`cp`, `mv`, `rm`, `touch`).

### UI and VGA

- UI draws multi-pane explorer/viewer controls and status.
- VGA layer provides cell rendering, buffered flush, and helper drawing primitives.

## Error and Status Conventions

- Filesystem uses integer status codes (e.g. `FS_OK`, `FS_ERR_NOTFOUND`, `FS_ERR_RDONLY`).
- Shell command handlers return success/failure to command loop.
- Status and error messages are printed in the command panel/status area.

## Related Module Pages

- [Filesystem module](fs.html)
- [Kernel module](kernel.html)
- [Input module](input.html)
- [Editor module](editor.html)
- [VGA module](vga.html)
