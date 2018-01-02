#ifndef APP_MODULES_WS2812_H_
#define APP_MODULES_WS2812_H_

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_math.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "osapi.h"

#define FADE_IN  1
#define FADE_OUT 0
#define SHIFT_LOGICAL  0
#define SHIFT_CIRCULAR 1


typedef struct {
  int size;
  uint8_t colorsPerLed;
  uint8_t values[0];
} ws2812_buffer;


void ICACHE_RAM_ATTR ws2812_write_data(const uint8_t *pixels, uint32_t length, const uint8_t *pixels2, uint32_t length2);
int ws2812_buffer_shift(ws2812_buffer * buffer, int shiftValue, unsigned shift_type, int pos_start, int pos_end);
int ws2812_buffer_fill(ws2812_buffer * buffer, int * colors);

/*

void wifi_add_sprintf_field(lua_State* L, char* name, char* string, ...);
void wifi_add_int_field(lua_State* L, char* name, lua_Integer integer);

static inline void register_lua_cb(lua_State* L,int* cb_ref)
{
  int ref=luaL_ref(L, LUA_REGISTRYINDEX);
  if( *cb_ref != LUA_NOREF)
  {
	luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  }
  *cb_ref = ref;
}

static inline void unregister_lua_cb(lua_State* L, int* cb_ref)
{
  if(*cb_ref != LUA_NOREF)
  {
	luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  	*cb_ref = LUA_NOREF;
  }
}

void wifi_change_default_host_name(void);

*/

#endif /* APP_MODULES_WS2812_H_ */
