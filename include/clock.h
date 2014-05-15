#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <std.h>
#include <ts7200.h>

inline void clock_t4enable(void) {
    *(int*)(TIMER4_BASE + TIMER4_CONTROL) = 0x100;
}

inline uint clock_t4tick(void) {
    return *(uint*)(TIMER4_BASE + TIMER4_VALUE);
}

#endif
