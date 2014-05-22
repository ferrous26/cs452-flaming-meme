
#ifndef __MATH_H__
#define __MATH_H__

#include <debug.h>

static inline uint __attribute__ ((pure)) mod2(uint top, uint mod) {
    return (top & (mod - 1));
}

static inline uint __attribute__ ((pure)) lg(uint x) {

    uint r = 0;

    if (x & 0xFFFF0000) { r += 16; x >>= 16; }
    if (x & 0x0000FF00) { r +=  8; x >>=  8; }
    if (x & 0x000000F0) { r +=  4; x >>=  4; }

    switch (x) {
    case 0:
	return r;
    case 1:
	return r + 1;
    case 2:
    case 3:
	return r + 2;
    case 4:
    case 5:
    case 6:
    case 7:
	return r + 3;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
	return r + 4;
    };

    assert(false, "lg bug!");
    return 0;
}

static inline uint __attribute__ ((pure)) short_lg(uint x) {

    uint r = 0;

    if (x & 0x0000FF00) { r +=  8; x >>=  8; }
    if (x & 0x000000F0) { r +=  4; x >>=  4; }

    switch (x) {
    case 0:
	return r;
    case 1:
	return r + 1;
    case 2:
    case 3:
	return r + 2;
    case 4:
    case 5:
    case 6:
    case 7:
	return r + 3;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
	return r + 4;
    };

    assert(false, "short_lg bug!");
    return 0;
}

#endif
