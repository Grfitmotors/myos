// mouse.c - PS/2 Mouse Driver (IRQ 12)
// Supports standard 3-button PS/2 mouse with scroll wheel detection

#include "mouse.h"
#include "../kernel/kernel.h"
#include "../kernel/idt.h"
#include "../kernel/vga.h"

#define MOUSE_STATUS    0x64    // Controller status port
#define MOUSE_CMD       0x64    // Controller command port
#define MOUSE_DATA      0x60    // Data port

#define MOUSE_CMD_WRITE  0xD4   // Tell controller next byte goes to mouse
#define MOUSE_STATUS_IBF 0x02   // Input buffer full bit
#define MOUSE_STATUS_OBF 0x01   // Output buffer full bit
#define MOUSE_OUTBUF     0x20   // Mouse output buffer bit

// Mouse state
static volatile int32_t  mouse_x = 40;
static volatile int32_t  mouse_y = 12;
static volatile uint8_t  mouse_buttons = 0;
static volatile int8_t   mouse_dx = 0;
static volatile int8_t   mouse_dy = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];

// Screen limits
#define SCREEN_W 80
#define SCREEN_H 25

static void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout-- && (inb(MOUSE_STATUS) & MOUSE_STATUS_IBF));
}

static void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout-- && !(inb(MOUSE_STATUS) & MOUSE_STATUS_OBF));
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_CMD, MOUSE_CMD_WRITE); // Tell controller to send to mouse
    mouse_wait_write();
    outb(MOUSE_DATA, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA);
}

static uint8_t mouse_cmd(uint8_t cmd) {
    mouse_write(cmd);
    return mouse_read();
}

static void mouse_irq_handler(registers_t* regs) {
    // Read data byte
    if (!(inb(MOUSE_STATUS) & MOUSE_OUTBUF)) return;
    
    uint8_t data = inb(MOUSE_DATA);
    mouse_bytes[mouse_cycle] = data;

    switch (mouse_cycle) {
        case 0:
            // First byte: flags - bit 3 must be set (always 1)
            if (!(data & 0x08)) return; // Invalid packet, stay at 0
            mouse_cycle = 1;
            break;
        case 1:
            mouse_cycle = 2;
            break;
        case 2: {
            mouse_cycle = 0;

            // Parse mouse packet
            uint8_t flags = mouse_bytes[0];
            int8_t  raw_dx = (int8_t)mouse_bytes[1];
            int8_t  raw_dy = (int8_t)mouse_bytes[2];

            // Handle X sign bit (bit 4)
            int32_t dx = raw_dx;
            // Handle Y sign bit (bit 5) - Y is inverted on PS/2
            int32_t dy = -raw_dy;

            // Overflow flags - discard packet
            if ((flags & 0x40) || (flags & 0x80)) break;

            mouse_buttons = flags & 0x07;
            mouse_dx = (int8_t)dx;
            mouse_dy = (int8_t)dy;

            // Update position with bounds checking
            mouse_x += dx / 2;  // Divide for finer control in text mode
            mouse_y += dy / 2;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
            if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;

            // Draw mouse cursor on screen
            vga_draw_mouse((uint32_t)mouse_x, (uint32_t)mouse_y);
            break;
        }
    }
}

void mouse_init(void) {
    // Enable PS/2 mouse port
    mouse_wait_write();
    outb(MOUSE_CMD, 0xA8);  // Enable aux mouse device

    // Enable interrupts for mouse
    mouse_wait_write();
    outb(MOUSE_CMD, 0x20);  // Read current controller config
    mouse_wait_read();
    uint8_t status = inb(MOUSE_DATA);
    status |= 0x02;         // Enable IRQ12 (mouse interrupt bit)
    status &= ~0x20;        // Enable mouse clock
    mouse_wait_write();
    outb(MOUSE_CMD, 0x60);  // Write controller config
    mouse_wait_write();
    outb(MOUSE_DATA, status);

    // Reset mouse
    uint8_t ack = mouse_cmd(0xFF);
    if (ack == 0xFA) {
        mouse_read(); // Read BAT result (0xAA)
        mouse_read(); // Device ID (0x00 for standard mouse)
    }

    // Set defaults
    mouse_cmd(0xF6);  // Set defaults

    // Set sample rate to 100/sec
    mouse_cmd(0xF3);
    mouse_cmd(100);

    // Set resolution: 4 counts/mm
    mouse_cmd(0xE8);
    mouse_cmd(0x02);

    // Enable data reporting
    mouse_cmd(0xF4);

    // Install IRQ handler
    irq_install_handler(12, mouse_irq_handler);
    irq_clear_mask(12);
    irq_clear_mask(2);  // Must also unmask cascade IRQ2

    // Initial cursor draw
    vga_draw_mouse(mouse_x, mouse_y);
}

void mouse_get_state(int32_t* x, int32_t* y, uint8_t* buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}
