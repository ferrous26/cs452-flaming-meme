#ifndef __STD_H__
#define __STD_H__

#define UNUSED(x) (void)(x)

typedef unsigned int bool;
#define true  1
#define false 0

typedef unsigned int   uint32;
typedef signed   int   int32;
typedef unsigned short uint16;
typedef signed   short int16;
typedef unsigned char  uint8;
typedef          char  int8;

#define UINT_MAX 0xffffffff
#define  INT_MAX 0xefffffff

#define SIZE_MAX 0xffffffff
typedef uint32 size;
typedef int32  ssize;

#endif
