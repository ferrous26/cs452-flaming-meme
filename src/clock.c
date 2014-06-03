#include <irq.h>
#include <ts7200.h>

#include <clock.h>

//TODO: temp for testing, should be handled by some user task
uint ticks;

void clock_t4enable(void) {
    *(int*)(TIMER4_BASE | TIMER4_CONTROL) = 0x100;
}

uint clock_t4tick(void) {
    return *(uint*)(TIMER4_BASE | TIMER4_VALUE);
}

void clock_enable(void) {
    *(uint*)(TIMER3_BASE | LDR_OFFSET) = 5080;

    const uint ctrl = ENABLE_MASK | MODE_MASK | CLKSEL_MASK;
    *(uint*)(TIMER3_BASE | CRTL_OFFSET) = ctrl;

    register uint r0 asm("r0");
    *(uint*)(TIMER3_BASE | CLR_OFFSET) = r0;
}

void irq_clock() {
    register uint r0 asm("r0");
    *(uint*)(TIMER3_BASE | CLR_OFFSET) = r0;

    ticks++;
    vt_goto_home();
    kprintf("CLOCK: %d", ticks);
    vt_flush();
}

