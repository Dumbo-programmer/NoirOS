#!/bin/bash
# NoirOS Build Script for WSL/Linux

set -e

echo "Building NoirOS..."

# Check required tools
if ! command -v gcc &>/dev/null; then
    echo "Installing build tools..."
    sudo apt update
    sudo apt install -y build-essential gcc-multilib
fi

if ! command -v grub-mkrescue &>/dev/null; then
    echo "Installing GRUB tools..."
    sudo apt install -y grub-pc-bin grub-common xorriso
fi

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
cd "$SCRIPT_DIR"

echo "Working directory: $(pwd)"

# Clean previous builds
rm -rf build kernel.elf kernel.bin NoirOS.iso iso

# Create build directory
mkdir -p build

# Compile all C sources found in src/ — not a hardcoded list, so new files
# are picked up automatically without editing this script.
echo "Compiling C sources..."
for src in src/*.c; do
    obj="build/$(basename "$src" .c).o"
    echo "  $src -> $obj"
    gcc -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-pic -Iinclude -c "$src" -o "$obj"
done

# Compile assembly
echo "Compiling assembly..."
as --32 src/start.S -o build/start.o

# Link kernel — gather all .o files dynamically
echo "Linking kernel..."
OBJS=$(ls build/*.o)
ld -m elf_i386 -T linker.ld -nostdlib -z max-page-size=0x1000 \
   --build-id=none -N \
   -o kernel.elf $OBJS

echo "Built ELF kernel: kernel.elf"

# Create flat binary
echo "Creating binary..."
objcopy -O binary kernel.elf kernel.bin
echo "Built raw binary: kernel.bin"

# Create bootable ISO
echo "Creating ISO..."
mkdir -p iso/boot/grub
cp kernel.elf iso/boot/kernel.elf
cp boot/grub/grub.cfg iso/boot/grub/
grub-mkrescue -o NoirOS.iso iso/ 2>/dev/null
echo "Created NoirOS.iso"

echo ""
echo "Build complete!"
echo "To run: qemu-system-i386 -cdrom NoirOS.iso -m 512"
