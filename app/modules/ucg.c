// Module for Ucglib

// Do not use the code from ucg submodule and skip the complete source here
// if the ucg module is not selected.
// Reason: The whole ucg submodule code tree might not even exist in this case.
#include "user_modules.h"
#ifdef LUA_USE_MODULES_UCG

#include "module.h"
#include "lauxlib.h"


#define USE_PIN_LIST
#include "ucg.h"
#include "ucg_nodemcu_hal.h"


#include "ucg_config.h"


#ifdef ESP_PLATFORM
// ESP32
#include "spi_common.h"

#include "sdkconfig.h"
#endif

#ifndef CONFIG_LUA_MODULE_U8G2
// ignore unused functions if u8g2 module will be skipped anyhow
#pragma GCC diagnostic ignored "-Wunused-function"
#endif



typedef struct
{
  ucg_nodemcu_t ucg;

  ucg_dev_fnptr dev_cb;
  ucg_dev_fnptr ext_cb;

  // For Print() function
  ucg_int_t tx, ty;
  uint8_t tdir;

  int font_ref;
  int host_ref;
} ucg_ud_t;




// shorthand macro for the ucg structure inside the userdata
#define GET_UCG() \
  ucg_ud_t *ud = (ucg_ud_t *)luaL_checkudata( L, 1, "ucg.display" ); \
  ucg_t *ucg = (ucg_t *)(&(ud->ucg));


// helper function: retrieve given number of integer arguments
static void lucg_get_int_args( lua_State *L, uint8_t stack, uint8_t num, ucg_int_t *args)
{
  while (num-- > 0) {
    *args++ = luaL_checkinteger( L, stack++ );
  }
}

// Lua: ucg.begin( self, fontmode )
static int lucg_begin( lua_State *L )
{
  GET_UCG();

  ucg_Init( ucg, ud->dev_cb, ud->ext_cb, ucg_com_nodemcu_hw_spi );

  ucg_int_t fontmode = luaL_checkinteger( L, 2 );

  ucg_SetFontMode( ucg, fontmode );

  return 0;
}

// Lua: ucg.clearScreen( self )
static int lucg_clearScreen( lua_State *L )
{
  GET_UCG();

  ucg_ClearScreen( ucg );

  return 0;
}

// Lua: ucg.draw90Line( self, x, y, len, dir, col_idx )
static int lucg_draw90Line( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[5];
  lucg_get_int_args( L, 2, 5, args );

  ucg_Draw90Line( ucg, args[0], args[1], args[2], args[3], args[4] );

  return 0;
}

