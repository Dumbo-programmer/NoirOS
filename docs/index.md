---
layout: default
title: NoirOS
---

# NoirOS

<span style="color:#23f0d8; font-weight:700;">Noir</span> <span style="color:#ff5a64; font-weight:700;">OS</span> docs in retro terminal mode.

> "An idiot admires complexity, a genius admires simplicity"
>
> "But I'm an idiot who admires simplicity"

NoirOS is a simple, educational operating system project. It is designed to help users understand the basics of OS development, including bootloading, kernel design, and basic user applications.

## Demo Video

<video controls autoplay muted loop playsinline preload="metadata" width="900">
  <source src="https://raw.githubusercontent.com/Dumbo-programmer/NoirOS/main/Images/demo.mp4" type="video/mp4">
  Your browser does not support embedded video. Download it from <a href="https://raw.githubusercontent.com/Dumbo-programmer/NoirOS/main/Images/demo.mp4">demo.mp4</a>.
</video>

<p><strong style="color:#23f0d8;">Tip:</strong> The video is pulled directly from the GitHub repository to avoid static path 404s.</p>

## Features
- Custom kernel written in C and Assembly
- Persistent shell command panel
- Basic file system support
- Text editor
- Snake, Pong, Dodge, and Catch games
- Mouse and keyboard input handling
- VGA text mode UI
- Boot splash loader with Noir OS logo

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
- [Playground](playground.html) - Try NoirOS commands in your browser.
- [API Reference](api.html) — Public interfaces and module map.
### Module docs
- [Input](input.html)
- [Editor](editor.html)
- [Filesystem](fs.html)
- [Snake Game](game_snake.html)
- [Kernel](kernel.html)
- [Mouse](mouse.html)
- [Utilities](util.html)
- [VGA / Drawing](vga.html)

## License
NoirOS is released under the MIT License. See the `License` file for details.

## Contributing
Contributions are welcome! Please open issues or pull requests to help improve NoirOS.
