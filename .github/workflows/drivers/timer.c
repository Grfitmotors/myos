// timer.c - PIT (Programmable Interval Timer) Driver

#include "timer.h"
#include "../kernel/kernel.h"
#include "../kernel/idt.h"

#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL2    0x42
#define PIT_CMD         0x43
#define PIT_BASE_FREQ   1193180

static volatile uint32_t tick_count = 0;

static void timer_irq_handler(registers_t* regs) {
    tick_count++;
}

void timer_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQ / hz;

    // Channel 0, lobyte/hibyte, square wave mode
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    irq_install_handler(0, timer_irq_handler);
    irq_clear_mask(0);
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_sleep(uint32_t ms) {
    uint32_t end = tick_count + ms / 10; // 100Hz = 10ms per tick
    while (tick_count < end) __asm__("hlt");
}
