// kernel.c - Main kernel entry point

#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../fs/fat.h"
#include "../shell/shell.h"

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // Initialize VGA text mode first
    vga_init();
    vga_clear();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("  __  __       ___  ____  \n");
    vga_print(" |  \\/  |_   _/ _ \\/ ___| \n");
    vga_print(" | |\\/| | | | | | | |     \n");
    vga_print(" | |  | | |_| | |_| | |__ \n");
    vga_print(" |_|  |_|\\__, |\\___/ \\____|\n");
    vga_print("          |___/             \n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("\n  MyOS v0.1 - x86 32-bit\n\n");

    // Verify multiboot
    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_print("[PANIC] Not booted by a Multiboot-compliant bootloader!\n");
        for(;;) __asm__("hlt");
    }

    // Initialize core systems
    vga_print("[INIT] Setting up GDT...\n");
    gdt_init();

    vga_print("[INIT] Setting up IDT & ISRs...\n");
    idt_init();

    vga_print("[INIT] Setting up PIC...\n");
    pic_remap(0x20, 0x28);

    vga_print("[INIT] Setting up Timer (PIT)...\n");
    timer_init(100);    // 100 Hz

    vga_print("[INIT] Setting up PS/2 Keyboard...\n");
    keyboard_init();

    vga_print("[INIT] Setting up PS/2 Mouse...\n");
    mouse_init();

    vga_print("[INIT] Setting up FAT filesystem...\n");
    // fat_init() would need ATA driver; placeholder for now
    // fat_init();

    vga_print("[INIT] All systems nominal!\n\n");

    // Enable interrupts
    __asm__("sti");

    // Start shell
    shell_init();
    shell_run();

    // Should never reach here
    for(;;) __asm__("hlt");
}
