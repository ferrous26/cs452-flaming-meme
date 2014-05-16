#ifndef __STD_H__
#define __STD_H__

#include <limits.h>

#define UNUSED(x) (void)(x)

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

typedef uint32 size;
typedef int32  ssize;

int abs(const int val);
bool isspace(const char c);
bool isdigit(const char c);
bool ishexdigit(const char c);

#endif
