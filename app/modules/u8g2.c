// Module for binding the u8g2 library
// Note: This file is intended to be shared between esp8266 and esp32 platform

// Do not use the code from u8g2 submodule and skip the complete source here
// if the u8g2 module is not selected.
// Reason: The whole u8g2 submodule code tree might not even exist in this case.
#include "user_modules.h"
#if defined(LUA_USE_MODULES_U8G2) || defined(ESP_PLATFORM)

#include "module.h"
#include "lauxlib.h"

#define U8X8_USE_PINS
#include "u8g2.h"
#include "u8x8_nodemcu_hal.h"

#include "u8g2_displays.h"
#include "u8g2_fonts.h"

#ifdef ESP_PLATFORM
// ESP32
#include "spi_common.h"

#include "sdkconfig.h"
#endif

#ifndef CONFIG_LUA_MODULE_U8G2
// ignore unused functions if u8g2 module will be skipped anyhow
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


typedef struct {
  int font_ref;
  int host_ref;
  u8g2_nodemcu_t u8g2;
} u8g2_ud_t;

#define GET_U8G2() \
  u8g2_ud_t *ud = (u8g2_ud_t *)luaL_checkudata( L, 1, "u8g2.display" ); \
  u8g2_t *u8g2 = (u8g2_t *)(&(ud->u8g2));

static int lu8g2_clearBuffer( lua_State *L )
{
  GET_U8G2();

  u8g2_ClearBuffer( u8g2 );

  return 0;
}

static int lu8g2_drawBox( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );

  u8g2_DrawBox( u8g2, x, y, w, h );

  return 0;
}

static int lu8g2_drawCircle( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0  = luaL_checkint( L, ++stack );
  int y0  = luaL_checkint( L, ++stack );
  int rad = luaL_checkint( L, ++stack );
  int opt = luaL_optint( L, ++stack, U8G2_DRAW_ALL );

  u8g2_DrawCircle( u8g2, x0, y0, rad, opt );

  return 0;
}

static int lu8g2_drawDisc( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0  = luaL_checkint( L, ++stack );
  int y0  = luaL_checkint( L, ++stack );
  int rad = luaL_checkint( L, ++stack );
  int opt = luaL_optint( L, ++stack, U8G2_DRAW_ALL );

  u8g2_DrawDisc( u8g2, x0, y0, rad, opt );

  return 0;
}

static int lu8g2_drawEllipse( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0  = luaL_checkint( L, ++stack );
  int y0  = luaL_checkint( L, ++stack );
  int rx  = luaL_checkint( L, ++stack );
  int ry  = luaL_checkint( L, ++stack );
  int opt = luaL_optint( L, ++stack, U8G2_DRAW_ALL );

  u8g2_DrawEllipse( u8g2, x0, y0, rx, ry, opt );

  return 0;
}

static int lu8g2_drawFilledEllipse( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0  = luaL_checkint( L, ++stack );
  int y0  = luaL_checkint( L, ++stack );
  int rx  = luaL_checkint( L, ++stack );
  int ry  = luaL_checkint( L, ++stack );
  int opt = luaL_optint( L, ++stack, U8G2_DRAW_ALL );

  u8g2_DrawFilledEllipse( u8g2, x0, y0, rx, ry, opt );

  return 0;
}

static int lu8g2_drawFrame( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );

  u8g2_DrawFrame( u8g2, x, y, w, h );

  return 0;
}

static int lu8g2_drawGlyph( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x   = luaL_checkint( L, ++stack );
  int y   = luaL_checkint( L, ++stack );
  int enc = luaL_checkint( L, ++stack );

  u8g2_DrawGlyph( u8g2, x, y, enc );

  return 0;
}

static int lu8g2_drawHLine( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );

  u8g2_DrawHLine( u8g2, x, y, w );

  return 0;
}

static int lu8g2_drawLine( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0 = luaL_checkint( L, ++stack );
  int y0 = luaL_checkint( L, ++stack );
  int x1 = luaL_checkint( L, ++stack );
  int y1 = luaL_checkint( L, ++stack );

  u8g2_DrawLine( u8g2, x0, y0, x1, y1 );

  return 0;
}

