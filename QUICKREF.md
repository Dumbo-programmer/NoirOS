# NoirOS Quick Reference

## Overview
NoirOS is a minimalist hobby operating system with a modular architecture, basic file system, and simple UI.

## Features
- File explorer and viewer
- Editable files and directories
- Command input (ls, edit, help, snake)
- Power controls (restart, shutdown, sleep)
- Mouse and keyboard support

## Main Components
- `src/kernel.c`: Main loop, mode switching, command input
- `src/ui.c`: UI drawing, explorer, viewer, power screens
- `src/fs.c`: In-memory file system, directory and file operations
- `src/editor.c`: Basic text editor
- `src/game_snake.c`: Snake game
- `include/*.h`: Header files for each subsystem

## Build & Run
- See `BUILD.md` for Windows build instructions (WSL required)
- Use `wsl bash build.sh` to build
- Run with QEMU: `qemu-system-i386 -cdrom NoirOS.iso -m 512`

## Commands (in UI)
- `ls` / `dir`: List files
- `edit <file>`: Open editor
- `help`: Show help file
- `snake`: Launch snake game

## Power Controls
- F1: Restart
- F2: Shutdown
- F3: Sleep

## Extending NoirOS
- Add new apps/games in `src/`
- Register in `kernel.c` and UI
- Use modular headers for new features

## Troubleshooting
- If build fails, check WSL and required packages
- For UI issues, verify VGA and input modules

## License
MIT License
