#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "c_math.h"

#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_stdarg.h"
#include "c_string.h"

#include "strbuf.h"

/**
 * All this code is mostly from http://www.esp8266.com/viewtopic.php?f=21&t=1143&sid=a620a377672cfe9f666d672398415fcb
 * from user Markus Gritsch.
 * I just put this code into its own module and pushed into a forked repo,
 * to easily create a pull request. Thanks to Markus Gritsch for the code.
 */

// ----------------------------------------------------------------------------
// -- This WS2812 code must be compiled with -O2 to get the timing right.  Read this:
// -- http://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
// -- The ICACHE_FLASH_ATTR is there to trick the compiler and get the very first pulse width correct.
static void ICACHE_FLASH_ATTR __attribute__((optimize("O2"))) send_ws_0(uint8_t gpio){
  uint8_t i;
  i = 4; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 9; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

static void ICACHE_FLASH_ATTR __attribute__((optimize("O2"))) send_ws_1(uint8_t gpio){
  uint8_t i;
  i = 8; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 6; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

// brightness may be between 0 and 1
#define MAX_BRIGHTNESS 1

// Brightness is a floating-point number which will be multiplied before
// setting an LED
static lua_Number brightness = MAX_BRIGHTNESS;


// Lua: ws2812.set_brightness(brightness)
// 
// brightness must be between 0 and MAX_BRIGHTNESS
static int ICACHE_FLASH_ATTR set_brightness(lua_State* L){
  brightness = luaL_checknumber(L,1);
  return 0;
}

// Lua: brightness =ws2812.brightness()
// returns the current brightness
static int ICACHE_FLASH_ATTR get_brightness(lua_State* L){
  lua_pushnumber(L,brightness);
  return 1;
}

// Lua: R,G,B = ws2812.hsv2rgb(H,S,V)
// H: Hue, S: Saturation, V: Value
// R,G,B are on [0,255]
//
// H is given on [0, 1]. S and V are given on [0, 1].
// This function does not corrupt your buffer
//
//  modified from Alvy Ray Smith's site:
//  http://www.alvyray.com/Papers/hsv2rgb.htm
static int ICACHE_FLASH_ATTR ws2812_hsv2rgb(lua_State* L) {
  uint8_t i;
  lua_Number m, n, f;
  const lua_Number h = luaL_checknumber(L,1) * 6;
  const lua_Number s = luaL_checknumber(L,2);
  lua_Number v = luaL_checknumber(L,3);

  // not very elegant way of dealing with out of range: return black
  if ((s<0.0) || (s>1.0) || (v<0.0) || (v>1.0)) {
    NODE_ERR("S or V out of range\n");
    lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
    return 3;
  }
  v = v * 255;
  if ((h < 0.0) || (h > 6.0)) {
    lua_pushnumber(L, v); lua_pushnumber(L, v); lua_pushnumber(L, v);
    return 3;
  }
  i = floor(h);
  f = h - i;
  if ( !(i&1) ) {
    f = 1 - f; // if i is even
  }
  m = v * (1 - s) ;
  n = v * (1 - s * f) ;
  switch (i) {
  case 6:
  case 0: // RETURN_RGB(v, n, m)
    lua_pushnumber(L, v ); lua_pushnumber(L, n ); lua_pushnumber(L, m );break;
  case 1: // RETURN_RGB(n, v, m) 
    lua_pushnumber(L, n ); lua_pushnumber(L, v ); lua_pushnumber(L, m );break;
  case 2:  // RETURN_RGB(m, v, n)
    lua_pushnumber(L, m ); lua_pushnumber(L, v ); lua_pushnumber(L, n );break;
  case 3:  // RETURN_RGB(m, n, v)
    lua_pushnumber(L, m ); lua_pushnumber(L, n ); lua_pushnumber(L, v );break;
  case 4:  // RETURN_RGB(n, m, v)
    lua_pushnumber(L, n ); lua_pushnumber(L, m ); lua_pushnumber(L, v );break;
  case 5:  // RETURN_RGB(v, m, n)
    lua_pushnumber(L, v ); lua_pushnumber(L, m ); lua_pushnumber(L, n );break;
  }
  return 3;
} 

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as R G B values.
// This function does not corrupt your buffer
//
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writergb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);
  // ignore incomplete byte triples for re-arranging
  length -= length % 3;
  char *transfer = (char*)c_malloc(length);

  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // add brightness
  size_t i;
  for (i = 0; i < length; i ++) {
    transfer[i] = buffer[i]*brightness;
  }

  // transfer from grb to rgb
  for (i = 0; i < length; i += 3) {
    const char r = transfer[i];
    const char g = transfer[i + 1];
    transfer[i] = g;
    transfer[i + 1] = r;
  }

  os_delay_us(1);
  os_delay_us(1);

  os_intr_lock();
  const char * const end = transfer + length;
  while (transfer != end) {
    uint8_t mask = 0x80;
    while (mask) {
      ( *transfer & mask) ? send_ws_1(pin_num[pin]) : send_ws_0(pin_num[pin]);
      mask >>= 1;
    }
    ++transfer;
  }
  os_intr_unlock();
  c_free(transfer-length);

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "set_brightness" ), LFUNCVAL( set_brightness )},
  { LSTRKEY( "brightness" ), LFUNCVAL( get_brightness )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writergb )},
  { LSTRKEY( "hsv2rgb" ), LFUNCVAL( ws2812_hsv2rgb )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}
