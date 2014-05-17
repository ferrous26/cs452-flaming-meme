#include <std.h>

void memcpy(void* destaddr, const void* srcaddr, size len);

int memcmp(const void* left, const void* right, const size len);

/**
 * Compare two strings and return whether or not they are equal.
 */
bool streql(const char* left, const char* right);
