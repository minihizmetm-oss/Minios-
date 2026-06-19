#include "../include/types.h"

#define PIT_FREQ        1193182
#define PIT_CHANNEL0    0x40
#define PIT_COMMAND     0x43
#define TIMER_HZ        1000

static volatile uint64_t jiffies = 0;
static volatile uint64_t seconds = 0;

void timer_init(void) {
    uint32_t divisor = PIT_FREQ / TIMER_HZ;
    __outb(PIT_COMMAND, 0x36);
    __outb(PIT_CHANNEL0, divisor & 0xFF);
    __outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void timer_tick(void) {
    jiffies++;
    if(jiffies % TIMER_HZ == 0) seconds++;
    extern void scheduler_tick(void);
    scheduler_tick();
}

uint64_t get_jiffies(void) { return jiffies; }
uint64_t get_seconds(void) { return seconds; }

void sleep_ms(uint64_t ms) {
    uint64_t target = jiffies + ms;
    while(jiffies < target) __asm__ __volatile__("pause");
}
