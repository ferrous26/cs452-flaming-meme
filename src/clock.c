#include <irq.h>
#include <ts7200.h>

#include <clock.h>
#include <scheduler.h>
#include <kernel.h>

#define TIMER3_TICKS_PER_OS_TICK 5080

void clock_t4enable(void) {
    *(int*)(TIMER4_BASE | TIMER4_CONTROL) = 0x100;
}

uint clock_t4tick(void) {
    return *(uint*)(TIMER4_BASE | TIMER4_VALUE);
}

void clock_enable(void) {
    *(uint*)(TIMER3_BASE | LDR_OFFSET) = TIMER3_TICKS_PER_OS_TICK;

    const uint ctrl = ENABLE_MASK | MODE_MASK | CLKSEL_MASK;
    *(uint*)(TIMER3_BASE | CRTL_OFFSET) = ctrl;

    register uint r0 asm("r0");
    *(uint*)(TIMER3_BASE | CLR_OFFSET) = r0;
}

void irq_clock() {
    register uint r0 asm("r0");
    *(uint*)(TIMER3_BASE | CLR_OFFSET) = r0;

    task* t = task_clock_event;
    task_clock_event = NULL;

    if (t) {
	t->sp[0] = 0;
	scheduler_schedule(t);
	return;
    }

    // TODO: turn this into an assert
    vt_log("Missed a clock tick");
    vt_flush();
}
