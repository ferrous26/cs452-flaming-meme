
#ifndef __STD_H__
#define __STD_H__

#define FOREVER for (;;)

#define UNUSED(x) (void)(x)
#define NULL 0

typedef unsigned int bool;
#define true  1
#define false 0

typedef unsigned int   uint32;
typedef unsigned int   uint;
typedef signed   int   int32;
typedef unsigned short uint16;
typedef signed   short int16;
typedef unsigned char  uint8;
typedef          char  int8;

typedef void (*voidf)(void);

int  __attribute__ ((const)) abs(const int val);
bool __attribute__ ((const)) isspace(const char c);
bool __attribute__ ((const)) isdigit(const char c);
bool __attribute__ ((const)) ishexdigit(const char c);
int  __attribute__ ((const)) log10(int c); // naively calculate log base 10

#include <limits.h>
#include <memory.h>

// This is the code the has been used extensively by Prof. Buhr
// for CS246 and also CS343 since the actual distrubution of the
// randomness doesn't matter much for any of out purposes it should be fine
#define rand(seed) (seed = (36969 * (seed & 65535) + (seed >> 16)))

static inline uint __attribute__ ((const)) mod2(uint top, uint mod) {
    return (top & (mod - 1));
}



#endif