static int lu8g2_drawPixel( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );

  u8g2_DrawPixel( u8g2, x, y );

  return 0;
}

static int lu8g2_drawRBox( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );
  int r = luaL_checkint( L, ++stack );

  u8g2_DrawRBox( u8g2, x, y, w, h, r );

  return 0;
}

static int lu8g2_drawRFrame( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );
  int r = luaL_checkint( L, ++stack );

  u8g2_DrawRFrame( u8g2, x, y, w, h, r );

  return 0;
}

static int lu8g2_drawStr( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  const char *str = luaL_checkstring( L, ++stack );

  u8g2_DrawStr( u8g2, x, y, str );

  return 0;
}

static int lu8g2_drawTriangle( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x0 = luaL_checkint( L, ++stack );
  int y0 = luaL_checkint( L, ++stack );
  int x1 = luaL_checkint( L, ++stack );
  int y1 = luaL_checkint( L, ++stack );
  int x2 = luaL_checkint( L, ++stack );
  int y2 = luaL_checkint( L, ++stack );

  u8g2_DrawTriangle( u8g2, x0, y0, x1, y1, x2, y2 );

  return 0;
}

static int lu8g2_drawUTF8( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  const char *str = luaL_checkstring( L, ++stack );

  u8g2_DrawUTF8( u8g2, x, y, str );

  return 0;
}

static int lu8g2_drawVLine( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );

  u8g2_DrawVLine( u8g2, x, y, h );

  return 0;
}

static int lu8g2_drawXBM( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int x = luaL_checkint( L, ++stack );
  int y = luaL_checkint( L, ++stack );
  int w = luaL_checkint( L, ++stack );
  int h = luaL_checkint( L, ++stack );
  size_t len;
  const char *bitmap = luaL_checklstring( L, ++stack, &len );

  u8g2_DrawXBM( u8g2, x, y, w, h, (uint8_t *)bitmap );

  return 0;
}

static int lu8g2_getAscent( lua_State *L )
{
  GET_U8G2();

  lua_pushinteger( L, u8g2_GetAscent( u8g2 ) );
  return 1;
}

static int lu8g2_getDescent( lua_State *L )
{
  GET_U8G2();

  lua_pushinteger( L, u8g2_GetDescent( u8g2 ) );
  return 1;
}

static int lu8g2_getStrWidth( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  const char *s = luaL_checkstring( L, ++stack );

  lua_pushinteger( L, u8g2_GetStrWidth( u8g2, s ) );
  return 1;
}

static int lu8g2_getUTF8Width( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  const char *s = luaL_checkstring( L, ++stack );

  lua_pushinteger( L, u8g2_GetUTF8Width( u8g2, s ) );
  return 1;
}

static int lu8g2_sendBuffer( lua_State *L )
{
  GET_U8G2();

  u8g2_SendBuffer( u8g2 );

  return 0;
}

static int lu8g2_setBitmapMode( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int is_transparent = luaL_checkint( L, ++stack );

  u8g2_SetBitmapMode( u8g2, is_transparent );

  return 0;
}

static int lu8g2_setContrast( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int value = luaL_checkint( L, ++stack );

  u8g2_SetContrast( u8g2, value );

  return 0;
}

static int lu8g2_setDisplayRotation( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  const u8g2_cb_t *u8g2_cb = (u8g2_cb_t *)lua_touserdata( L, ++stack );

  u8g2_SetDisplayRotation( u8g2, u8g2_cb );

  return 0;
}

static int lu8g2_setDrawColor( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int col = luaL_checkint( L, ++stack );

  u8g2_SetDrawColor( u8g2, col );

  return 0;
}

static int lu8g2_setFlipMode( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int is_enable = luaL_checkint( L, ++stack );

  u8g2_SetFlipMode( u8g2, is_enable );

  return 0;
}

static int lu8g2_setFont( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  const uint8_t *font = NULL;

  luaL_unref( L, LUA_REGISTRYINDEX, ud->font_ref );
  ud->font_ref = LUA_NOREF;

  if (lua_islightuserdata( L, ++stack )) {
    font = (const uint8_t *)lua_touserdata( L, stack );

  } else if (lua_isstring( L, stack )) {
    // ref the font string to safe it in case the string variable gets gc'ed
    lua_pushvalue( L, stack );
    ud->font_ref = luaL_ref( L, LUA_REGISTRYINDEX );

    size_t len;
    font = (const uint8_t *)luaL_checklstring( L, stack, &len );

  }
  luaL_argcheck( L, font != NULL, stack, "invalid font" );

  u8g2_SetFont( u8g2, font );

  return 0;
}

