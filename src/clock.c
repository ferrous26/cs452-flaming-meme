#include <clock.h>

void clock_t4enable(void) {
    *(int*)(TIMER4_BASE + TIMER4_CONTROL) = 0x100;
}

uint clock_t4tick(void) {
    return *(uint*)(TIMER4_BASE + TIMER4_VALUE);
}
