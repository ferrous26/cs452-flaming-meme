
#ifndef __MATH_H__
#define __MATH_H__

#include <std.h>

static inline uint __attribute__ ((const)) mod2(uint top, uint mod) {
    return (top & (mod - 1));
}

#endif
