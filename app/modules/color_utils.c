#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_math.h"
#include "c_string.h"
#include "user_interface.h"
#include "osapi.h"

#include "color_utils.h"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define abs(a) ((a) > 0 ? (a) : (0-a))
#define min3(a,b, c) min((a), min((b), (c)))
#define max3(a,b, c) max((a), max((b), (c)))

#define CANARY_VALUE 0x37383132


// convert hsv to grb value
uint32_t hsv2grb(uint16_t hue, uint8_t sat, uint8_t val)
{
  uint16_t H_accent = (hue % 360) / 60;
  uint16_t bottom = ((255 - sat) * val)>>8;
  uint16_t top = val;
  uint8_t rising  = ((top-bottom)  *(hue%60   )  )  /  60  +  bottom;
  uint8_t falling = ((top-bottom)  *(60-hue%60)  )  /  60  +  bottom;

  uint8_t r;
  uint8_t g;
  uint8_t b;

  switch(H_accent) {
  case 0:
    r = top;
    g = rising;
    b = bottom;
    break;

  case 1:
    r = falling;
    g = top;
    b = bottom;
    break;

  case 2:
    r = bottom;
    g = top;
    b = rising;
    break;

  case 3:
    r = bottom;
    g = falling;
    b = top;
    break;

  case 4:
    r = rising;
    g = bottom;
    b = top;
    break;

  case 5:
    r = top;
    g = bottom;
    b = falling;
    break;
  }
  uint32_t result = (g << 16) | (r << 8) | b;
  return result;
}

// convert hsv to grbw value
uint32_t hsv2grbw(uint16_t hue, uint8_t sat, uint8_t val) {

  uint32_t grb = hsv2grb(hue, sat, val);

  uint8_t g = ((grb & 0x00FF0000) >> 16);
  uint8_t r = ((grb & 0x0000FF00) >> 8);
  uint8_t b = (grb & 0x000000FF);

  // calculate white component
  uint8_t w = min3(g, r, b);
  g = g - w;
  r = r - w;
  b = b - w;
  uint32_t grbw = (g << 24) | (r << 16) | (b << 8) | w;

  return grbw;
}


// convert grb to hsv value
uint32_t grb2hsv(uint8_t g, uint8_t r, uint8_t b) {
    uint8_t m = min3(r, g, b);
    uint8_t M = max3(r, g, b);
    uint8_t delta = M - m;

    int hue = 0;
    int saturation = 0;
    int value = 0;

    if(delta == 0) {
        /* Achromatic case (i.e. grayscale) */
        hue = -1;          /* undefined */
        saturation = 0;
    } else {
        int h;

        if(r == M)
            h = ((g-b)*60) / delta;
        else if(g == M)
            h = ((b-r)*60) / delta + 120;
        else /*if(b == M)*/
            h = ((r-g)*60) / delta + 240;

        if(h < 0)
            h += 360;

        hue = h;

        /* The constatnt 8 is tuned to statistically cause as little
         * tolerated mismatches as possible in RGB -> HSV -> RGB conversion.
         * (See the unit test at the bottom of this file.)
         */
        saturation = (256*delta-8) / M;
    }
    value = M;

    uint32_t result = (hue << 16) | (saturation << 8) | value;
    return result;

}

/*
 * Put a value 0 to 360 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t color_wheel(uint16_t pos) {
  return hsv2grb(pos, 255, 255);
}


// convert hsv to grb value
static int cu_hsv2grb(lua_State *L) {
  const int hue = luaL_checkint(L, 1);
  const int sat = luaL_checkint(L, 2);
  const int val = luaL_checkint(L, 3);

  luaL_argcheck(L, hue >= 0 && hue <= 360, 1, "should be a 0-360");
  luaL_argcheck(L, sat >= 0 && sat <= 255, 2, "should be 0-255");
  luaL_argcheck(L, val >= 0 && val <= 255, 3, "should be 0-255");

  // convert to grb
  uint32_t tmp_color = hsv2grb(hue, sat, val);

  // return
  lua_pushnumber(L, (tmp_color & 0x00FF0000) >> 16);
  lua_pushnumber(L, (tmp_color & 0x0000FF00) >> 8);
  lua_pushnumber(L, (tmp_color & 0x000000FF));

  return 3;
}


// convert hsv to grbw value
static int cu_hsv2grbw(lua_State *L) {
  const int hue = luaL_checkint(L, 1);
  const int sat = luaL_checkint(L, 2);
  const int val = luaL_checkint(L, 3);

  luaL_argcheck(L, hue >= 0 && hue <= 360, 1, "should be a 0-360");
  luaL_argcheck(L, sat >= 0 && sat <= 255, 2, "should be 0-255");
  luaL_argcheck(L, val >= 0 && val <= 255, 3, "should be 0-255");

  // convert to grbw
  uint32_t tmp_color = hsv2grbw(hue, sat, val);

  // return g, r, b, w
  lua_pushnumber(L, (tmp_color & 0xFF000000) >> 24);
  lua_pushnumber(L, (tmp_color & 0x00FF0000) >> 16);
  lua_pushnumber(L, (tmp_color & 0x0000FF00) >> 8);
  lua_pushnumber(L, (tmp_color & 0x000000FF));

  return 4;
}


// create a color wheel value
static int cu_color_wheel(lua_State *L) {
  const int wheel_index = luaL_checkint(L, 1);

  luaL_argcheck(L, wheel_index >= 0 && wheel_index <= 360, 1, "should be a 0-360");

  uint32_t color = color_wheel(wheel_index);

  uint8_t r = (color & 0x00FF0000) >> 16;
  uint8_t g = (color & 0x0000FF00) >>  8;
  uint8_t b = (color & 0x000000FF) >>  0;

  // return
  lua_pushnumber(L, g);
  lua_pushnumber(L, r);
  lua_pushnumber(L, b);

  return 3;
}

// convert grb values to hsv
static int cu_grb2hsv(lua_State *L) {

  const int g = luaL_checkint(L, 1);
  const int r = luaL_checkint(L, 2);
  const int b = luaL_checkint(L, 3);

  luaL_argcheck(L, g == r && g == b, 1, "greyscale value cannot be converted to hsv");

  uint32_t hsv = grb2hsv(g, r, b);

  uint16_t h = (hsv & 0xFFFF0000) >> 16;
  uint8_t s = (hsv & 0x0000FF00) >>  8;
  uint8_t v = (hsv & 0x000000FF) >>  0;

  // return
  lua_pushnumber(L, h);
  lua_pushnumber(L, s);
  lua_pushnumber(L, v);

  return 3;
}


static const LUA_REG_TYPE color_utils_map[] =
{
  { LSTRKEY( "hsv2grb" ),       LFUNCVAL( cu_hsv2grb )},
  { LSTRKEY( "hsv2grbw" ),      LFUNCVAL( cu_hsv2grbw )},
  { LSTRKEY( "colorWheel" ),    LFUNCVAL( cu_color_wheel )},
  { LSTRKEY( "grb2hsv" ),       LFUNCVAL( cu_grb2hsv )},
  { LNILKEY, LNILVAL}
};

NODEMCU_MODULE(COLOR_UTILS, "color_utils", color_utils_map, NULL);
