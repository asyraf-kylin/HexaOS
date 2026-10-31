/* timer.c - PIT kanal 0 sebagai detak sistem (IRQ0).
   kdelay_ms() di klib.c memakai kanal 2, jadi tidak bentrok. */
#include "kernel.h"
#include "task.h"

#define PIT_BASE_HZ 1193182u

static volatile uint32_t ticks = 0;
static timer_cb_t        callback = 0;

static void timer_irq(registers_t* r) {
    ticks++;
    if (callback) callback();
}

void timer_init(uint32_t hz, timer_cb_t cb) {
    uint32_t divisor = PIT_BASE_HZ / hz;
    callback = cb;
    ticks = 0;
    outb(0x43, 0x36);                       /* kanal 0, lo/hi byte, mode 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)(divisor >> 8));
    irq_register(0, timer_irq);             /* membuka IRQ0 di PIC */
}

uint32_t timer_ticks(void) {
    return ticks;
}