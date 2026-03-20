# NoirOS - Build Instructions


## Building on Windows

### Prerequisites
1. **WSL (Windows Subsystem for Linux)** - Install Ubuntu from Microsoft Store
2. **Build tools in WSL**:
   ```bash
   sudo apt update
   sudo apt install build-essential gcc-multilib grub-pc-bin grub-common xorriso
   ```

### Quick Build
Use the provided build script:
```powershell
wsl bash build.sh
```

This will:
- Compile all C sources with appropriate flags (`-m32`, `-ffreestanding`)
- Assemble the bootloader (`start.S`)
- Link everything using the custom linker script (`linker.ld`)
- Generate `kernel.elf`, `kernel.bin`, and `NoirOS.iso`

### Manual Build (in WSL)
```bash
# Clean
rm -rf build kernel.elf kernel.bin NoirOS.iso iso

# Compile
mkdir -p build
gcc -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-pic -Iinclude -c src/*.c -o build/
as --32 src/start.S -o build/start.o

# Link
ld -m elf_i386 -T linker.ld -nostdlib -z max-page-size=0x1000 --build-id=none -N \
   -o kernel.elf build/start.o build/*.o

# Create ISO
mkdir -p iso/boot/grub
cp kernel.elf iso/boot/kernel.elf
cp boot/grub/grub.cfg iso/boot/grub/
grub-mkrescue -o NoirOS.iso iso/
```

## Running
### With QEMU (recommended):
```bash
qemu-system-i386 -cdrom NoirOS.iso -m 512
```

### With VirtualBox:
1. Create new VM (Type: Other, Version: Other/Unknown)
2. Set RAM to 512MB
3. Mount `NoirOS.iso` as CD-ROM
4. Boot from CD

## Features
- **File System**: Tree-based in-memory filesystem with directories
- **Text Editor**: Multi-line editor with syntax support
- **Snake Game**: Real-time game with collision detection
- **Mouse Support**: Full PS/2 mouse integration with clickable UI
- **Power Controls**: Animated restart, shutdown, and sleep screens
- **Shell**: Command-line interface with history

## Controls
### Mouse:
- Click file explorer items to select
- Click power buttons (Restart/Shutdown/Sleep)
- Click viewer panel to scroll

### Keyboard:
- **F1**: Restart system
- **F2**: Shutdown system  
- **F3**: Sleep mode
- **Arrow Keys**: Navigate file explorer
- **Enter**: Command mode
- **ESC**: Exit modes

## Technical Details
- **Architecture**: x86 (32-bit)
- **Boot**: Multiboot-compliant with GRUB
- **Memory**: Direct VGA text buffer manipulation
- **Input**: Raw scancode processing for keyboard/mouse
- **No stdlib**: All utilities implemented from scratch

## Known Warnings
The build produces some warnings for missing function declarations - these are non-critical and don't affect functionality.

## File Structure
```
src/           - Source code (.c files)
include/       - Header files (.h files)
boot/grub/     - GRUB configuration
build/         - Compiled object files
iso/           - ISO staging directory
build.sh       - Automated build script
linker.ld      - Custom linker script
Makefile       - Build configuration (Windows/Linux)
```
