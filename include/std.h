#ifndef __STD_H__
#define __STD_H__

#include <types.h>

/* Signify code that runs once (or rarely) */
#define TEXT_COLD __attribute__ ((section (".text.cold")))
#define DATA_COLD __attribute__ ((section (".data.cold")))

#define FOREVER for (;;)

static inline int __attribute__ ((const))
abs(const int val) { return val < 0 ? -val : val; }

bool __attribute__ ((const)) isspace(const char c);
bool __attribute__ ((const)) isdigit(const char c);
bool __attribute__ ((const)) ishexdigit(const char c);
int  __attribute__ ((const)) log10(int c);
int  __attribute__ ((const)) ulog10(uint c);

#include <limits.h>
#include <memory.h>
#include <stdio.h>

// This is the code the has been used extensively by Prof. Buhr
// for CS246 and also CS343 since the actual distrubution of the
// randomness doesn't matter much for any of out purposes it should be fine
#define rand(seed) (seed = (36969 * (seed & 65535) + (seed >> 16)))

static inline uint __attribute__ ((const)) mod2(uint top, uint mod) {
    return (top & (mod - 1));
}

static inline int __attribute__ ((const)) mod2_int(int top, int mod) {
    return (top & (mod - 1));
}


#define MIN(x, y) ((x) > (y) ? (y): (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define BETWEEN(val, lo, hi)  ((val) >= (lo) && (val) <= (hi))
#define XBETWEEN(val, lo, hi) ((val) > (lo) && (val) < (hi))

#ifdef ASSERT
#define assert(expr, msg, ...)						\
    {									\
	if (!(expr))							\
	    Abort(__FILE__, __LINE__, msg, ##__VA_ARGS__);		\
    }

#else
#define assert(expr, msg, ...)
#endif

#endif
