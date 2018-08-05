#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef  int32_t s32_t;
typedef uint32_t u32_t;
typedef  int16_t s16_t;
typedef uint16_t u16_t;
typedef   int8_t s8_t;
typedef  uint8_t u8_t;

#ifndef __CYGWIN__
typedef long long ptrdiff_t;
#define offsetof(type, member)  __builtin_offsetof (type, member)
#endif
