// Module for Ucglib

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_stdlib.h"

#include "ucg.h"

#include "ucg_config.h"

struct _lucg_userdata_t
{
    ucg_t ucg;

    ucg_dev_fnptr dev_cb;
    ucg_dev_fnptr ext_cb;  

    // For Print() function
    ucg_int_t tx, ty;
    uint8_t tdir;
};

typedef struct _lucg_userdata_t lucg_userdata_t;


#define delayMicroseconds os_delay_us

static int16_t ucg_com_esp8266_hw_spi(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data);




// shorthand macro for the ucg structure inside the userdata
#define LUCG (&(lud->ucg))


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

// Lua: ucg.begin( self, fontmode )
static int lucg_begin( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_Init( LUCG, lud->dev_cb, lud->ext_cb, ucg_com_esp8266_hw_spi );

    ucg_int_t fontmode = luaL_checkinteger( L, 2 );

    ucg_SetFontMode( LUCG, fontmode );

    return 0;
}

// Lua: ucg.clearScreen( self )
static int lucg_clearScreen( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_ClearScreen( LUCG );

    return 0;
}

// Lua: ucg.draw90Line( self, x, y, len, dir, col_idx )
static int lucg_draw90Line( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[5];
    lucg_get_int_args( L, 2, 5, args );

    ucg_Draw90Line( LUCG, args[0], args[1], args[2], args[3], args[4] );

    return 0;
}

