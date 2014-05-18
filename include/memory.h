#include <std.h>

void memcpy(void* destaddr, const void* srcaddr, size len);
void memcpy4(void* destaddr, const void* srcaddr, size len);
void memcpy8(void* destaddr, const void* srcaddr, size len);
void memcpy16(void* destaddr, const void* srcaddr, size len);
void memcpy32(void* destaddr, const void* srcaddr, size len);

int memcmp(const void* left, const void* right, const size len);
int memcmp4(const void* left, const void* right, const size len);
