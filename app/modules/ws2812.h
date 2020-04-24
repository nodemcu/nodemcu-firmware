#ifndef APP_MODULES_WS2812_H_
#define APP_MODULES_WS2812_H_

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "user_interface.h"
#include "driver/uart.h"
#include "osapi.h"

#define FADE_IN  1
#define FADE_OUT 0
#define SHIFT_LOGICAL  0
#define SHIFT_CIRCULAR 1

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

typedef struct {
  int size;
  uint8_t colorsPerLed;
  uint8_t values[0];
} ws2812_buffer;

typedef struct {
  size_t offset;
  uint8_t* tmp_pixels;
  int shiftValue;
  size_t shift_len;
  size_t remaining_len;
  unsigned shift_type;
  ws2812_buffer* buffer;
} ws2812_buffer_shift_prepare;


void ICACHE_RAM_ATTR ws2812_write_data(const uint8_t *pixels, uint32_t length, const uint8_t *pixels2, uint32_t length2);
// To shift the lua_State is needed for error message and memory allocation.
// We also need the shift operation inside a timer callback, where we cannot access the lua_State,
// so This is split up in prepare and the actual call, which can be called multiple times with the same prepare object.
// After being done just luaM_free on the prepare object.
void ws2812_buffer_shift_prepared(ws2812_buffer_shift_prepare* prepare);
ws2812_buffer_shift_prepare* ws2812_buffer_get_shift_prepare(lua_State* L, ws2812_buffer * buffer, int shiftValue, unsigned shift_type, int pos_start, int pos_end);

int ws2812_buffer_fill(ws2812_buffer * buffer, int * colors);
void ws2812_buffer_fade(ws2812_buffer * buffer, int fade, unsigned direction);

#endif /* APP_MODULES_WS2812_H_ */