static int lu8g2_setFontDirection( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int dir = luaL_checkint( L, ++stack );

  u8g2_SetFontDirection( u8g2, dir );

  return 0;
}

static int lu8g2_setFontMode( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int is_transparent = luaL_checkint( L, ++stack );

  u8g2_SetFontMode( u8g2, is_transparent );

  return 0;
}

static int lu8g2_setFontPosBaseline( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontPosBaseline( u8g2 );

  return 0;
}

static int lu8g2_setFontPosBottom( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontPosBottom( u8g2 );

  return 0;
}

static int lu8g2_setFontPosTop( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontPosTop( u8g2 );

  return 0;
}

static int lu8g2_setFontPosCenter( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontPosCenter( u8g2 );

  return 0;
}

static int lu8g2_setFontRefHeightAll( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontRefHeightAll( u8g2 );

  return 0;
}

static int lu8g2_setFontRefHeightExtendedText( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontRefHeightExtendedText( u8g2 );

  return 0;
}

static int lu8g2_setFontRefHeightText( lua_State *L )
{
  GET_U8G2();

  u8g2_SetFontRefHeightText( u8g2 );

  return 0;
}

static int lu8g2_setPowerSave( lua_State *L )
{
  GET_U8G2();
  int stack = 1;

  int is_enable = luaL_checkint( L, ++stack );

  u8g2_SetPowerSave( u8g2, is_enable );

  return 0;
}


