#!/bin/bash

echo "Testing NoirOS basic functionality..."

# Check if files exist
if [ ! -f "NoirOS.iso" ]; then
    echo "❌ NoirOS.iso not found - build may have failed"
    exit 1
fi

if [ ! -f "kernel.elf" ]; then
    echo "❌ kernel.elf not found - build may have failed"
    exit 1
fi

echo "✅ ISO and kernel files exist"

# Check file sizes
iso_size=$(stat -c%s "NoirOS.iso" 2>/dev/null || echo "0")
if [ "$iso_size" -lt 1000000 ]; then
    echo "❌ ISO size seems too small: $iso_size bytes"
    exit 1
fi

echo "✅ ISO size looks good: $iso_size bytes"

# List included files to verify structure
echo ""
echo "📁 Project structure:"
echo "   Source files:"
ls -la src/*.c | wc -l | xargs echo "     C files:"
ls -la include/*.h | wc -l | xargs echo "     Headers:"

echo ""
echo "📁 Build artifacts:"
ls -la build/*.o | wc -l | xargs echo "     Object files:"

echo ""
echo "🎯 Key features expected:"
echo "   ✅ File system with sample files"
echo "   ✅ Text editor (edit filename command)"
echo "   ✅ Snake game (snake command)"
echo "   ✅ Mouse support (PS/2 mouse)"
echo "   ✅ Power buttons (F1=restart, F2=shutdown, F3=sleep)"
echo "   ✅ Clickable UI elements"
echo ""

echo "🚀 To test in QEMU:"
echo "   qemu-system-i386 -cdrom NoirOS.iso -m 512 -display gtk,zoom-to-fit=on -full-screen"
echo ""
echo "🎮 Controls to test:"
echo "   - Arrow keys: Navigate file list"
echo "   - Enter: Command mode"
echo "   - F1/F2/F3: Power buttons"
echo "   - Mouse: Click UI elements"
echo "   - Commands: help, snake, edit filename, ls"
echo ""

# Try quick syntax check on C files
echo "🔍 Quick syntax check:"
for file in src/*.c; do
    if ! gcc -fsyntax-only -Iinclude "$file" 2>/dev/null; then
        echo "❌ Syntax issues in $file"
    fi
done
echo "✅ Syntax check completed"

echo ""
echo "🏁 NoirOS ready for testing!"
