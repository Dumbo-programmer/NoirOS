CC := gcc
AS := as
LD := ld
OBJCOPY := objcopy

CFLAGS := -ffreestanding -O2 -Wall -Wextra -m32 -fno-pie -fno-pic -Iinclude

SRC := src
OBJ := build

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
ASM := $(SRC)/start.S
ASMOBJ := $(OBJ)/start.o

ISO_DIR := iso
ISO := NoirOS.iso
KERNEL_ELF := kernel.elf
KERNEL_BIN := kernel.bin

.PHONY: all clean prepare_iso run

all: $(KERNEL_ELF) $(KERNEL_BIN) $(ISO)

$(OBJ):
	mkdir -p $(OBJ)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(ASMOBJ): $(ASM) | $(OBJ)
	$(AS) --32 $< -o $@

# Link into ELF using linker.ld
$(KERNEL_ELF): $(OBJS) $(ASMOBJ) linker.ld
	$(LD) -m elf_i386 -T linker.ld -nostdlib -z max-page-size=0x1000 \
	     --build-id=none -N \
	     -o $(KERNEL_ELF) $(ASMOBJ) $(OBJS)
	@echo "Built ELF kernel: $(KERNEL_ELF)"


# Convert ELF -> raw binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)
	@echo "Built raw binary: $(KERNEL_BIN)"

# Copy ELF kernel into ISO (GRUB prefers ELF!)
prepare_iso: $(KERNEL_ELF)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	@cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/

$(ISO): prepare_iso
	@grub-mkrescue -o $(ISO) $(ISO_DIR) >/dev/null 2>&1 || \
	 (echo "grub-mkrescue failed - ensure grub & xorriso are installed"; false)
	@echo "Created $(ISO)"

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 512

clean:
	rm -rf $(OBJ) $(KERNEL_ELF) $(KERNEL_BIN) $(ISO) $(ISO_DIR)
