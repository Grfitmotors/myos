// vga.c - VGA Text Mode Driver (80x25)

#include "vga.h"
#include "kernel.h"

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

static uint16_t* vga_buf = (uint16_t*)VGA_MEMORY;
static uint8_t   vga_color = 0;
static uint32_t  vga_col = 0;
static uint32_t  vga_row = 0;

static inline uint16_t vga_entry(uint8_t c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void vga_update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_scroll(void) {
    uint16_t blank = vga_entry(' ', vga_color);

    // Move everything up one row
    for (uint32_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga_buf[i] = vga_buf[i + VGA_WIDTH];
    }
    // Clear last row
    for (uint32_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga_buf[i] = blank;
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_init(void) {
    vga_color = vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_col = 0;
    vga_row = 0;
    
    // Enable cursor (scan lines 14-15)
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void vga_clear(void) {
    uint16_t blank = vga_entry(' ', vga_color);
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buf[i] = blank;
    }
    vga_col = 0;
    vga_row = 0;
    vga_update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = vga_make_color(fg, bg);
}

uint8_t vga_make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
        }
    } else {
        vga_buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color);
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }

    if (vga_row >= VGA_HEIGHT)
        vga_scroll();

    vga_update_cursor();
}

void vga_print(const char* str) {
    while (*str) vga_putchar(*str++);
}

void vga_print_hex(uint32_t val) {
    const char* hex = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[9 - i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
    vga_print(buf);
}

void vga_print_dec(uint32_t val) {
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    vga_print(&buf[i + 1]);
}

void vga_set_cursor(uint32_t col, uint32_t row) {
    vga_col = col;
    vga_row = row;
    vga_update_cursor();
}

void vga_get_cursor(uint32_t* col, uint32_t* row) {
    *col = vga_col;
    *row = vga_row;
}

// Draw mouse cursor at position (x,y) on VGA text screen
// Uses a small character as "cursor"
static uint8_t mouse_saved_char = ' ';
static uint8_t mouse_saved_color = 0;
static uint32_t mouse_last_x = 0;
static uint32_t mouse_last_y = 0;
static bool mouse_drawn = false;

void vga_draw_mouse(uint32_t x, uint32_t y) {
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;

    // Restore previous position
    if (mouse_drawn) {
        uint32_t idx = mouse_last_y * VGA_WIDTH + mouse_last_x;
        vga_buf[idx] = vga_entry(mouse_saved_char, mouse_saved_color);
    }

    // Save & draw at new position
    uint32_t idx = y * VGA_WIDTH + x;
    mouse_saved_char  = vga_buf[idx] & 0xFF;
    mouse_saved_color = (vga_buf[idx] >> 8) & 0xFF;
    vga_buf[idx] = vga_entry(0xDB, vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    mouse_last_x = x;
    mouse_last_y = y;
    mouse_drawn = true;
}
