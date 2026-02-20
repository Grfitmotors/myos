// shell.c - Interactive command shell

#include "shell.h"
#include "../kernel/kernel.h"
#include "../kernel/vga.h"
#include "../kernel/exec.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../fs/fat.h"

#define CMD_BUF_SIZE 256
#define MAX_ARGS 16

static char cmd_buf[CMD_BUF_SIZE];
static int cmd_len = 0;

// ──────────────────────────── string utilities ────────────────────────────

static void shell_print_hex(uint32_t v) { vga_print_hex(v); }
static void shell_print_dec(uint32_t v) { vga_print_dec(v); }

// Parse command line into argc/argv
static int parse_args(char* line, char* argv[], int max) {
    int argc = 0;
    while (*line && argc < max) {
        while (*line == ' ') line++;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
        if (*line == ' ') *line++ = '\0';
    }
    return argc;
}

// ──────────────────────────── built-in commands ───────────────────────────

static void cmd_help(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("━━━ MyOS Commands ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("  help     - Show this help\n");
    vga_print("  cls      - Clear screen\n");
    vga_print("  echo <t> - Print text\n");
    vga_print("  info     - System information\n");
    vga_print("  mouse    - Show mouse state\n");
    vga_print("  time     - Show system uptime\n");
    vga_print("  ls       - List files on disk\n");
    vga_print("  run <f>  - Execute a .bin or .exe file\n");
    vga_print("  color    - Test VGA colors\n");
    vga_print("  reboot   - Reboot system\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_cls(void) {
    vga_clear();
}

static void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        vga_print(argv[i]);
        if (i < argc - 1) vga_putchar(' ');
    }
    vga_putchar('\n');
}

static void cmd_info(void) {
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("MyOS v0.1  |  x86 32-bit Protected Mode\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("Bootloader  : GRUB Multiboot\n");
    vga_print("CPU Mode    : Protected Mode (Ring 0)\n");
    vga_print("VGA Mode    : Text 80x25\n");
    vga_print("Drivers     : PIT, PS/2 Keyboard, PS/2 Mouse\n");
    vga_print("Filesystem  : FAT12/FAT16\n");
    vga_print("Exec        : Flat binary (.bin), PE32 (.exe)\n");
}

static void cmd_mouse(void) {
    int32_t mx, my;
    uint8_t mb;
    mouse_get_state(&mx, &my, &mb);
    vga_print("Mouse position: X=");
    shell_print_dec((uint32_t)mx);
    vga_print(" Y=");
    shell_print_dec((uint32_t)my);
    vga_print("  Buttons: ");
    if (mb & 0x01) vga_print("[L] ");
    if (mb & 0x02) vga_print("[R] ");
    if (mb & 0x04) vga_print("[M] ");
    if (!mb)       vga_print("none");
    vga_putchar('\n');
}

static void cmd_time(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / 100;
    vga_print("Uptime: ");
    shell_print_dec(seconds / 3600); vga_print("h ");
    shell_print_dec((seconds % 3600) / 60); vga_print("m ");
    shell_print_dec(seconds % 60); vga_print("s  (");
    shell_print_dec(ticks); vga_print(" ticks)\n");
}

static void cmd_ls(void) {
    if (!fat_is_mounted()) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_print("Filesystem not mounted.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return;
    }

    fat_dir_entry_t entries[64];
    uint32_t count = fat_list_dir(entries, 64);

    if (count == 0) {
        vga_print("(empty)\n");
        return;
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("Name             Size    Attrs\n");
    vga_print("─────────────────────────────\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (uint32_t i = 0; i < count; i++) {
        fat_dir_entry_t* e = &entries[i];
        // Print name (8.3)
        char name[13];
        int ni = 0;
        for (int j = 0; j < 8 && e->name[j] != ' '; j++) name[ni++] = e->name[j];
        if (e->name[8] != ' ') {
            name[ni++] = '.';
            for (int j = 8; j < 11 && e->name[j] != ' '; j++) name[ni++] = e->name[j];
        }
        name[ni] = '\0';

        if (e->attrs & FAT_ATTR_SUBDIR) {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            vga_print("[DIR] ");
            vga_print(name);
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_print("      ");
            vga_print(name);
            // Pad to 16 chars
            for (int p = ni; p < 13; p++) vga_putchar(' ');
            shell_print_dec(e->file_size);
        }
        vga_putchar('\n');
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

static void cmd_run(int argc, char* argv[]) {
    if (argc < 2) {
        vga_print("Usage: run <filename>\n");
        return;
    }
    exec_result_t r = exec_load(argv[1]);
    if (r.error == EXEC_OK) {
        vga_print("[EXEC] Exited with code ");
        shell_print_dec((uint32_t)r.exit_code);
        vga_putchar('\n');
    }
}

static void cmd_color(void) {
    vga_print("VGA Color test:\n");
    for (int fg = 0; fg < 16; fg++) {
        vga_set_color((vga_color_t)fg, VGA_COLOR_BLACK);
        vga_print("Color ");
        shell_print_dec(fg);
        vga_print("  ");
    }
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_putchar('\n');
}

static void cmd_reboot(void) {
    vga_print("Rebooting...\n");
    // PS/2 controller reset line
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);
    // If that fails, use triple fault
    __asm__("cli; lidt [0]; int 0");
}

// ──────────────────────────── shell main loop ─────────────────────────────

void shell_init(void) {
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    vga_print("Shell ready. Type 'help' for commands.\n\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void shell_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("MyOS");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("> ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void shell_execute(char* line) {
    char* argv[MAX_ARGS];
    // Null-terminate and trim
    while (cmd_len > 0 && line[cmd_len-1] == ' ') line[--cmd_len] = '\0';
    if (cmd_len == 0) return;

    int argc = parse_args(line, argv, MAX_ARGS);
    if (argc == 0) return;

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    if (strcmp(argv[0], "help") == 0)        cmd_help();
    else if (strcmp(argv[0], "cls") == 0)    cmd_cls();
    else if (strcmp(argv[0], "clear") == 0)  cmd_cls();
    else if (strcmp(argv[0], "echo") == 0)   cmd_echo(argc, argv);
    else if (strcmp(argv[0], "info") == 0)   cmd_info();
    else if (strcmp(argv[0], "mouse") == 0)  cmd_mouse();
    else if (strcmp(argv[0], "time") == 0)   cmd_time();
    else if (strcmp(argv[0], "ls") == 0)     cmd_ls();
    else if (strcmp(argv[0], "dir") == 0)    cmd_ls();
    else if (strcmp(argv[0], "run") == 0)    cmd_run(argc, argv);
    else if (strcmp(argv[0], "color") == 0)  cmd_color();
    else if (strcmp(argv[0], "reboot") == 0) cmd_reboot();
    else {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_print("Unknown command: ");
        vga_print(argv[0]);
        vga_print("\nType 'help' for available commands.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

void shell_run(void) {
    cmd_len = 0;
    shell_prompt();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putchar('\n');
            cmd_buf[cmd_len] = '\0';
            shell_execute(cmd_buf);
            cmd_len = 0;
            shell_prompt();
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                vga_putchar('\b');
            }
        } else if (c >= 32 && cmd_len < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_len++] = c;
            vga_putchar(c);
        }
    }
}