static const LUA_REG_TYPE lu8g2_display_map[] = {
  { LSTRKEY( "clearBuffer" ),        LFUNCVAL( lu8g2_clearBuffer ) },
  { LSTRKEY( "drawBox" ),            LFUNCVAL( lu8g2_drawBox ) },
  { LSTRKEY( "drawCircle" ),         LFUNCVAL( lu8g2_drawCircle ) },
  { LSTRKEY( "drawDisc" ),           LFUNCVAL( lu8g2_drawDisc ) },
  { LSTRKEY( "drawEllipse" ),        LFUNCVAL( lu8g2_drawEllipse ) },
  { LSTRKEY( "drawFilledEllipse" ),  LFUNCVAL( lu8g2_drawFilledEllipse ) },
  { LSTRKEY( "drawFrame" ),          LFUNCVAL( lu8g2_drawFrame ) },
  { LSTRKEY( "drawGlyph" ),          LFUNCVAL( lu8g2_drawGlyph ) },
  { LSTRKEY( "drawHLine" ),          LFUNCVAL( lu8g2_drawHLine ) },
  { LSTRKEY( "drawLine" ),           LFUNCVAL( lu8g2_drawLine ) },
  { LSTRKEY( "drawPixel" ),          LFUNCVAL( lu8g2_drawPixel ) },
  { LSTRKEY( "drawRBox" ),           LFUNCVAL( lu8g2_drawRBox ) },
  { LSTRKEY( "drawRFrame" ),         LFUNCVAL( lu8g2_drawRFrame ) },
  { LSTRKEY( "drawStr" ),            LFUNCVAL( lu8g2_drawStr ) },
  { LSTRKEY( "drawTriangle" ),       LFUNCVAL( lu8g2_drawTriangle ) },
  { LSTRKEY( "drawUTF8" ),           LFUNCVAL( lu8g2_drawUTF8 ) },
  { LSTRKEY( "drawVLine" ),          LFUNCVAL( lu8g2_drawVLine ) },
  { LSTRKEY( "drawXBM" ),            LFUNCVAL( lu8g2_drawXBM ) },
  { LSTRKEY( "getAscent" ),          LFUNCVAL( lu8g2_getAscent ) },
  { LSTRKEY( "getDescent" ),         LFUNCVAL( lu8g2_getDescent ) },
  { LSTRKEY( "getStrWidth" ),        LFUNCVAL( lu8g2_getStrWidth ) },
  { LSTRKEY( "getUTF8Width" ),       LFUNCVAL( lu8g2_getUTF8Width ) },
  { LSTRKEY( "sendBuffer" ),         LFUNCVAL( lu8g2_sendBuffer ) },
  { LSTRKEY( "setBitmapMode" ),      LFUNCVAL( lu8g2_setBitmapMode ) },
  { LSTRKEY( "setContrast" ),        LFUNCVAL( lu8g2_setContrast ) },
  { LSTRKEY( "setDisplayRotation" ), LFUNCVAL( lu8g2_setDisplayRotation ) },
  { LSTRKEY( "setDrawColor" ),       LFUNCVAL( lu8g2_setDrawColor ) },
  { LSTRKEY( "setFlipMode" ),        LFUNCVAL( lu8g2_setFlipMode ) },
  { LSTRKEY( "setFont" ),            LFUNCVAL( lu8g2_setFont ) },
  { LSTRKEY( "setFontDirection" ),   LFUNCVAL( lu8g2_setFontDirection ) },
  { LSTRKEY( "setFontMode" ),        LFUNCVAL( lu8g2_setFontMode ) },
  { LSTRKEY( "setFontPosBaseline" ), LFUNCVAL( lu8g2_setFontPosBaseline ) },
  { LSTRKEY( "setFontPosBottom" ),   LFUNCVAL( lu8g2_setFontPosBottom ) },
  { LSTRKEY( "setFontPosTop" ),      LFUNCVAL( lu8g2_setFontPosTop ) },
  { LSTRKEY( "setFontPosCenter" ),   LFUNCVAL( lu8g2_setFontPosCenter ) },
  { LSTRKEY( "setFontRefHeightAll" ),          LFUNCVAL( lu8g2_setFontRefHeightAll ) },
  { LSTRKEY( "setFontRefHeightExtendedText" ), LFUNCVAL( lu8g2_setFontRefHeightExtendedText ) },
  { LSTRKEY( "setFontRefHeightText" ),         LFUNCVAL( lu8g2_setFontRefHeightText ) },
  { LSTRKEY( "setPowerSave" ),       LFUNCVAL( lu8g2_setPowerSave ) },
  //{ LSTRKEY( "__gc" ),    LFUNCVAL( lu8g2_display_free ) },
  { LSTRKEY( "__index" ), LROVAL( lu8g2_display_map ) },
  {LNILKEY, LNILVAL}
};


uint8_t u8x8_d_overlay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

typedef void (*display_setup_fn_t)(u8g2_t *u8g2, const u8g2_cb_t *rotation, u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb);

