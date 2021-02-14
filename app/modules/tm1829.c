#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include "user_interface.h"

#include "pixbuf.h"

static inline uint32_t _getCycleCount(void) {
  uint32_t cycles;
  __asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
  return cycles;
}

// This algorithm reads the cpu clock cycles to calculate the correct
// pulse widths. It works in both 80 and 160 MHz mode.
static void ICACHE_RAM_ATTR tm1829_write_to_pin(uint8_t pin, const uint8_t *pixels, size_t length) {
  const uint8_t *p, *end;
  uint8_t phasergb = 0;

  p   =  pixels;
  end =  p + length;

  const uint32_t t0l  = (1000 * system_get_cpu_freq()) / 3333;  // 0.390us (spec=0.35 +- 0.15)
  const uint32_t t1l  = (1000 * system_get_cpu_freq()) / 1250;  // 0.800us (spec=0.70 +- 0.15)

  const uint32_t ttot = (1000 * system_get_cpu_freq()) / 800;   // 1.25us

  while (p != end) {
    register int i;

    register uint8_t pixel = *p++;
    if ((phasergb == 0) && (pixel == 0xFF)) {
      // clamp initial byte value to avoid constant-current shenanigans.  Yuck!
      pixel = 0xFE;
    }
    if (++phasergb == 3) {
      phasergb = 0;
    }

    ets_intr_lock();

    for (i = 7; i >= 0; i--) {
       register uint32_t pin_mask = 1 << pin;

       // Select low time
       register uint32_t tl = ((pixel >> i) & 1) ? t1l : t0l;
       register uint32_t t1, t2;

       register uint32_t t = _getCycleCount();

       t1 = t + tl;
       t2 = t + ttot;

       GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask); // Set pin low
       while (_getCycleCount() < t1);

       GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask); // Set pin high
       while (_getCycleCount() < t2);
    }

    ets_intr_unlock();
  }
}

// Lua: tm1829.write(pin, "string")
// Byte triples in the string are interpreted as GRB values.
static int ICACHE_FLASH_ATTR tm1829_write(lua_State* L)
{
  const uint8_t pin = luaL_checkinteger(L, 1);
  const uint8_t *pixels;
  size_t length;

  switch(lua_type(L, 3)) {
  case LUA_TSTRING: {
    pixels = luaL_checklstring(L, 2, &length);
    break;
   }
#ifdef LUA_USE_MODULES_PIXBUF      
  case LUA_TUSERDATA: {
    pixbuf *buffer = pixbuf_from_lua_arg(L, 2);
    luaL_argcheck(L, pixbuf_channels(buffer) == 3, 2, "Bad pixbuf format");
    pixels = buffer->values;
    length = 3 * buffer->npix;
    break;
   }
#endif
  default:
    return luaL_argerror(L, 2, "String or pixbuf expected");
  }

  // Initialize the output pin and wait a bit
  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 1);

  // Send the buffer
  tm1829_write_to_pin(pin_num[pin], pixels, length);

  os_delay_us(500); // reset time

  return 0;
}

LROT_BEGIN(tm1829, NULL, 0)
  LROT_FUNCENTRY( write, tm1829_write )
LROT_END(tm1829, NULL, 0)


int luaopen_tm1829(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  return 0;
}

NODEMCU_MODULE(TM1829, "tm1829", tm1829, luaopen_tm1829);
