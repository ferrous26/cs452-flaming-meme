
#include <clock.h>
#include <irq.h>
#include <ts7200.h>
#include <debug.h>
#include <kernel.h>


#define TIMER3_TICKS_PER_OS_TICK 5080

void clock_t4enable(void) {
    *(int*)(TIMER4_BASE | TIMER4_CONTROL) = 0x100;
}

uint clock_t4tick(void) {
    return *(uint*)(TIMER4_BASE | TIMER4_VALUE);
}

void clock_init() {
    clock_deinit();

    *(uint*)(TIMER3_BASE | LDR_OFFSET) = TIMER3_TICKS_PER_OS_TICK;

    const uint ctrl = ENABLE_MASK | MODE_MASK | CLKSEL_MASK;
    *(uint*)(TIMER3_BASE | CRTL_OFFSET) = ctrl;

    const uint clear_addr = TIMER3_BASE | CLR_OFFSET;
    *(uint*)clear_addr = clear_addr;
}

void clock_deinit() {
    *(uint*)(TIMER3_BASE | CRTL_OFFSET) = 0;
}

void irq_clock() {
    const uint clear_addr = TIMER3_BASE | CLR_OFFSET;
    *(uint*)clear_addr = clear_addr;

    task* t = int_queue[CLOCK_TICK];
    int_queue[CLOCK_TICK] = NULL;

    if (t) {
	t->sp[0] = 0;
	scheduler_reschedule(t);
	return;
    }

    assert(t, "Missed a clock tick");
}