// Lua: ucg.drawBox( self, x, y, w, h )
static int lucg_drawBox( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawBox( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawCircle( self, x0, y0, rad, option )
static int lucg_drawCircle( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawCircle( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawDisc( self, x0, y0, rad, option )
static int lucg_drawDisc( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawDisc( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawFrame( self, x, y, w, h )
static int lucg_drawFrame( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawFrame( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawGradientBox( self, x, y, w, h )
static int lucg_drawGradientBox( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawGradientBox( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: width = ucg.drawGlyph( self, x, y, dir, encoding )
static int lucg_drawGlyph( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    const char *c = luaL_checkstring( L, (1+3) + 1 );
    if (c == NULL)
        return 0;

    lua_pushinteger( L, ucg_DrawGlyph( LUCG, args[0], args[1], args[2], *c ) );

    return 1;
}

// Lua: ucg.drawGradientLine( self, x, y, len, dir )
static int lucg_drawGradientLine( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawGradientLine( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawHLine( self, x, y, len )
static int lucg_drawHLine( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    ucg_DrawHLine( LUCG, args[0], args[1], args[2] );

    return 0;
}

// Lua: ucg.drawLine( self, x1, y1, x2, y2 )
static int lucg_drawLine( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_DrawLine( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.drawPixel( self, x, y )
static int lucg_drawPixel( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[2];
    lucg_get_int_args( L, 2, 2, args );

    ucg_DrawPixel( LUCG, args[0], args[1] );

    return 0;
}

// Lua: ucg.drawRBox( self, x, y, w, h, r )
static int lucg_drawRBox( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[5];
    lucg_get_int_args( L, 2, 5, args );

    ucg_DrawRBox( LUCG, args[0], args[1], args[2], args[3], args[4] );

    return 0;
}

// Lua: ucg.drawRFrame( self, x, y, w, h, r )
static int lucg_drawRFrame( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[5];
    lucg_get_int_args( L, 2, 5, args );

    ucg_DrawRFrame( LUCG, args[0], args[1], args[2], args[3], args[4] );

    return 0;
}

// Lua: width = ucg.drawString( self, x, y, dir, str )
static int lucg_drawString( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    const char *s = luaL_checkstring( L, (1+3) + 1 );
    if (s == NULL)
        return 0;

    lua_pushinteger( L, ucg_DrawString( LUCG, args[0], args[1], args[2], s ) );

    return 1;
}

// Lua: ucg.drawTetragon( self, x0, y0, x1, y1, x2, y2, x3, y3 )
static int lucg_drawTetragon( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[8];
    lucg_get_int_args( L, 2, 8, args );

    ucg_DrawTetragon( LUCG, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7] );

    return 0;
}

// Lua: ucg.drawTriangle( self, x0, y0, x1, y1, x2, y2 )
static int lucg_drawTriangle( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[6];
    lucg_get_int_args( L, 2, 6, args );

    ucg_DrawTriangle( LUCG, args[0], args[1], args[2], args[3], args[4], args[5] );

    return 0;
}

// Lua: ucg.drawVLine( self, x, y, len )
static int lucg_drawVLine( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    ucg_DrawVLine( LUCG, args[0], args[1], args[2] );

    return 0;
}

// Lua: height = ucg.getFontAscent( self )
static int lucg_getFontAscent( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    lua_pushinteger( L, ucg_GetFontAscent( LUCG ) );

    return 1;
}

// Lua: height = ucg.getFontDescent( self )
static int lucg_getFontDescent( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    lua_pushinteger( L, ucg_GetFontDescent( LUCG ) );

    return 1;
}

// Lua: height = ucg.getHeight( self )
static int lucg_getHeight( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    lua_pushinteger( L, ucg_GetHeight( LUCG ) );

    return 1;
}

// Lua: width = ucg.getStrWidth( self, string )
static int lucg_getStrWidth( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    const char *s = luaL_checkstring( L, 2 );
    if (s == NULL)
        return 0;

    lua_pushinteger( L, ucg_GetStrWidth( LUCG, s ) );

    return 1;
}

// Lua: width = ucg.getWidth( self )
static int lucg_getWidth( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    lua_pushinteger( L, ucg_GetWidth( LUCG ) );

    return 1;
}

// Lua: ucg.setClipRange( self, x, y, w, h )
static int lucg_setClipRange( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[4];
    lucg_get_int_args( L, 2, 4, args );

    ucg_SetClipRange( LUCG, args[0], args[1], args[2], args[3] );

    return 0;
}

// Lua: ucg.setColor( self, [idx], r, g, b )
static int lucg_setColor( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    ucg_int_t opt = luaL_optint( L, (1+3) + 1, -1 );

    if (opt < 0)
        ucg_SetColor( LUCG, 0, args[0], args[1], args[2] );
    else
        ucg_SetColor( LUCG, args[0], args[1], args[2], opt );

    return 0;
}

// Lua: ucg.setFont( self, font )
static int lucg_setFont( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_fntpgm_uint8_t *font = (ucg_fntpgm_uint8_t *)lua_touserdata( L, 2 );
    if (font != NULL)
        ucg_SetFont( LUCG, font );
    else
        luaL_argerror(L, 2, "font data expected");

    return 0;
}

// Lua: ucg.print( self, str )
static int lucg_print( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    const char *s = luaL_checkstring( L, 2 );
    if (s == NULL)
        return 0;

    while (*s)
    {
        ucg_int_t delta;
        delta = ucg_DrawGlyph(LUCG, lud->tx, lud->ty, lud->tdir, *(s++));
        switch(lud->tdir)
        {
        case 0: lud->tx += delta; break;
        case 1: lud->ty += delta; break;
        case 2: lud->tx -= delta; break;
        default: case 3: lud->ty -= delta; break;
        }
    }

    return 0;
}

// Lua: ucg.setFontMode( self, fontmode )
static int lucg_setFontMode( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t fontmode = luaL_checkinteger( L, 2 );

    ucg_SetFontMode( LUCG, fontmode );

    return 0;
}

// Lua: ucg.setFontPosBaseline( self )
static int lucg_setFontPosBaseline( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetFontPosBaseline( LUCG );

    return 0;
}

// Lua: ucg.setFontPosBottom( self )
static int lucg_setFontPosBottom( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetFontPosBottom( LUCG );

    return 0;
}

// Lua: ucg.setFontPosCenter( self )
static int lucg_setFontPosCenter( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetFontPosCenter( LUCG );

    return 0;
}

// Lua: ucg.setFontPosTop( self )
static int lucg_setFontPosTop( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetFontPosTop( LUCG );

    return 0;
}

// Lua: ucg.setMaxClipRange( self )
static int lucg_setMaxClipRange( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetMaxClipRange( LUCG );

    return 0;
}

// Lua: ucg.setPrintPos( self, x, y )
static int lucg_setPrintPos( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_int_t args[2];
    lucg_get_int_args( L, 2, 2, args );

    lud->tx = args[0];
    lud->ty = args[1];

    return 0;
}

// Lua: ucg.setPrintDir( self, dir )
static int lucg_setPrintDir( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    lud->tdir = luaL_checkinteger( L, 2 );

    return 0;
}

// Lua: ucg.setRotate90( self )
static int lucg_setRotate90( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetRotate90( LUCG );

    return 0;
}

// Lua: ucg.setRotate180( self )
static int lucg_setRotate180( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetRotate180( LUCG );

    return 0;
}

// Lua: ucg.setRotate270( self )
static int lucg_setRotate270( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetRotate270( LUCG );

    return 0;
}

// Lua: ucg.setScale2x2( self )
static int lucg_setScale2x2( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_SetScale2x2( LUCG );

    return 0;
}

// Lua: ucg.undoRotate( self )
static int lucg_undoRotate( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_UndoRotate( LUCG );

    return 0;
}

// Lua: ucg.undoScale( self )
static int lucg_undoScale( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    ucg_UndoScale( LUCG );

    return 0;
}


spi_data_type cache;
uint8_t cached;

#define CACHED_TRANSFER(dat, num) cache = cached = 0;  \
    while( arg > 0 ) {                                \
        if (cached == 4) {                                              \
            platform_spi_transaction( 1, 0, 0, 32, cache, 0, 0, 0 );    \
            cache = cached = 0;                                         \
        }                                                               \
        cache = (cache << num*8) | dat;                                 \
        cached += num;                                                  \
        arg--;                                                          \
    }                                                                   \
    if (cached > 0) {                                                   \
        platform_spi_transaction( 1, 0, 0, cached * 8, cache, 0, 0, 0 ); \
    }


static int16_t ucg_com_esp8266_hw_spi(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data)
{
  switch(msg)
  {
    case UCG_COM_MSG_POWER_UP:
        /* "data" is a pointer to ucg_com_info_t structure with the following information: */
        /*	((ucg_com_info_t *)data)->serial_clk_speed value in nanoseconds */
        /*	((ucg_com_info_t *)data)->parallel_clk_speed value in nanoseconds */
      
        /* setup pins */
    
        // we assume that the SPI interface was already initialized
        // just care for the /CS and D/C pins
        //platform_gpio_write( ucg->pin_list[0], value );

        if ( ucg->pin_list[UCG_PIN_RST] != UCG_PIN_VAL_NONE )
            platform_gpio_mode( ucg->pin_list[UCG_PIN_RST], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
        platform_gpio_mode( ucg->pin_list[UCG_PIN_CD], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
      
        if ( ucg->pin_list[UCG_PIN_CS] != UCG_PIN_VAL_NONE )
            platform_gpio_mode( ucg->pin_list[UCG_PIN_CS], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
      
        break;

    case UCG_COM_MSG_POWER_DOWN:
        break;

    case UCG_COM_MSG_DELAY:
        delayMicroseconds(arg);
        break;

    case UCG_COM_MSG_CHANGE_RESET_LINE:
        if ( ucg->pin_list[UCG_PIN_RST] != UCG_PIN_VAL_NONE )
            platform_gpio_write( ucg->pin_list[UCG_PIN_RST], arg );
        break;

    case UCG_COM_MSG_CHANGE_CS_LINE:
        if ( ucg->pin_list[UCG_PIN_CS] != UCG_PIN_VAL_NONE )
            platform_gpio_write( ucg->pin_list[UCG_PIN_CS], arg );
        break;

    case UCG_COM_MSG_CHANGE_CD_LINE:
        platform_gpio_write( ucg->pin_list[UCG_PIN_CD], arg );
        break;

    case UCG_COM_MSG_SEND_BYTE:
        platform_spi_send( 1, 8, arg );
        break;

    case UCG_COM_MSG_REPEAT_1_BYTE:
        CACHED_TRANSFER(data[0], 1);
        break;

    case UCG_COM_MSG_REPEAT_2_BYTES:
        CACHED_TRANSFER((data[0] << 8) | data[1], 2);
        break;

    case UCG_COM_MSG_REPEAT_3_BYTES:
        while( arg > 0 ) {
            platform_spi_transaction( 1, 0, 0, 24, (data[0] << 16) | (data[1] << 8) | data[2], 0, 0, 0 );
            arg--;
        }
        break;

    case UCG_COM_MSG_SEND_STR:
        CACHED_TRANSFER(*data++, 1);
        break;

    case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
        while(arg > 0)
        {
            if ( *data != 0 )
            {
                /* set the data line directly, ignore the setting from UCG_CFG_CD */
                if ( *data == 1 )
                {
                    platform_gpio_write( ucg->pin_list[UCG_PIN_CD], 0 );
                }
                else
                {
                    platform_gpio_write( ucg->pin_list[UCG_PIN_CD], 1 );
                }
            }
            data++;
            platform_spi_send( 1, 8, *data );
            data++;
            arg--;
        }
        break;
  }
  return 1;
}



// device destructor
static int lucg_close_display( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    return 0;
}


// ***************************************************************************
// Device constructors
//
//
#undef UCG_DISPLAY_TABLE_ENTRY
#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension)       \
    static int lucg_ ## binding( lua_State *L )                   \
    {                                                             \
        unsigned cs = luaL_checkinteger( L, 1 );                  \
        if (cs == 0)                                              \
            return luaL_error( L, "CS pin required" );            \
        unsigned dc = luaL_checkinteger( L, 2 );                  \
        if (dc == 0)                                              \
            return luaL_error( L, "D/C pin required" );           \
        unsigned res = luaL_optinteger( L, 3, UCG_PIN_VAL_NONE ); \
                                                                        \
        lucg_userdata_t *lud = (lucg_userdata_t *) lua_newuserdata( L, sizeof( lucg_userdata_t ) ); \
                                                                        \
        /* do a dummy init so that something usefull is part of the ucg structure */ \
        ucg_Init( LUCG, ucg_dev_default_cb, ucg_ext_none, (ucg_com_fnptr)0 ); \
                                                                        \
        /* reset cursor position */                                     \
        lud->tx   = 0;                                                  \
        lud->ty   = 0;                                                  \
        lud->tdir = 0;  /* default direction */                         \
                                                                        \
        uint8_t i;                                                      \
        for( i = 0; i < UCG_PIN_COUNT; i++ )                            \
            lud->ucg.pin_list[i] = UCG_PIN_VAL_NONE;                    \
                                                                        \
        lud->dev_cb = device;                                           \
        lud->ext_cb = extension;                                        \
        lud->ucg.pin_list[UCG_PIN_RST] = res;                           \
        lud->ucg.pin_list[UCG_PIN_CD]  = dc;                            \
        lud->ucg.pin_list[UCG_PIN_CS]  = cs;                            \
                                                                        \
        /* set its metatable */                                         \
        luaL_getmetatable(L, "ucg.display");                            \
        lua_setmetatable(L, -2);                                        \
                                                                        \
        return 1;                                                       \
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
#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension) { LSTRKEY( #binding ), LFUNCVAL ( lucg_ ##binding ) },
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
