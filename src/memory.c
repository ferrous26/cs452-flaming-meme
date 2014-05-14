#include <memory.h>

void memcpy(char* destaddr, char* srcaddr, size len) {
  // TODO: optimize with ldm and stm instructions
  while (len--) *destaddr++ = *srcaddr++;
}

int memcmp(void* left, void* right, size len) {

  size i = 0;

  for (; i < size; i++, left++, right++) {
    int diff = (int8)*left - (int8)*right;
    if (diff) return diff;
  }

  return 0;
}

bool streql(char* left, char* right) {

  while (*left) {
    if (*left != *right) return false;
    left++;
    right++;
  }

  return true;
}
