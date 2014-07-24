#ifndef __CLZ_H__
#define __CLZ_H__

#include <std.h>

// This table will be initialized by the kernel, so it should be a non-issue
// for user land tasks to make use of
extern uint8 table[256];

// This algorithm is borrowed from
// http://graphics.stanford.edu/%7Eseander/bithacks.html#IntegerLogLookup
static inline uint32 __attribute__ ((pure)) choose_priority(const uint32 v) {
    uint32 result;
    uint  t;
    uint  tt;

    if ((tt = v >> 16))
        result = (t = tt >> 8) ? 24 + table[t] : 16 + table[tt];
    else
        result = (t = v >> 8) ? 8 + table[t] : table[v];

    return result;
}

#endif
