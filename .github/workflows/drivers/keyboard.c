// keyboard.c - PS/2 Keyboard Driver

#include "keyboard.h"
#include "../kernel/kernel.h"
#include "../kernel/idt.h"
#include "../kernel/vga.h"

#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_CMD_PORT     0x64

// Keyboard buffer
#define KB_BUFFER_SIZE 256
static char kb_buffer[KB_BUFFER_SIZE];
static volatile uint32_t kb_buf_head = 0;
static volatile uint32_t kb_buf_tail = 0;

static bool shift_pressed = false;
static bool caps_lock = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;

// US QWERTY scancode set 1
static const char scancode_table[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',   // 0x00
    '7', '8', '9', '0', '-', '=',  '\b', '\t',  // 0x08
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',    // 0x10
    'o', 'p', '[', ']', '\n', 0,  'a', 's',     // 0x18
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',    // 0x20
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',   // 0x28
    'b', 'n', 'm', ',', '.', '/', 0,  '*',      // 0x30
    0,   ' ', 0,   0,   0,   0,   0,   0,       // 0x38
};

static const char scancode_shift[] = {
    0,    0,   '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,  '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
};

static void keyboard_irq_handler(registers_t* regs) {
    uint8_t scancode = inb(KB_DATA_PORT);
    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F;

    // Handle special keys
    switch (scancode) {
        case 0x2A: case 0x36:  // Left/Right Shift
            shift_pressed = !released;
            return;
        case 0x1D:              // Ctrl
            ctrl_pressed = !released;
            return;
        case 0x38:              // Alt
            alt_pressed = !released;
            return;
        case 0x3A:              // Caps Lock
            if (!released) caps_lock = !caps_lock;
            return;
    }

    if (released) return;

    // Get character
    char c = 0;
    if (scancode < sizeof(scancode_table)) {
        bool upper = shift_pressed ^ caps_lock;
        c = upper ? scancode_shift[scancode] : scancode_table[scancode];
    }

    if (c == 0) return;

    // Buffer the character
    uint32_t next = (kb_buf_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_buf_tail) {
        kb_buffer[kb_buf_head] = c;
        kb_buf_head = next;
    }
}

void keyboard_init(void) {
    // Wait for controller to be ready
    while (inb(KB_STATUS_PORT) & 0x02);

    // Enable keyboard (clear any pending data)
    inb(KB_DATA_PORT);

    irq_install_handler(1, keyboard_irq_handler);
    irq_clear_mask(1);
}

bool keyboard_has_key(void) {
    return kb_buf_head != kb_buf_tail;
}

char keyboard_getchar(void) {
    while (!keyboard_has_key());
    char c = kb_buffer[kb_buf_tail];
    kb_buf_tail = (kb_buf_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

char keyboard_try_getchar(void) {
    if (!keyboard_has_key()) return 0;
    char c = kb_buffer[kb_buf_tail];
    kb_buf_tail = (kb_buf_tail + 1) % KB_BUFFER_SIZE;
    return c;
}
