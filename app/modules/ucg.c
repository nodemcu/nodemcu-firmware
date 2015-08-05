// Module for Ucglib

#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_stdlib.h"

#include "ucg.h"

//#include "u8g_config.h"

typedef struct ucg_t lucg_userdata_t;


// helper function: retrieve and check userdata argument
static lucg_userdata_t *get_lud( lua_State *L )
{
    lucg_userdata_t *lud = (lucg_userdata_t *)luaL_checkudata(L, 1, "ucg.display");
    luaL_argcheck(L, lud, 1, "ucg.display expected");
    return lud;
}

// helper function: retrieve given number of integer arguments
static void lucg_get_int_args( lua_State *L, uint8_t stack, uint8_t num, ucg_int_t *args)
{
    while (num-- > 0)
    {
        *args++ = luaL_checkinteger( L, stack++ );
    }
}



// device destructor
static int lucg_close_display( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    return 0;
}



// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE lucg_display_map[] =
{
#if 0
    { LSTRKEY( "begin" ),  LFUNCVAL( lu8g_begin ) },
    { LSTRKEY( "drawBitmap" ),  LFUNCVAL( lu8g_drawBitmap ) },
    { LSTRKEY( "drawBox" ),  LFUNCVAL( lu8g_drawBox ) },
    { LSTRKEY( "drawCircle" ),  LFUNCVAL( lu8g_drawCircle ) },
    { LSTRKEY( "drawDisc" ),  LFUNCVAL( lu8g_drawDisc ) },
    { LSTRKEY( "drawEllipse" ),  LFUNCVAL( lu8g_drawEllipse ) },
    { LSTRKEY( "drawFilledEllipse" ),  LFUNCVAL( lu8g_drawFilledEllipse ) },
    { LSTRKEY( "drawFrame" ),  LFUNCVAL( lu8g_drawFrame ) },
    { LSTRKEY( "drawHLine" ),  LFUNCVAL( lu8g_drawHLine ) },
    { LSTRKEY( "drawLine" ),  LFUNCVAL( lu8g_drawLine ) },
    { LSTRKEY( "drawPixel" ),  LFUNCVAL( lu8g_drawPixel ) },
    { LSTRKEY( "drawRBox" ),  LFUNCVAL( lu8g_drawRBox ) },
    { LSTRKEY( "drawRFrame" ),  LFUNCVAL( lu8g_drawRFrame ) },
    { LSTRKEY( "drawStr" ),  LFUNCVAL( lu8g_drawStr ) },
    { LSTRKEY( "drawStr90" ),  LFUNCVAL( lu8g_drawStr90 ) },
    { LSTRKEY( "drawStr180" ),  LFUNCVAL( lu8g_drawStr180 ) },
    { LSTRKEY( "drawStr270" ),  LFUNCVAL( lu8g_drawStr270 ) },
    { LSTRKEY( "drawTriangle" ),  LFUNCVAL( lu8g_drawTriangle ) },
    { LSTRKEY( "drawVLine" ),  LFUNCVAL( lu8g_drawVLine ) },
    { LSTRKEY( "drawXBM" ),  LFUNCVAL( lu8g_drawXBM ) },
    { LSTRKEY( "firstPage" ),  LFUNCVAL( lu8g_firstPage ) },
    { LSTRKEY( "getColorIndex" ),  LFUNCVAL( lu8g_getColorIndex ) },
    { LSTRKEY( "getFontAscent" ),  LFUNCVAL( lu8g_getFontAscent ) },
    { LSTRKEY( "getFontDescent" ),  LFUNCVAL( lu8g_getFontDescent ) },
    { LSTRKEY( "getFontLineSpacing" ),  LFUNCVAL( lu8g_getFontLineSpacing ) },
    { LSTRKEY( "getHeight" ),  LFUNCVAL( lu8g_getHeight ) },
    { LSTRKEY( "getMode" ),  LFUNCVAL( lu8g_getMode ) },
    { LSTRKEY( "getStrWidth" ), LFUNCVAL( lu8g_getStrWidth ) },
    { LSTRKEY( "getWidth" ),  LFUNCVAL( lu8g_getWidth ) },
    { LSTRKEY( "nextPage" ),  LFUNCVAL( lu8g_nextPage ) },
    { LSTRKEY( "setColorIndex" ),  LFUNCVAL( lu8g_setColorIndex ) },
    { LSTRKEY( "setDefaultBackgroundColor" ),  LFUNCVAL( lu8g_setDefaultBackgroundColor ) },
    { LSTRKEY( "setDefaultForegroundColor" ),  LFUNCVAL( lu8g_setDefaultForegroundColor ) },
    { LSTRKEY( "setFont" ),  LFUNCVAL( lu8g_setFont ) },
    { LSTRKEY( "setFontLineSpacingFactor" ),  LFUNCVAL( lu8g_setFontLineSpacingFactor ) },
    { LSTRKEY( "setFontPosBaseline" ),  LFUNCVAL( lu8g_setFontPosBaseline ) },
    { LSTRKEY( "setFontPosBottom" ),  LFUNCVAL( lu8g_setFontPosBottom ) },
    { LSTRKEY( "setFontPosCenter" ),  LFUNCVAL( lu8g_setFontPosCenter ) },
    { LSTRKEY( "setFontPosTop" ),  LFUNCVAL( lu8g_setFontPosTop ) },
    { LSTRKEY( "setFontRefHeightAll" ),  LFUNCVAL( lu8g_setFontRefHeightAll ) },
    { LSTRKEY( "setFontRefHeightExtendedText" ),  LFUNCVAL( lu8g_setFontRefHeightExtendedText ) },
    { LSTRKEY( "setFontRefHeightText" ),  LFUNCVAL( lu8g_setFontRefHeightText ) },
    { LSTRKEY( "setRot90" ),  LFUNCVAL( lu8g_setRot90 ) },
    { LSTRKEY( "setRot180" ),  LFUNCVAL( lu8g_setRot180 ) },
    { LSTRKEY( "setRot270" ),  LFUNCVAL( lu8g_setRot270 ) },
    { LSTRKEY( "setScale2x2" ),  LFUNCVAL( lu8g_setScale2x2 ) },
    { LSTRKEY( "sleepOff" ),  LFUNCVAL( lu8g_sleepOff ) },
    { LSTRKEY( "sleepOn" ),  LFUNCVAL( lu8g_sleepOn ) },
    { LSTRKEY( "undoRotation" ),  LFUNCVAL( lu8g_undoRotation ) },
    { LSTRKEY( "undoScale" ),  LFUNCVAL( lu8g_undoScale ) },
#endif
    { LSTRKEY( "__gc" ),  LFUNCVAL( lucg_close_display ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "__index" ), LROVAL ( lucg_display_map ) },
#endif
    { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE lucg_map[] = 
{
#if 0
#undef U8G_DISPLAY_TABLE_ENTRY
#define U8G_DISPLAY_TABLE_ENTRY(device) { LSTRKEY( #device ), LFUNCVAL ( lu8g_ ##device ) },
    U8G_DISPLAY_TABLE_I2C
    U8G_DISPLAY_TABLE_SPI
#endif

#if LUA_OPTIMIZE_MEMORY > 0

    // Register fonts
#if 0
#undef U8G_FONT_TABLE_ENTRY
#define U8G_FONT_TABLE_ENTRY(font) { LSTRKEY( #font ), LUDATA( (void *)(u8g_ ## font) ) },
    U8G_FONT_TABLE
#endif

#if 0
    // Options for circle/ ellipse drawing
    { LSTRKEY( "DRAW_UPPER_RIGHT" ), LNUMVAL( U8G_DRAW_UPPER_RIGHT ) },
    { LSTRKEY( "DRAW_UPPER_LEFT" ),  LNUMVAL( U8G_DRAW_UPPER_LEFT ) },
    { LSTRKEY( "DRAW_LOWER_RIGHT" ), LNUMVAL( U8G_DRAW_LOWER_RIGHT ) },
    { LSTRKEY( "DRAW_LOWER_LEFT" ),  LNUMVAL( U8G_DRAW_LOWER_LEFT ) },
    { LSTRKEY( "DRAW_ALL" ),         LNUMVAL( U8G_DRAW_ALL ) },

    // Display modes
    { LSTRKEY( "MODE_BW" ),       LNUMVAL( U8G_MODE_BW ) },
    { LSTRKEY( "MODE_GRAY2BIT" ), LNUMVAL( U8G_MODE_GRAY2BIT ) },
#endif

    { LSTRKEY( "__metatable" ), LROVAL( lucg_map ) },
#endif
    { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_ucg( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
    luaL_rometatable(L, "ucg.display", (void *)lucg_display_map);  // create metatable
    return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
    int n;
    luaL_register( L, AUXLIB_UCG, lucg_map );

    // Set it as its own metatable
    lua_pushvalue( L, -1 );
    lua_setmetatable( L, -2 );

    // Module constants  

    // Register fonts
#if 0
#undef U8G_FONT_TABLE_ENTRY
#define U8G_FONT_TABLE_ENTRY(font) MOD_REG_LUDATA( L, #font, (void *)(u8g_ ## font) );
    U8G_FONT_TABLE
#endif

#if 0
    // Options for circle/ ellipse drawing
    MOD_REG_NUMBER( L, "DRAW_UPPER_RIGHT", U8G_DRAW_UPPER_RIGHT );
    MOD_REG_NUMBER( L, "DRAW_UPPER_LEFT",  U8G_DRAW_UPPER_RIGHT );
    MOD_REG_NUMBER( L, "DRAW_LOWER_RIGHT", U8G_DRAW_UPPER_RIGHT );
    MOD_REG_NUMBER( L, "DRAW_LOWER_LEFT",  U8G_DRAW_UPPER_RIGHT );

    // Display modes
    MOD_REG_NUMBER( L, "MODE_BW",       U8G_MODE_BW );
    MOD_REG_NUMBER( L, "MODE_GRAY2BIT", U8G_MODE_BW );
#endif

    // create metatable
    luaL_newmetatable(L, "ucg.display");
    // metatable.__index = metatable
    lua_pushliteral(L, "__index");
    lua_pushvalue(L,-2);
    lua_rawset(L,-3);
    // Setup the methods inside metatable
    luaL_register( L, NULL, ucg_display_map );

    return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