// Lua: ucg.drawBox( self, x, y, w, h )
static int lucg_drawBox( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawBox( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawCircle( self, x0, y0, rad, option )
static int lucg_drawCircle( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawCircle( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawDisc( self, x0, y0, rad, option )
static int lucg_drawDisc( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawDisc( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawFrame( self, x, y, w, h )
static int lucg_drawFrame( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawFrame( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawGradientBox( self, x, y, w, h )
static int lucg_drawGradientBox( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawGradientBox( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: width = ucg.drawGlyph( self, x, y, dir, encoding )
static int lucg_drawGlyph( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[3];
  lucg_get_int_args( L, 2, 3, args );

  const char *c = luaL_checkstring( L, (1+3) + 1 );
  if (c == NULL)
    return 0;

  lua_pushinteger( L, ucg_DrawGlyph( ucg, args[0], args[1], args[2], *c ) );

  return 1;
}

// Lua: ucg.drawGradientLine( self, x, y, len, dir )
static int lucg_drawGradientLine( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawGradientLine( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawHLine( self, x, y, len )
static int lucg_drawHLine( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[3];
  lucg_get_int_args( L, 2, 3, args );

  ucg_DrawHLine( ucg, args[0], args[1], args[2] );

  return 0;
}

// Lua: ucg.drawLine( self, x1, y1, x2, y2 )
static int lucg_drawLine( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_DrawLine( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.drawPixel( self, x, y )
static int lucg_drawPixel( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[2];
  lucg_get_int_args( L, 2, 2, args );

  ucg_DrawPixel( ucg, args[0], args[1] );

  return 0;
}

// Lua: ucg.drawRBox( self, x, y, w, h, r )
static int lucg_drawRBox( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[5];
  lucg_get_int_args( L, 2, 5, args );

  ucg_DrawRBox( ucg, args[0], args[1], args[2], args[3], args[4] );

  return 0;
}

// Lua: ucg.drawRFrame( self, x, y, w, h, r )
static int lucg_drawRFrame( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[5];
  lucg_get_int_args( L, 2, 5, args );

  ucg_DrawRFrame( ucg, args[0], args[1], args[2], args[3], args[4] );

  return 0;
}

// Lua: width = ucg.drawString( self, x, y, dir, str )
static int lucg_drawString( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[3];
  lucg_get_int_args( L, 2, 3, args );

  const char *s = luaL_checkstring( L, (1+3) + 1 );
  if (s == NULL)
    return 0;

  lua_pushinteger( L, ucg_DrawString( ucg, args[0], args[1], args[2], s ) );

  return 1;
}

// Lua: ucg.drawTetragon( self, x0, y0, x1, y1, x2, y2, x3, y3 )
static int lucg_drawTetragon( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[8];
  lucg_get_int_args( L, 2, 8, args );

  ucg_DrawTetragon( ucg, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7] );

  return 0;
}

// Lua: ucg.drawTriangle( self, x0, y0, x1, y1, x2, y2 )
static int lucg_drawTriangle( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[6];
  lucg_get_int_args( L, 2, 6, args );

  ucg_DrawTriangle( ucg, args[0], args[1], args[2], args[3], args[4], args[5] );

  return 0;
}

// Lua: ucg.drawVLine( self, x, y, len )
static int lucg_drawVLine( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[3];
  lucg_get_int_args( L, 2, 3, args );

  ucg_DrawVLine( ucg, args[0], args[1], args[2] );

  return 0;
}

// Lua: height = ucg.getFontAscent( self )
static int lucg_getFontAscent( lua_State *L )
{
  GET_UCG();

  lua_pushinteger( L, ucg_GetFontAscent( ucg ) );

  return 1;
}

// Lua: height = ucg.getFontDescent( self )
static int lucg_getFontDescent( lua_State *L )
{
  GET_UCG();

  lua_pushinteger( L, ucg_GetFontDescent( ucg ) );

  return 1;
}

// Lua: height = ucg.getHeight( self )
static int lucg_getHeight( lua_State *L )
{
  GET_UCG();

  lua_pushinteger( L, ucg_GetHeight( ucg ) );

  return 1;
}

// Lua: width = ucg.getStrWidth( self, string )
static int lucg_getStrWidth( lua_State *L )
{
  GET_UCG();

  const char *s = luaL_checkstring( L, 2 );
  if (s == NULL)
    return 0;

  lua_pushinteger( L, ucg_GetStrWidth( ucg, s ) );

  return 1;
}

// Lua: width = ucg.getWidth( self )
static int lucg_getWidth( lua_State *L )
{
  GET_UCG();

  lua_pushinteger( L, ucg_GetWidth( ucg ) );

  return 1;
}

// Lua: ucg.setClipRange( self, x, y, w, h )
static int lucg_setClipRange( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[4];
  lucg_get_int_args( L, 2, 4, args );

  ucg_SetClipRange( ucg, args[0], args[1], args[2], args[3] );

  return 0;
}

// Lua: ucg.setColor( self, [idx], r, g, b )
static int lucg_setColor( lua_State *L )
{
  GET_UCG();

  ucg_int_t args[3];
  lucg_get_int_args( L, 2, 3, args );

  ucg_int_t opt = luaL_optint( L, (1+3) + 1, -1 );

  if (opt < 0) {
    ucg_SetColor( ucg, 0, args[0], args[1], args[2] );
  } else {
    ucg_SetColor( ucg, args[0], args[1], args[2], opt );
  }

    return 0;
}

// Lua: ucg.setFont( self, font )
static int lucg_setFont( lua_State *L )
{
  GET_UCG();

  ucg_fntpgm_uint8_t *font = (ucg_fntpgm_uint8_t *)lua_touserdata( L, 2 );
  if (font != NULL)
    ucg_SetFont( ucg, font );
  else
    luaL_argerror(L, 2, "font data expected");

  return 0;
}

// Lua: ucg.print( self, str )
static int lucg_print( lua_State *L )
{
  GET_UCG();

  const char *s = luaL_checkstring( L, 2 );
  if (s == NULL)
    return 0;

  while (*s)
  {
    ucg_int_t delta;
    delta = ucg_DrawGlyph(ucg, ud->tx, ud->ty, ud->tdir, *(s++));
    switch(ud->tdir) {
    case 0: ud->tx += delta; break;
    case 1: ud->ty += delta; break;
    case 2: ud->tx -= delta; break;
    default: case 3: ud->ty -= delta; break;
    }
  }

  return 0;
}

// Lua: ucg.setFontMode( self, fontmode )
static int lucg_setFontMode( lua_State *L )
{
  GET_UCG();

  ucg_int_t fontmode = luaL_checkinteger( L, 2 );

  ucg_SetFontMode( ucg, fontmode );

  return 0;
}

// Lua: ucg.setFontPosBaseline( self )
static int lucg_setFontPosBaseline( lua_State *L )
{
  GET_UCG();

  ucg_SetFontPosBaseline( ucg );

  return 0;
}

// Lua: ucg.setFontPosBottom( self )
static int lucg_setFontPosBottom( lua_State *L )
{
  GET_UCG();

  ucg_SetFontPosBottom( ucg );

  return 0;
}

// Lua: ucg.setFontPosCenter( self )
static int lucg_setFontPosCenter( lua_State *L )
{
  GET_UCG();

  ucg_SetFontPosCenter( ucg );

  return 0;
}

// Lua: ucg.setFontPosTop( self )
static int lucg_setFontPosTop( lua_State *L )
{
  GET_UCG();

  ucg_SetFontPosTop( ucg );

  return 0;
}

// Lua: ucg.setMaxClipRange( self )
static int lucg_setMaxClipRange( lua_State *L )
{
  GET_UCG();

  ucg_SetMaxClipRange( ucg );

  return 0;
}

// Lua: ucg.setPrintPos( self, x, y )
static int lucg_setPrintPos( lua_State *L )
{
  GET_UCG();
  (void)ucg;

  ucg_int_t args[2];
  lucg_get_int_args( L, 2, 2, args );

  ud->tx = args[0];
  ud->ty = args[1];

  return 0;
}

// Lua: ucg.setPrintDir( self, dir )
static int lucg_setPrintDir( lua_State *L )
{
  GET_UCG();
  (void)ucg;

  ud->tdir = luaL_checkinteger( L, 2 );

  return 0;
}

// Lua: ucg.setRotate90( self )
static int lucg_setRotate90( lua_State *L )
{
  GET_UCG();

  ucg_SetRotate90( ucg );

  return 0;
}

// Lua: ucg.setRotate180( self )
static int lucg_setRotate180( lua_State *L )
{
  GET_UCG();

  ucg_SetRotate180( ucg );

  return 0;
}

// Lua: ucg.setRotate270( self )
static int lucg_setRotate270( lua_State *L )
{
  GET_UCG();

  ucg_SetRotate270( ucg );

  return 0;
}

// Lua: ucg.setScale2x2( self )
static int lucg_setScale2x2( lua_State *L )
{
  GET_UCG();

  ucg_SetScale2x2( ucg );

  return 0;
}

// Lua: ucg.undoRotate( self )
static int lucg_undoRotate( lua_State *L )
{
  GET_UCG();

  ucg_UndoRotate( ucg );

  return 0;
}

// Lua: ucg.undoScale( self )
static int lucg_undoScale( lua_State *L )
{
  GET_UCG();

  ucg_UndoScale( ucg );

  return 0;
}


// device destructor
static int lucg_close_display( lua_State *L )
{
    return 0;
}


// ***************************************************************************
// Device constructors
//
//
static int ldisplay_hw_spi( lua_State *L, ucg_dev_fnptr device, ucg_dev_fnptr extension )
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

  ucg_ud_t *ud = (ucg_ud_t *) lua_newuserdata( L, sizeof( ucg_ud_t ) );
  ucg_nodemcu_t *ext_ucg = &(ud->ucg);
  ud->font_ref = LUA_NOREF;
  ud->host_ref = host_ref;
  ext_ucg->hal = host ? (void *)(host->host) : NULL;

  ucg_t *ucg = (ucg_t *)ext_ucg;

  /* do a dummy init so that something usefull is part of the ucg structure */
  ucg_Init( ucg, ucg_dev_default_cb, ucg_ext_none, (ucg_com_fnptr)0 );

  /* reset cursor position */
  ud->tx   = 0;
  ud->ty   = 0;
  ud->tdir = 0;  /* default direction */

  uint8_t i;
  for( i = 0; i < UCG_PIN_COUNT; i++ )
    ucg->pin_list[i] = UCG_PIN_VAL_NONE;

  ud->dev_cb = device;
  ud->ext_cb = extension;
  if (res >= 0)
    ucg->pin_list[UCG_PIN_RST] = res;
  ucg->pin_list[UCG_PIN_CD]  = dc;
  ucg->pin_list[UCG_PIN_CS]  = cs;

  /* set its metatable */
  luaL_getmetatable(L, "ucg.display");
  lua_setmetatable(L, -2);

  return 1;
}
#undef UCG_DISPLAY_TABLE_ENTRY
#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension) \
  static int l ## binding( lua_State *L )                    \
  {                                                          \
   return ldisplay_hw_spi( L, device, extension );           \
  }
//
// Unroll the display table and insert binding functions.
UCG_DISPLAY_TABLE
//
// ***************************************************************************



// Module function map
static const LUA_REG_TYPE lucg_display_map[] =
{
  { LSTRKEY( "begin" ),              LFUNCVAL( lucg_begin ) },
  { LSTRKEY( "clearScreen" ),        LFUNCVAL( lucg_clearScreen ) },
  { LSTRKEY( "draw90Line" ),         LFUNCVAL( lucg_draw90Line ) },
  { LSTRKEY( "drawBox" ),            LFUNCVAL( lucg_drawBox ) },
  { LSTRKEY( "drawCircle" ),         LFUNCVAL( lucg_drawCircle ) },
  { LSTRKEY( "drawDisc" ),           LFUNCVAL( lucg_drawDisc ) },
  { LSTRKEY( "drawFrame" ),          LFUNCVAL( lucg_drawFrame ) },
  { LSTRKEY( "drawGlyph" ),          LFUNCVAL( lucg_drawGlyph ) },
  { LSTRKEY( "drawGradientBox" ),    LFUNCVAL( lucg_drawGradientBox ) },
  { LSTRKEY( "drawGradientLine" ),   LFUNCVAL( lucg_drawGradientLine ) },
  { LSTRKEY( "drawHLine" ),          LFUNCVAL( lucg_drawHLine ) },
  { LSTRKEY( "drawLine" ),           LFUNCVAL( lucg_drawLine ) },
  { LSTRKEY( "drawPixel" ),          LFUNCVAL( lucg_drawPixel ) },
  { LSTRKEY( "drawRBox" ),           LFUNCVAL( lucg_drawRBox ) },
  { LSTRKEY( "drawRFrame" ),         LFUNCVAL( lucg_drawRFrame ) },
  { LSTRKEY( "drawString" ),         LFUNCVAL( lucg_drawString ) },
  { LSTRKEY( "drawTetragon" ),       LFUNCVAL( lucg_drawTetragon ) },
  { LSTRKEY( "drawTriangle" ),       LFUNCVAL( lucg_drawTriangle ) },
  { LSTRKEY( "drawVLine" ),          LFUNCVAL( lucg_drawVLine ) },
  { LSTRKEY( "getFontAscent" ),      LFUNCVAL( lucg_getFontAscent ) },
  { LSTRKEY( "getFontDescent" ),     LFUNCVAL( lucg_getFontDescent ) },
  { LSTRKEY( "getHeight" ),          LFUNCVAL( lucg_getHeight ) },
  { LSTRKEY( "getStrWidth" ),        LFUNCVAL( lucg_getStrWidth ) },
  { LSTRKEY( "getWidth" ),           LFUNCVAL( lucg_getWidth ) },
  { LSTRKEY( "print" ),              LFUNCVAL( lucg_print ) },
  { LSTRKEY( "setClipRange" ),       LFUNCVAL( lucg_setClipRange ) },
  { LSTRKEY( "setColor" ),           LFUNCVAL( lucg_setColor ) },
  { LSTRKEY( "setFont" ),            LFUNCVAL( lucg_setFont ) },
  { LSTRKEY( "setFontMode" ),        LFUNCVAL( lucg_setFontMode ) },
  { LSTRKEY( "setFontPosBaseline" ), LFUNCVAL( lucg_setFontPosBaseline ) },
  { LSTRKEY( "setFontPosBottom" ),   LFUNCVAL( lucg_setFontPosBottom ) },
  { LSTRKEY( "setFontPosCenter" ),   LFUNCVAL( lucg_setFontPosCenter ) },
  { LSTRKEY( "setFontPosTop" ),      LFUNCVAL( lucg_setFontPosTop ) },
  { LSTRKEY( "setMaxClipRange" ),    LFUNCVAL( lucg_setMaxClipRange ) },
  { LSTRKEY( "setPrintDir" ),        LFUNCVAL( lucg_setPrintDir ) },
  { LSTRKEY( "setPrintPos" ),        LFUNCVAL( lucg_setPrintPos ) },
  { LSTRKEY( "setRotate90" ),        LFUNCVAL( lucg_setRotate90 ) },
  { LSTRKEY( "setRotate180" ),       LFUNCVAL( lucg_setRotate180 ) },
  { LSTRKEY( "setRotate270" ),       LFUNCVAL( lucg_setRotate270 ) },
  { LSTRKEY( "setScale2x2" ),        LFUNCVAL( lucg_setScale2x2 ) },
  { LSTRKEY( "undoClipRange" ),      LFUNCVAL( lucg_setMaxClipRange ) },
  { LSTRKEY( "undoRotate" ),         LFUNCVAL( lucg_undoRotate ) },
  { LSTRKEY( "undoScale" ),          LFUNCVAL( lucg_undoScale ) },

  { LSTRKEY( "__gc" ),  LFUNCVAL( lucg_close_display ) },
  { LSTRKEY( "__index" ), LROVAL ( lucg_display_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE lucg_map[] =
{
#undef UCG_DISPLAY_TABLE_ENTRY
#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension) { LSTRKEY( #binding ), LFUNCVAL ( l ## binding ) },
  UCG_DISPLAY_TABLE

  // Register fonts
#undef UCG_FONT_TABLE_ENTRY
#define UCG_FONT_TABLE_ENTRY(font) { LSTRKEY( #font ), LUDATA( (void *)(ucg_ ## font) ) },
  UCG_FONT_TABLE

  // Font modes
  { LSTRKEY( "FONT_MODE_TRANSPARENT" ), LNUMVAL( UCG_FONT_MODE_TRANSPARENT ) },
  { LSTRKEY( "FONT_MODE_SOLID" ),       LNUMVAL( UCG_FONT_MODE_SOLID ) },

  // Options for circle/ disc drawing
  { LSTRKEY( "DRAW_UPPER_RIGHT" ), LNUMVAL( UCG_DRAW_UPPER_RIGHT ) },
  { LSTRKEY( "DRAW_UPPER_LEFT" ),  LNUMVAL( UCG_DRAW_UPPER_LEFT ) },
  { LSTRKEY( "DRAW_LOWER_RIGHT" ), LNUMVAL( UCG_DRAW_LOWER_RIGHT ) },
  { LSTRKEY( "DRAW_LOWER_LEFT" ),  LNUMVAL( UCG_DRAW_LOWER_LEFT ) },
  { LSTRKEY( "DRAW_ALL" ),         LNUMVAL( UCG_DRAW_ALL ) },

  { LSTRKEY( "__metatable" ), LROVAL( lucg_map ) },
  { LNILKEY, LNILVAL }
};

int luaopen_ucg( lua_State *L )
{
    luaL_rometatable(L, "ucg.display", (void *)lucg_display_map);  // create metatable
    return 0;
}

NODEMCU_MODULE(UCG, "ucg", lucg_map, luaopen_ucg);

#endif /* LUA_USE_MODULES_UCG */
