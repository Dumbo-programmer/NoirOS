# NoirOS Real Power Operations

## What Actually Happens Now

### F1 - Restart
- **Language**: "Gonna restart now...", "Killing processes", "BRB!"
- **Countdown**: 3 seconds (press ESC to cancel)
- **Action**: Actually attempts to restart the NoirOS kernel by jumping to boot address
- **Result**: System will try to reboot itself (may cause QEMU to restart or crash)

### F2 - Shutdown  
- **Language**: "Time to shut down...", "Bye in 3...", "See ya!"
- **Countdown**: 3 seconds (press ESC to cancel)
- **Action**: Halts the CPU permanently with `cli; hlt` instruction
- **Result**: QEMU will freeze/hang (like a real computer being powered off)

### F3 - Sleep
- **Language**: "Time for a nap...", "Sleepy", "Wakey wakey", "Morning!"
- **Countdown**: 2 seconds (press ESC to cancel)
- **Action**: Uses `hlt` instruction to put CPU in low-power state
- **Result**: System truly sleeps until you press any key to wake up

## Language Style
- **Informal**: "Gonna restart", "Time for a nap", "See ya!"
- **Casual**: "BRB!", "Morning!", "Wakey wakey"
- **Fewer Comments**: Removed most technical comments from code
- **Direct**: "Killing processes" instead of "Stopping user processes"

## Technical Implementation

### Restart
```c
void (*restart_kernel)(void) = (void (*)(void))0x100000;
restart_kernel(); // Jump to kernel start address
```

### Shutdown
```c
while(1) {
    __asm__ volatile ("cli; hlt"); // Disable interrupts and halt forever
}
```

### Sleep
```c
__asm__ volatile ("hlt"); // Halt until interrupt (keypress)
```

## User Experience

1. **More Natural**: Power operations feel like a real OS
2. **Shorter Countdowns**: 2-3 seconds instead of 5
3. **Casual Language**: Friendlier, less corporate
4. **Real Effects**: Actually impacts the system state
5. **Escape Hatches**: Still can cancel with ESC during countdown

## What to Expect

- **Shutdown**: QEMU window will become unresponsive (like a powered-off computer)
- **Restart**: May cause QEMU to restart or require manual restart
- **Sleep**: Screen goes dark, press any key to wake up with animation

The power buttons now behave like a real operating system within the QEMU environment!
