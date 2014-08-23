#ifndef __TYPES_H__
#define __TYPES_H__

#define NULL 0
#define UNUSED(x) (void)(x)


typedef unsigned int bool;
#define true  1
#define false 0

// TODO: these are architecture specific, so should be in another file
typedef unsigned int   uint32;
typedef unsigned int   uint;
typedef signed   int   int32;
typedef unsigned short uint16;
typedef signed   short int16;
typedef unsigned char  uint8;
typedef          char  int8;

typedef uint32 size_t;
typedef int32  ssize_t;

typedef void (*voidf)(void);

typedef int task_id;
typedef uint task_priority;

#endif
