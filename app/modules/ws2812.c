#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
/**
 * All this code is mostly from http://www.esp8266.com/viewtopic.php?f=21&t=1143&sid=a620a377672cfe9f666d672398415fcb
 * from user Markus Gritsch.
 * I just put this code into its own module and pushed into a forked repo,
 * to easily create a pull request. Thanks to Markus Gritsch for the code.
 *
 */

// ----------------------------------------------------------------------------
// -- This WS2812 code must be compiled with -O2 to get the timing right.
// -- http://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
// The ICACHE_FLASH_ATTR is there to trick the compiler and get the very first pulse width correct.
static void ICACHE_FLASH_ATTR send_ws_0(uint8_t gpio) {
  uint8_t i;
  i = 4;
  while (i--)
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 9;
  while (i--)
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

static void ICACHE_FLASH_ATTR send_ws_1(uint8_t gpio) {
  uint8_t i;
  i = 8;
  while (i--)
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 6;
  while (i--)
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_write(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);
  os_delay_us(10);

  os_intr_lock();
  const char * const end = buffer + length;
  while (buffer != end) {
    uint8_t mask = 0x80;
    while (mask) {
      (*buffer & mask) ? send_ws_1(pin_num[pin]) : send_ws_0(pin_num[pin]);
      mask >>= 1;
    }
    ++buffer;
  }
  os_intr_unlock();

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_write )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}

// ----------------------------------------------------------------------------

