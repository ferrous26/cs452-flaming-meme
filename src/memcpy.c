#include <memory.h>

static inline void slowcpy(char* dest, const char* src, size len) {
    while (len--)
	*dest++ = *src++;
}

void memcpy(void* dest, const void* src, size len) {

    /* if (((uint)dest & 0x3) || ((uint)src & 0x3)) { */
    /* 	slowcpy((char*)dest, (const char*)src, len); */
    /* 	return; */
    /* } */

    // need to perform log base 2 calculation to figure out the jump
    slowcpy((char*)dest, (const char*)src, len);

}