// ***************************************************************************
// Device constructors
//
// I2C based devices will use this function template to implement the Lua binding.
//
static int ldisplay_i2c( lua_State *L, display_setup_fn_t setup_fn )
{
  int stack = 0;

  int id = -1;
  int i2c_addr = -1;
  int rfb_cb_ref = LUA_NOREF;

  if (lua_type( L, ++stack) == LUA_TNUMBER) {
    /* hardware display connected */
    id = luaL_checkint( L, stack );
    i2c_addr = luaL_checkint( L, ++stack );
    luaL_argcheck( L, i2c_addr >= 0 && i2c_addr <= 0x7f, stack, "invalid i2c address" );
  } else
    stack--;
  if (lua_isfunction( L, ++stack )) {
    lua_pushvalue( L, stack );
    rfb_cb_ref = luaL_ref( L, LUA_REGISTRYINDEX );
  }
  if (id < 0 && rfb_cb_ref == LUA_NOREF)
    return luaL_error( L, "wrong args" );

  u8g2_ud_t *ud = (u8g2_ud_t *)lua_newuserdata( L, sizeof( u8g2_ud_t ) );
  u8g2_nodemcu_t *ext_u8g2 = &(ud->u8g2);
  ud->font_ref = LUA_NOREF;
  ud->host_ref = LUA_NOREF;
  /* the i2c driver id is forwarded in the hal member */
  ext_u8g2->hal = id >= 0 ? (void *)id : NULL;

  u8g2_t *u8g2 = (u8g2_t *)ext_u8g2;
  u8x8_t *u8x8 = (u8x8_t *)u8g2;

  setup_fn( u8g2, U8G2_R0, u8x8_byte_nodemcu_i2c, u8x8_gpio_and_delay_nodemcu );

  /* prepare overlay data */
  if (rfb_cb_ref != LUA_NOREF) {
    ext_u8g2->overlay.template_display_cb = u8x8->display_cb;
    ext_u8g2->overlay.hardware_display_cb = NULL;
    ext_u8g2->overlay.rfb_cb_ref = LUA_NOREF;
    u8x8->display_cb = u8x8_d_overlay;
  }
  if (id >= 0) {
    /* hardware device specific initialization */
    u8x8_SetI2CAddress( u8x8, i2c_addr );
    ext_u8g2->overlay.hardware_display_cb = ext_u8g2->overlay.template_display_cb;
  }

  u8g2_InitDisplay( (u8g2_t *)u8g2 );
  u8g2_ClearDisplay( (u8g2_t *)u8g2 );
  u8g2_SetPowerSave( (u8g2_t *)u8g2, 0 );

  if (rfb_cb_ref != LUA_NOREF) {
    /* finally enable rfb display driver */
    ext_u8g2->overlay.rfb_cb_ref = rfb_cb_ref;
  }

  /* set its metatable */
  luaL_getmetatable(L, "u8g2.display");
  lua_setmetatable(L, -2);

  return 1;
}
//
// SPI based devices will use this function template to implement the Lua binding.
//
static int ldisplay_spi( lua_State *L, display_setup_fn_t setup_fn )
{
  int stack = 0;

#ifndef ESP_PLATFORM
  // ESP8266
  typedef struct {
    int host;
  } lspi_host_t;
  lspi_host_t host_elem;
  lspi_host_t *host = &host_elem;
#else
  // ESP32
  lspi_host_t *host = NULL;
#endif
  int host_ref = LUA_NOREF;
  int cs = -1;
  int dc = -1;
  int res = -1;
  int rfb_cb_ref = LUA_NOREF;
  int get_spi_pins;


  if (lua_type( L, ++stack ) == LUA_TUSERDATA) {
    host = (lspi_host_t *)luaL_checkudata( L, stack, "spi.master" );
    /* reference host object to avoid automatic gc */
    lua_pushvalue( L, stack );
    host_ref = luaL_ref( L, LUA_REGISTRYINDEX );
    get_spi_pins = 1;
  } else if (lua_type( L, stack ) == LUA_TNUMBER) {
    host->host = luaL_checkint( L, stack );
    get_spi_pins = 1;
  } else {
    get_spi_pins = 0;
    stack--;
  }

  if (get_spi_pins) {
    cs = luaL_checkint( L, ++stack );
    dc = luaL_checkint( L, ++stack );
    res = luaL_optint( L, ++stack, -1 );
  }

  if (lua_isfunction( L, ++stack )) {
    lua_pushvalue( L, stack );
    rfb_cb_ref = luaL_ref( L, LUA_REGISTRYINDEX );
  }
  if (!host && rfb_cb_ref == LUA_NOREF)
    return luaL_error( L, "wrong args" );

  u8g2_ud_t *ud = (u8g2_ud_t *)lua_newuserdata( L, sizeof( u8g2_ud_t ) );
  u8g2_nodemcu_t *ext_u8g2 = &(ud->u8g2);
  ud->font_ref = LUA_NOREF;
  ud->host_ref = host_ref;
  /* the spi host id is forwarded in the hal member */
  ext_u8g2->hal = host ? (void *)(host->host) : NULL;

  u8g2_t *u8g2 = (u8g2_t *)ext_u8g2;
  u8x8_t *u8x8 = (u8x8_t *)u8g2;

  setup_fn( u8g2, U8G2_R0, u8x8_byte_nodemcu_spi, u8x8_gpio_and_delay_nodemcu );

  /* prepare overlay data */
  if (rfb_cb_ref != LUA_NOREF) {
    ext_u8g2->overlay.template_display_cb = u8x8->display_cb;
    ext_u8g2->overlay.hardware_display_cb = NULL;
    ext_u8g2->overlay.rfb_cb_ref = LUA_NOREF;
    u8x8->display_cb = u8x8_d_overlay;
  }
  if (host) {
    /* hardware specific device initialization */
    u8x8_SetPin( u8x8, U8X8_PIN_CS, cs );
    u8x8_SetPin( u8x8, U8X8_PIN_DC, dc );
    if (res >= 0)
      u8x8_SetPin( u8x8, U8X8_PIN_RESET, res );
    ext_u8g2->overlay.hardware_display_cb = ext_u8g2->overlay.template_display_cb;
  }

  u8g2_InitDisplay( (u8g2_t *)u8g2 );
  u8g2_ClearDisplay( (u8g2_t *)u8g2 );
  u8g2_SetPowerSave( (u8g2_t *)u8g2, 0 );

  if (rfb_cb_ref != LUA_NOREF) {
    /* finally enable rfb display driver */
    ext_u8g2->overlay.rfb_cb_ref = rfb_cb_ref;
  }

  /* set its metatable */
  luaL_getmetatable(L, "u8g2.display");
  lua_setmetatable(L, -2);

  return 1;
}
//
//
#undef U8G2_DISPLAY_TABLE_ENTRY
#define U8G2_DISPLAY_TABLE_ENTRY(function, binding) \
  static int l ## binding( lua_State *L )           \
  {                                                 \
    return ldisplay_i2c( L, function );             \
  }
