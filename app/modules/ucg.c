// Module for Ucglib

#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_stdlib.h"

#include "ucg.h"

//#include "u8g_config.h"

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
static void lucg_get_int_args( lua_State *L, uint8_t stack, uint8_t num, uint8_t *args)
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

// Lua: ucg.setColor( self, [idx], r, g, b )
static int lucg_setColor( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    uint8_t args[3];
    lucg_get_int_args( L, 2, 3, args );

    int16_t opt = luaL_optint( L, (1+3) + 1, -1 );

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

// Lua: ucg.setPrintPos( self, x, y )
static int lucg_setPrintPos( lua_State *L )
{
    lucg_userdata_t *lud;

    if ((lud = get_lud( L )) == NULL)
        return 0;

    uint8_t args[2];
    lucg_get_int_args( L, 2, 2, args );

    lud->tx = args[0];
    lud->ty = args[1];

    return 0;
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
        platform_spi_send_recv( 1, arg );
        break;

    case UCG_COM_MSG_REPEAT_1_BYTE:
        while( arg > 0 ) {
            platform_spi_send_recv( 1, data[0] );
            arg--;
        }
        break;

    case UCG_COM_MSG_REPEAT_2_BYTES:
        while( arg > 0 ) {
            platform_spi_send_recv( 1, data[0] );
            platform_spi_send_recv( 1, data[1] );
            arg--;
        }
        break;

    case UCG_COM_MSG_REPEAT_3_BYTES:
        while( arg > 0 ) {
            platform_spi_send_recv( 1, data[0] );
            platform_spi_send_recv( 1, data[1] );
            platform_spi_send_recv( 1, data[2] );
            arg--;
        }
        break;

    case UCG_COM_MSG_SEND_STR:
        while( arg > 0 ) {
            platform_spi_send_recv( 1, *data++ );
            arg--;
        }
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
            platform_spi_send_recv( 1, *data );
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


static int lucg_ili9341_18x240x320_hw_spi( lua_State *L )
{
    unsigned cs = luaL_checkinteger( L, 1 );
    if (cs == 0)
        return luaL_error( L, "CS pin required" );
    unsigned dc = luaL_checkinteger( L, 2 );
    if (dc == 0)
        return luaL_error( L, "D/C pin required" );
    unsigned res = luaL_optinteger( L, 3, UCG_PIN_VAL_NONE );

    lucg_userdata_t *lud = (lucg_userdata_t *) lua_newuserdata( L, sizeof( lucg_userdata_t ) );

    // do a dummy init so that something usefull is part of the ucg structure
    ucg_Init( LUCG, ucg_dev_default_cb, ucg_ext_none, (ucg_com_fnptr)0 );

    // reset cursor position
    lud->tx   = 0;
    lud->ty   = 0;
    lud->tdir = 0;  // default direction

    uint8_t i;
    for( i = 0; i < UCG_PIN_COUNT; i++ )
        lud->ucg.pin_list[i] = UCG_PIN_VAL_NONE;


    lud->dev_cb = ucg_dev_ili9341_18x240x320;
    lud->ext_cb = ucg_ext_ili9341_18;
    lud->ucg.pin_list[UCG_PIN_RST] = res;
    lud->ucg.pin_list[UCG_PIN_CD]  = dc;
    lud->ucg.pin_list[UCG_PIN_CS]  = cs;


    /* set its metatable */
    luaL_getmetatable(L, "ucg.display");
    lua_setmetatable(L, -2);

    return 1;
}



// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE lucg_display_map[] =
{
    { LSTRKEY( "begin" ),       LFUNCVAL( lucg_begin ) },
    { LSTRKEY( "clearScreen" ), LFUNCVAL( lucg_clearScreen ) },
    { LSTRKEY( "setColor" ),    LFUNCVAL( lucg_setColor ) },
    { LSTRKEY( "setFont" ),     LFUNCVAL( lucg_setFont ) },
    { LSTRKEY( "print" ),       LFUNCVAL( lucg_print ) },
    { LSTRKEY( "setPrintPos" ), LFUNCVAL( lucg_setPrintPos ) },

    { LSTRKEY( "__gc" ),  LFUNCVAL( lucg_close_display ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "__index" ), LROVAL ( lucg_display_map ) },
#endif
    { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE lucg_map[] = 
{
    { LSTRKEY( "ili9341_18x240x320_hw_spi" ), LFUNCVAL( lucg_ili9341_18x240x320_hw_spi ) },

#if LUA_OPTIMIZE_MEMORY > 0

    // Register fonts
    { LSTRKEY( "font_ncenR12_tr" ), LUDATA( (void *)(ucg_font_ncenR12_tr) ) },

    // Font modes
    { LSTRKEY( "FONT_MODE_TRANSPARENT" ), LNUMVAL( UCG_FONT_MODE_TRANSPARENT ) },
    { LSTRKEY( "FONT_MODE_SOLID" ),       LNUMVAL( UCG_FONT_MODE_SOLID ) },

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
    MOD_REG_LUDATA( L, "font_ncenR12_tr", (void *)(ucg_font_ncenR12_tr) );

    // Font modes
    MOD_REG_NUMBER( L, "FONT_MODE_TRANSPARENT", UCG_FONT_MODE_TRANSPARENT );
    MOD_REG_NUMBER( L, "FONT_MODE_SOLID",       UCG_FONT_MODE_SOLID );

    // create metatable
    luaL_newmetatable(L, "ucg.display");
    // metatable.__index = metatable
    lua_pushliteral(L, "__index");
    lua_pushvalue(L,-2);
    lua_rawset(L,-3);
    // Setup the methods inside metatable
    luaL_register( L, NULL, lucg_display_map );

    return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
