# MyOS Makefile
# Requires: nasm, gcc (cross-compiler i686-elf-gcc), grub-mkrescue, xorriso

# Try to find a suitable cross-compiler
CC_CROSS := i686-elf-gcc
CC_NATIVE := gcc

# Check if cross-compiler exists
ifeq (, $(shell which $(CC_CROSS) 2>/dev/null))
    CC := $(CC_NATIVE)
    LD := $(CC_NATIVE)
    CROSS := 0
    $(info [WARN] Cross-compiler not found, using native gcc with -m32)
else
    CC := $(CC_CROSS)
    LD := i686-elf-ld
    CROSS := 1
    $(info [OK] Using cross-compiler: $(CC_CROSS))
endif

AS      := nasm
ASFLAGS := -f elf32
CFLAGS  := -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
           -fno-stack-protector -fno-pie -fno-pic \
           -I kernel -I drivers -I fs -I shell

ifeq ($(CROSS),0)
    CFLAGS += -nostdlib -nostdinc
    LDFLAGS := -m32 -ffreestanding -nostdlib -T kernel.ld
else
    LDFLAGS := -T kernel.ld -ffreestanding -O2 -nostdlib
endif

LIBS := -lgcc

# Source files
ASM_SOURCES := boot/boot.asm \
               kernel/gdt_asm.asm \
               kernel/isr.asm

C_SOURCES   := kernel/kernel.c \
               kernel/gdt.c \
               kernel/idt.c \
               kernel/pic.c \
               kernel/vga.c \
               kernel/stdlib.c \
               kernel/exec.c \
               drivers/keyboard.c \
               drivers/mouse.c \
               drivers/timer.c \
               fs/fat.c \
               shell/shell.c

# Object files
ASM_OBJS := $(ASM_SOURCES:.asm=.o)
C_OBJS   := $(C_SOURCES:.c=.o)
OBJS     := $(ASM_OBJS) $(C_OBJS)

TARGET   := myos.bin
ISO      := myos.iso

.PHONY: all clean iso run

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD]  Linking $@"
ifeq ($(CROSS),0)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
else
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
endif
	@echo "[OK]  Built $@"

# ASM rules
%.o: %.asm
	@echo "[AS]  $<"
	$(AS) $(ASFLAGS) $< -o $@

# C rules
%.o: %.c
	@echo "[CC]  $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create bootable ISO with GRUB
iso: $(TARGET)
	@echo "[ISO] Creating bootable ISO..."
	mkdir -p isodir/boot/grub
	cp $(TARGET) isodir/boot/myos.bin
	echo 'set timeout=3'                            >  isodir/boot/grub/grub.cfg
	echo 'set default=0'                            >> isodir/boot/grub/grub.cfg
	echo 'menuentry "MyOS" {'                       >> isodir/boot/grub/grub.cfg
	echo '    multiboot /boot/myos.bin'             >> isodir/boot/grub/grub.cfg
	echo '    boot'                                 >> isodir/boot/grub/grub.cfg
	echo '}'                                        >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir
	@echo "[OK]  ISO created: $(ISO)"

# Run in QEMU (for testing)
run: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

# Run on real hardware: dd if=myos.iso of=/dev/sdX bs=4M
flash: iso
	@echo "To flash to USB: sudo dd if=$(ISO) of=/dev/sdX bs=4M && sync"

clean:
	@echo "[RM]  Cleaning..."
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf isodir