//
// Unroll the display table and insert binding functions for I2C based displays.
U8G2_DISPLAY_TABLE_I2C
//
//
#undef U8G2_DISPLAY_TABLE_ENTRY
#define U8G2_DISPLAY_TABLE_ENTRY(function, binding) \
  static int l ## binding( lua_State *L )           \
  {                                                 \
    return ldisplay_spi( L, function );             \
  }
//
// Unroll the display table and insert binding functions for SPI based displays.
U8G2_DISPLAY_TABLE_SPI
//


#undef U8G2_FONT_TABLE_ENTRY
#undef U8G2_DISPLAY_TABLE_ENTRY
#define U8G2_DISPLAY_TABLE_ENTRY(function, binding) \
  { LSTRKEY( #binding ),           LFUNCVAL( l ## binding ) },

static const LUA_REG_TYPE lu8g2_map[] = {
  U8G2_DISPLAY_TABLE_I2C
  U8G2_DISPLAY_TABLE_SPI
  //
  // Register fonts
#define U8G2_FONT_TABLE_ENTRY(font) \
  { LSTRKEY( #font ),              LUDATA( (void *)(u8g2_ ## font) ) },
  U8G2_FONT_TABLE
  //
  { LSTRKEY( "DRAW_UPPER_RIGHT" ), LNUMVAL( U8G2_DRAW_UPPER_RIGHT ) },
  { LSTRKEY( "DRAW_UPPER_LEFT" ),  LNUMVAL( U8G2_DRAW_UPPER_LEFT ) },
  { LSTRKEY( "DRAW_LOWER_RIGHT" ), LNUMVAL( U8G2_DRAW_LOWER_RIGHT ) },
  { LSTRKEY( "DRAW_LOWER_LEFT" ),  LNUMVAL( U8G2_DRAW_LOWER_LEFT ) },
  { LSTRKEY( "DRAW_ALL" ),         LNUMVAL( U8G2_DRAW_ALL ) },
  { LSTRKEY( "R0" ),               LUDATA( (void *)U8G2_R0 ) },
  { LSTRKEY( "R1" ),               LUDATA( (void *)U8G2_R1 ) },
  { LSTRKEY( "R2" ),               LUDATA( (void *)U8G2_R2 ) },
  { LSTRKEY( "R3" ),               LUDATA( (void *)U8G2_R3 ) },
  { LSTRKEY( "MIRROR" ),           LUDATA( (void *)U8G2_MIRROR ) },
  {LNILKEY, LNILVAL}
};

int luaopen_u8g2( lua_State *L ) {
  luaL_rometatable(L, "u8g2.display", (void *)lu8g2_display_map);
  return 0;
}

NODEMCU_MODULE(U8G2, "u8g2", lu8g2_map, luaopen_u8g2);

#endif /* defined(LUA_USE_MODULES_U8G2) || defined(ESP_PLATFORM) */
