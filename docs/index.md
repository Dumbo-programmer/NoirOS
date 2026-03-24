---
layout: default
title: NoirOS
---

# NoirOS

> "An idiot admires complexity, a genius admires simplicity"
>
> "But I'm an idiot who admires simplicity"

NoirOS is a simple, educational operating system project. It is designed to help users understand the basics of OS development, including bootloading, kernel design, and basic user applications.

## Features
- Custom kernel written in C and Assembly
- Simple shell interface
- Basic file system support
- Text editor
- Snake game
- Mouse and keyboard input handling
- VGA text mode UI

## Getting Started

### Building NoirOS

To build NoirOS, you will need a cross-compiler for i386 and the following tools:
- `make`
- `nasm`
- `qemu` (for emulation)

Clone the repository and run:

```sh
make
```

### Running in QEMU

After building, you can run NoirOS in QEMU:

```sh
make run
```

## Project Structure
- `src/` — Source code for the kernel and applications
- `include/` — Header files
- `boot/` — Bootloader files
- `build/` — Build output
- `iso/` — ISO image structure

## Documentation
- [API Reference](api.md) — Generated summary of module and function documentation.
### Module docs
- [Input](input.md)
- [Editor](editor.md)
- [Filesystem](fs.md)
- [Snake Game](game_snake.md)
- [Kernel](kernel.md)
- [Mouse](mouse.md)
- [Utilities](util.md)
- [VGA / Drawing](vga.md)

## License
NoirOS is released under the MIT License. See the `License` file for details.

## Contributing
Contributions are welcome! Please open issues or pull requests to help improve NoirOS.
