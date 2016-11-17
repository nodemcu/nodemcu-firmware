
#include "c_string.h"
#include "memeq.h"

int memeq(const char *a, const char *b, size_t l) {
  if (((uint32_t) a | (uint32_t) b) & 3) {
    return memcmp(a, b, l);
  }

  const uint32_t *a32 = (const uint32_t *) a;
  const uint32_t *b32 = (const uint32_t *) b;

  while (l >= 16) {
    int res = (*a32++ ^ *b32++);
    res = res | (*a32++ ^ *b32++);
    res = res | (*a32++ ^ *b32++);
    res = res | (*a32++ ^ *b32++);
    if (res) {
      return 1;
    }
    l -= 16;
  }

  if (l >= 8) {
    int res = (*a32++ ^ *b32++);
    res = res | (*a32++ ^ *b32++);
    if (res) {
      return 1;
    }
    l -= 8;
  }

  if (l >= 4) {
    int res = (*a32++ ^ *b32++);
    if (res) {
      return 1;
    }
    l -= 4;
  }

  if (!l) {
    return 0;
  }

  // l is 1,2,3
  int res = *a32 ^ *b32;
  // Processor is little endian
  res = res << ((4 - l) << 3);

  if (res) {
    return 1;
  }

  return 0;
}
