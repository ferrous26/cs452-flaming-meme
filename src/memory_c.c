#include <memory.h>

int memcmp(const void* left, const void* right, const size len) {

    size i = 0;

    for (; i < len; i++, left++, right++) {
	int diff = *(int8*)left - *(int*)right;
	if (diff) return diff;
    }

    return 0;
}

bool streql(const char* left, const char* right) {

    while (*left) {
	if (*left != *right) return false;
	left++;
	right++;
    }

    return true;
}
