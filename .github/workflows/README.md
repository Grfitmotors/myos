# MyOS - x86 32-bit Operating System

Egy egyszerű, de működőképes operációs rendszer x86 32-bit Protected Mode-ban,
GRUB Multiboot indítással, valódi hardveren is futtatható.

## Architektúra

```
myos/
├── boot/
│   └── boot.asm          # Multiboot header + stack setup, belépési pont
├── kernel/
│   ├── kernel.c/h        # Fő kernel, típusok, I/O makrók
│   ├── gdt.c/h           # Global Descriptor Table (szegmensek)
│   ├── gdt_asm.asm        # GDT flush Assembly stub
│   ├── idt.c/h           # Interrupt Descriptor Table
│   ├── isr.asm           # ISR/IRQ Assembly stub-ok (0-47)
│   ├── pic.c             # 8259 PIC (interrupt vezérlő)
│   ├── vga.c/h           # VGA text mode driver (80x25)
│   ├── stdlib.c          # memset, memcpy, strcmp, stb.
│   └── exec.c/h          # EXE/BIN program betöltő
├── drivers/
│   ├── keyboard.c/h      # PS/2 billentyűzet (IRQ1, scancode set 1)
│   ├── mouse.c/h         # PS/2 egér (IRQ12, 3 gombos)
│   └── timer.c/h         # PIT timer (IRQ0, 100Hz)
├── fs/
│   └── fat.c/h           # FAT12/FAT16 + ATA PIO olvasás
├── shell/
│   └── shell.c/h         # Interaktív parancssor
├── kernel.ld             # Linker script (1MB betöltési cím)
├── Makefile
└── grub.cfg
```

## Funkciók

| Modul | Leírás |
|---|---|
| **GDT** | 5 szegmens: null, kernel code/data, user code/data |
| **IDT** | 32 CPU kivétel + 16 IRQ (0-47) |
| **PIC** | 8259 PIC újraképezés 0x20/0x28-ra |
| **VGA** | 80×25 szöveges mód, görgetés, kurzor, 16 szín |
| **PS/2 Keyboard** | IRQ1, US QWERTY, Shift/CapsLock/Ctrl/Alt |
| **PS/2 Mouse** | IRQ12, X/Y pozíció, 3 gomb, valódi hardveren is! |
| **PIT Timer** | 100Hz, uptime számolás |
| **FAT12/16** | ATA PIO olvasás, könyvtár lista, fájl olvasás |
| **Exec** | Flat binary (.bin) és PE32 (.exe) betöltés |
| **Shell** | Interaktív parancssor 10 beépített paranccsal |

## Shell parancsok

```
help     - Parancsok listája
cls      - Képernyő törlése
echo <t> - Szöveg kiírása
info     - Rendszer infó
mouse    - Egér állapot (X, Y, gombok)
time     - Rendszer uptime
ls       - FAT fájlok listázása
run <f>  - Program futtatása (.bin vagy .exe)
color    - VGA szín teszt
reboot   - Újraindítás
```

## Fordítás

### Szükséges eszközök

```bash
# Ubuntu/Debian:
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso qemu-system-x86

# Cross-compiler (ajánlott valódi hardverre):
# Lásd: https://wiki.osdev.org/GCC_Cross-Compiler
# Cél: i686-elf-gcc
```

### Fordítás és ISO készítés

```bash
cd myos
make          # Lefordítja a kernelt (myos.bin)
make iso      # Bootolható ISO (myos.iso)
make run      # QEMU-ban tesztelés
```

### Valódi hardverre írás

```bash
# FIGYELEM: Cseréld le /dev/sdX-et a megfelelő USB meghajtóra!
# Ellenőrizd: lsblk
sudo dd if=myos.iso of=/dev/sdX bs=4M && sync
```

Ezután az USB meghajtóról bootrólható a gép.

## PS/2 egér - valódi hardver

Az egér driver az alábbi lépéseket hajtja végre inicializáláskor:

1. `0xA8` → Port `0x64`: PS/2 egér port engedélyezése
2. Controller config módosítása: IRQ12 engedélyezése, egér órajel bekapcsolása
3. Egér reset (`0xFF`), default beállítások (`0xF6`)
4. Sample rate: 100/sec, felbontás: 4 counts/mm
5. Adatküldés engedélyezése (`0xF4`)
6. IRQ12 handler telepítése + IRQ2 (cascade) engedélyezése

Az egér kurzora egy `█` (0xDB) karakterként jelenik meg a VGA képernyőn.

## EXE futtatás korlátozásai

A jelenlegi verzió Ring 0-ban (kernel módban) fut minden programot.
Valódi védelemhez szükséges lenne:
- Lapozás (paging) + virtuális memória
- Ring 3 (user mode) átváltás
- Syscall interface

**Flat binary (.bin):** 0x400000 (4MB) címre töltve, azonnal futtatva.

**PE32 (.exe):** MZ + PE fejléc feldolgozás, belépési pont kiszámítása.
> Fontos: A programok ne használjanak Windows API-t (kernel32.dll stb.),
> csak a saját kerneled funkcióit hívhatják meg!

## Fejlesztési lehetőségek

- [ ] Paging + virtuális memória
- [ ] Ring 3 user mode
- [ ] VGA grafikus mód (320x200 Mode 13h)
- [ ] FAT írás (fájl létrehozás)
- [ ] Több folyamat (multitasking)
- [ ] Hálózati stack (RTL8139 driver)
