
#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <std.h>

void memcpy(void* destaddr, const void* srcaddr, int len);
void memset(void *buffer, int c, int len);

//int memcmp(const void* left, const void* right, const size len);

#endif
