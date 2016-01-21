#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"

static inline uint32_t _getCycleCount(void) {
  uint32_t cycles;
  __asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
  return cycles;
}

// This algorithm reads the cpu clock cycles to calculate the correct
// pulse widths. It works in both 80 and 160 MHz mode.
// The values for t0h, t1h, ttot have been tweaked and it doesn't get faster than this.
// The datasheet is confusing and one might think that a shorter pulse time can be achieved.
// The period has to be at least 1.25us, even if the datasheet says:
//   T0H: 0.35 (+- 0.15) + T0L: 0.8 (+- 0.15), which is 0.85<->1.45 us.
//   T1H: 0.70 (+- 0.15) + T1L: 0.6 (+- 0.15), which is 1.00<->1.60 us.
// Anything lower than 1.25us will glitch in the long run.
static void ICACHE_RAM_ATTR ws2812_write(uint8_t pin, uint8_t *pixels, uint32_t length) {
  uint8_t *p, *end, pixel, mask;
  uint32_t t, t0h, t1h, ttot, c, start_time, pin_mask;

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // from now on, use translated pin number.
  pin = pin_num[pin];

  // Send the buffer
  ets_intr_lock();

  pin_mask = 1 << pin;
  p =  pixels;
  end =  p + length;
  pixel = *p++;
  mask = 0x80;
  start_time = 0;
  t0h  = (1000 * system_get_cpu_freq()) / 3022;  // 0.35us (spec=0.35 +- 0.15)
  t1h  = (1000 * system_get_cpu_freq()) / 1477;  // 0.70us (spec=0.70 +- 0.15)
  ttot = (1000 * system_get_cpu_freq()) /  800;  // 1.25us (MUST be >= 1.25)

  while (true) {
    if (pixel & mask) {
        t = t1h;
    } else {
        t = t0h;
    }
    while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask);      // Set pin high
    start_time = c;                                       // Save the start time
    while (((c = _getCycleCount()) - start_time) < t);    // Wait for high time to finish
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask);      // Set pin low
    if (!(mask >>= 1)) {                                  // Next bit/byte
      if (p >= end) {
        break;
      }
      pixel= *p++;
      mask = 0x80;
    }
  }
  ets_intr_unlock();
}

// Lua: ws2812.writergb(pin, "string")
// Byte triples in the string are interpreted as R G B values and sent to the hardware as G R B.
// WARNING: this function scrambles the input buffer :
//    a = string.char(255,0,128)
//    ws212.writergb(3,a)
//    =a.byte()
//    (0,255,128)

// ws2812.writergb(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// ws2812.writergb(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.writergb(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writergb(lua_State* L)
{
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *rgb = luaL_checklstring(L, 2, &length);

  // dont modify lua-internal lstring - make a copy instead
  char *buffer = (char *)c_malloc(length);
  c_memcpy(buffer, rgb, length);

  // Ignore incomplete Byte triples at the end of buffer:
  length -= length % 3;

  // Rearrange R G B values to G R B order needed by WS2812 LEDs:
  size_t i;
  for (i = 0; i < length; i += 3) {
    const char r = buffer[i];
    const char g = buffer[i + 1];
    buffer[i] = g;
    buffer[i + 1] = r;
  }

  ws2812_write(pin, (uint8_t*) buffer, length);

  c_free(buffer);

  return 0;
}

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// This function does not corrupt your buffer.
//
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writegrb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  ws2812_write(pin, (uint8_t*) buffer, length);

  return 0;
}

// Lua: ws2812.write_table_rgb(pin, table)
// Each element of table is table with r,g,b elements at indexes 1,2,3
// Table is not modified.
// Returns number of leds written.
//
// ws2812.write(4, {{255,0,0}, {0,255,0}}) uses GPIO2 and sets the first LED red, second LED green.

// Map that convert rgb to grb. At this moment mostly useless,
// used to make it easier for person who decides to add grb tables.
// I assume compiler is able to optimize it out.
const static int map_rgb[3] = {2,1,3};

static int ICACHE_FLASH_ATTR ws2812_write_table_rgb(lua_State* L) {

    // Stack: 1 pin 2 leds
    lua_settop(L, 2);

    const uint8_t pin = luaL_checkinteger(L, 1);

    luaL_checktype(L, 2, LUA_TTABLE);
    size_t length = lua_objlen(L, 2);

    uint8_t * buffer = (uint8_t *)c_malloc(3 * length);
    uint8_t * b = buffer;

    int i;

    for(i = 1; i <= length; i++) {
        lua_rawgeti(L, 2, i);
        luaL_checktype(L, -1, LUA_TTABLE);
        // Stack: -1 led 1 pin 2 leds

        // The following could be done in a single loop.
        // All this is basically just to save three lua_pop calls
        // so there's probably not much point to it.
        // but you know, inner loop and stuff... 8)

        int c;
        for(c = 0; c < 3 ; c++) {
            // led sinks down in the stack as we get individual components
            // thus we must recompute index every time
            lua_rawgeti(L, -1 - c, map_rgb[c]);
        }

        // Stack now:
        // -4 led -3 green, -2 red, -1 blue 1 pin 2 leds

        for(c = -3; c <= -1; c++) {
            *(b++) = (uint8_t)luaL_checkinteger(L, c);
        }
        lua_pop(L, 4);
    }

    ws2812_write(pin, buffer, 3*length);

    c_free(buffer);
    lua_pushnumber(L, length);
    return 1;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "writergb" ), LFUNCVAL( ws2812_writergb )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LSTRKEY( "write_table_rgb" ), LFUNCVAL(ws2812_write_table_rgb)},
  { LNILKEY, LNILVAL}
};

int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(WS2812, "ws2812", ws2812_map, luaopen_ws2812);
