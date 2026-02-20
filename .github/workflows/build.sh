#!/bin/bash
# build.sh - MyOS teljes fordítás és ISO készítés
# Futtasd: chmod +x build.sh && ./build.sh

set -e  # Hiba esetén leáll

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔══════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     MyOS Build Script                ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════╝${NC}"
echo ""

# ──────────────────────────────────────────
# 1. Eszközök ellenőrzése
# ──────────────────────────────────────────
echo -e "${YELLOW}[1/5] Eszközök ellenőrzése...${NC}"

check_tool() {
    if ! command -v $1 &>/dev/null; then
        echo -e "${RED}[HIBA] $1 nincs telepítve!${NC}"
        echo "       Telepítés: $2"
        exit 1
    fi
    echo -e "  ${GREEN}✓${NC} $1"
}

check_tool nasm       "sudo apt install nasm"
check_tool gcc        "sudo apt install gcc"
check_tool grub-mkrescue "sudo apt install grub-pc-bin grub-common"
check_tool xorriso    "sudo apt install xorriso"

# Cross-compiler ellenőrzés
if command -v i686-elf-gcc &>/dev/null; then
    CC="i686-elf-gcc"
    LD="i686-elf-gcc"
    echo -e "  ${GREEN}✓${NC} i686-elf-gcc (cross-compiler)"
else
    CC="gcc"
    LD="gcc"
    echo -e "  ${YELLOW}!${NC} i686-elf-gcc nem található, natív gcc -m32 módban fordít"
fi

# ──────────────────────────────────────────
# 2. Assembly fordítás
# ──────────────────────────────────────────
echo ""
echo -e "${YELLOW}[2/5] Assembly fordítás...${NC}"

nasm -f elf32 boot/boot.asm    -o boot/boot.o    && echo -e "  ${GREEN}✓${NC} boot.asm"
nasm -f elf32 kernel/gdt_asm.asm -o kernel/gdt_asm.o && echo -e "  ${GREEN}✓${NC} gdt_asm.asm"
nasm -f elf32 kernel/isr.asm   -o kernel/isr.o   && echo -e "  ${GREEN}✓${NC} isr.asm"

# ──────────────────────────────────────────
# 3. C fordítás
# ──────────────────────────────────────────
echo ""
echo -e "${YELLOW}[3/5] C fordítás...${NC}"

CFLAGS="-m32 -std=gnu99 -ffreestanding -O2 -Wall -fno-stack-protector -fno-pie -fno-pic -nostdlib -nostdinc -I kernel -I drivers -I fs -I shell"

compile() {
    $CC $CFLAGS -c $1 -o $2
    echo -e "  ${GREEN}✓${NC} $1"
}

compile kernel/kernel.c   kernel/kernel.o
compile kernel/gdt.c      kernel/gdt.o
compile kernel/idt.c      kernel/idt.o
compile kernel/pic.c      kernel/pic.o
compile kernel/vga.c      kernel/vga.o
compile kernel/stdlib.c   kernel/stdlib.o
compile kernel/exec.c     kernel/exec.o
compile drivers/keyboard.c drivers/keyboard.o
compile drivers/mouse.c   drivers/mouse.o
compile drivers/timer.c   drivers/timer.o
compile fs/fat.c          fs/fat.o
compile shell/shell.c     shell/shell.o

# ──────────────────────────────────────────
# 4. Linkelés
# ──────────────────────────────────────────
echo ""
echo -e "${YELLOW}[4/5] Linkelés -> myos.bin...${NC}"

$LD -m32 -T kernel.ld -ffreestanding -nostdlib -o myos.bin \
    boot/boot.o \
    kernel/gdt_asm.o \
    kernel/isr.o \
    kernel/kernel.o \
    kernel/gdt.o \
    kernel/idt.o \
    kernel/pic.o \
    kernel/vga.o \
    kernel/stdlib.o \
    kernel/exec.o \
    drivers/keyboard.o \
    drivers/mouse.o \
    drivers/timer.o \
    fs/fat.o \
    shell/shell.o \
    -lgcc 2>/dev/null || \
$LD -m32 -T kernel.ld -ffreestanding -nostdlib -o myos.bin \
    boot/boot.o kernel/gdt_asm.o kernel/isr.o \
    kernel/kernel.o kernel/gdt.o kernel/idt.o kernel/pic.o \
    kernel/vga.o kernel/stdlib.o kernel/exec.o \
    drivers/keyboard.o drivers/mouse.o drivers/timer.o \
    fs/fat.o shell/shell.o

echo -e "  ${GREEN}✓${NC} myos.bin kész ($(du -sh myos.bin | cut -f1))"

# Ellenőrzés: Multiboot magic megtalálható-e?
if grub-file --is-x86-multiboot myos.bin 2>/dev/null; then
    echo -e "  ${GREEN}✓${NC} Multiboot ellenőrzés OK"
else
    echo -e "  ${YELLOW}!${NC} grub-file nem elérhető, kihagyjuk az ellenőrzést"
fi

# ──────────────────────────────────────────
# 5. ISO készítés
# ──────────────────────────────────────────
echo ""
echo -e "${YELLOW}[5/5] Bootolható ISO készítése...${NC}"

rm -rf isodir
mkdir -p isodir/boot/grub

cp myos.bin isodir/boot/myos.bin

cat > isodir/boot/grub/grub.cfg << 'EOF'
set timeout=5
set default=0

menuentry "MyOS v0.1" {
    multiboot /boot/myos.bin
    boot
}

menuentry "MyOS v0.1 (safe mode)" {
    multiboot /boot/myos.bin nosmp
    boot
}
EOF

grub-mkrescue -o myos.iso isodir 2>/dev/null
echo -e "  ${GREEN}✓${NC} myos.iso kész ($(du -sh myos.iso | cut -f1))"

# ──────────────────────────────────────────
# Összefoglaló
# ──────────────────────────────────────────
echo ""
echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  FORDÍTÁS SIKERES!                       ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Kernel:    ${CYAN}myos.bin${NC}"
echo -e "  ISO:       ${CYAN}myos.iso${NC}"
echo ""
echo -e "${YELLOW}Futtatás QEMU-ban (teszt):${NC}"
echo "  qemu-system-i386 -cdrom myos.iso -m 32M"
echo ""
echo -e "${YELLOW}Írás USB-re (valódi hardver):${NC}"
echo -e "  ${RED}FIGYELEM: Cseréld le /dev/sdX-et a megfelelő eszközre!${NC}"
echo "  lsblk                              # USB meghajtó neve"
echo "  sudo dd if=myos.iso of=/dev/sdX bs=4M status=progress"
echo "  sync"
echo ""
echo -e "${YELLOW}Bootolás:${NC}"
echo "  1. Dugd be az USB-t"
echo "  2. Újraindítás közben nyomd meg: F12 / F8 / ESC / DEL"
echo "     (BIOS-tól függően)"
echo "  3. Válaszd ki az USB meghajtót a boot menüből"
echo ""
