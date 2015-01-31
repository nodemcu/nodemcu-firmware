// Module for U8glib

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

//#include "c_string.h"
//#include "c_stdlib.h"

#include "u8g.h"

typedef struct lu8g_userdata
{
    uint8_t i2c_addr;
    u8g_t u8g;
} lu8g_userdata_t;


// Font look-up array
#define LU8G_FONT_6X10 0

const static u8g_fntpgm_uint8_t *font_array[] =
{
    u8g_font_6x10
};


// Lua: u8g.setFont( self, font )
static int lu8g_setFont( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    unsigned fontnr = luaL_checkinteger( L, stack );
    stack++;

    u8g_SetFont( &(lud->u8g), font_array[fontnr] );

    return 0;
}

// Lua: u8g.setFontRefHeightExtendedText( self )
static int lu8g_setFontRefHeightExtendedText( lua_State *L )
{
    lu8g_userdata_t *lud;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, 1, "u8g.display");
    luaL_argcheck(L, lud, 1, "u8g.display expected");

    if (lud == NULL)
        return 0;

    u8g_SetFontRefHeightExtendedText( &(lud->u8g) );

    return 0;
}

// Lua: u8g.setDefaultForegroundColor( self )
static int lu8g_setDefaultForegroundColor( lua_State *L )
{
    lu8g_userdata_t *lud;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, 1, "u8g.display");
    luaL_argcheck(L, lud, 1, "u8g.display expected");

    if (lud == NULL)
        return 0;

    u8g_SetDefaultForegroundColor( &(lud->u8g) );

    return 0;
}

// Lua: u8g.setFontPosTop( self )
static int lu8g_setFontPosTop( lua_State *L )
{
    lu8g_userdata_t *lud;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, 1, "u8g.display");
    luaL_argcheck(L, lud, 1, "u8g.display expected");

    if (lud == NULL)
        return 0;

    u8g_SetFontPosTop( &(lud->u8g) );

    return 0;
}

// Lua: pix_len = u8g.drawStr( self, x, y, string )
static int lu8g_drawStr( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    u8g_uint_t x = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t y = luaL_checkinteger( L, stack );
    stack++;

    const char *s = luaL_checkstring( L, stack );
    stack++;
    if (s == NULL)
        return 0;

    lua_pushinteger( L, u8g_DrawStr( &(lud->u8g), x, y, s ) );

    return 1;
}

// Lua: u8g.drawBox( self, x, y, width, height )
static int lu8g_drawBox( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    u8g_uint_t x = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t y = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t w = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t h = luaL_checkinteger( L, stack );
    stack++;

    u8g_DrawBox( &(lud->u8g), x, y, w, h );

    return 0;
}

// Lua: u8g.drawFrame( self, x, y, width, height )
static int lu8g_drawFrame( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    u8g_uint_t x = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t y = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t w = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t h = luaL_checkinteger( L, stack );
    stack++;

    u8g_DrawFrame( &(lud->u8g), x, y, w, h );

    return 0;
}

// Lua: u8g.drawDisc( self, x0, y0, rad, opt = U8G_DRAW_ALL )
static int lu8g_drawDisc( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    u8g_uint_t x0 = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t y0 = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t rad = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t opt = luaL_optinteger( L, stack, U8G_DRAW_ALL );
    stack++;

    u8g_DrawDisc( &(lud->u8g), x0, y0, rad, opt );

    return 0;
}

// Lua: u8g.drawCircle( self, x0, y0, rad, opt = U8G_DRAW_ALL )
static int lu8g_drawCircle( lua_State *L )
{
    lu8g_userdata_t *lud;
    uint8_t stack = 1;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, stack, "u8g.display");
    luaL_argcheck(L, lud, stack, "u8g.display expected");
    stack++;

    if (lud == NULL)
        return 0;

    u8g_uint_t x0 = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t y0 = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t rad = luaL_checkinteger( L, stack );
    stack++;

    u8g_uint_t opt = luaL_optinteger( L, stack, U8G_DRAW_ALL );
    stack++;

    u8g_DrawCircle( &(lud->u8g), x0, y0, rad, opt );

    return 0;
}

// Lua: u8g.firstPage( self )
static int lu8g_firstPage( lua_State *L )
{
    lu8g_userdata_t *lud;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, 1, "u8g.display");
    luaL_argcheck(L, lud, 1, "u8g.display expected");

    if (lud == NULL)
        return 0;

    u8g_FirstPage( &(lud->u8g) );

    return 0;
}

// Lua: bool = u8g.nextPage( self )
static int lu8g_nextPage( lua_State *L )
{
    lu8g_userdata_t *lud;

    lud = (lu8g_userdata_t *)luaL_checkudata(L, 1, "u8g.display");
    luaL_argcheck(L, lud, 1, "u8g.display expected");

    if (lud == NULL)
        return 0;

    lua_pushboolean( L, u8g_NextPage( &(lud->u8g) ) );

    return 1;
}

// ------------------------------------------------------------
// comm functions
//
#define I2C_SLA         0x3c
#define I2C_CMD_MODE    0x000
#define I2C_DATA_MODE   0x040

#define ESP_I2C_ID 0


static uint8_t do_i2c_start(uint8_t id, uint8_t sla)
{
    platform_i2c_send_start( id );

    // ignore return value -> tolerate missing ACK
    platform_i2c_send_address( id, sla, PLATFORM_I2C_DIRECTION_TRANSMITTER );

    return 1;
}

static uint8_t u8g_com_esp8266_ssd_start_sequence(u8g_t *u8g)
{
    /* are we requested to set the a0 state? */
    if ( u8g->pin_list[U8G_PI_SET_A0] == 0 )
        return 1;

    /* setup bus, might be a repeated start */
    if ( do_i2c_start( ESP_I2C_ID, I2C_SLA ) == 0 )
        return 0;
    if ( u8g->pin_list[U8G_PI_A0_STATE] == 0 )
    {
        // ignore return value -> tolerate missing ACK
        if ( platform_i2c_send_byte( ESP_I2C_ID, I2C_CMD_MODE ) == 0 )
            ; //return 0;
    }
    else
    {
        platform_i2c_send_byte( ESP_I2C_ID, I2C_DATA_MODE );
    }

    u8g->pin_list[U8G_PI_SET_A0] = 0;
    return 1;
}


uint8_t u8g_com_esp8266_ssd_i2c_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
    switch(msg)
    {
    case U8G_COM_MSG_INIT:
        // we assume that the i2c bus was already initialized
        //u8g_i2c_init(u8g->pin_list[U8G_PI_I2C_OPTION]);

        break;
    
    case U8G_COM_MSG_STOP:
        break;

    case U8G_COM_MSG_RESET:
        /* Currently disabled, but it could be enable. Previous restrictions have been removed */
        /* u8g_com_arduino_digital_write(u8g, U8G_PI_RESET, arg_val); */
        break;
      
    case U8G_COM_MSG_CHIP_SELECT:
        u8g->pin_list[U8G_PI_A0_STATE] = 0;
        u8g->pin_list[U8G_PI_SET_A0] = 1;		/* force a0 to set again, also forces start condition */
        if ( arg_val == 0 )
        {
            /* disable chip, send stop condition */
            platform_i2c_send_stop( ESP_I2C_ID );
        }
        else
        {
            /* enable, do nothing: any byte writing will trigger the i2c start */
        }
        break;

    case U8G_COM_MSG_WRITE_BYTE:
        //u8g->pin_list[U8G_PI_SET_A0] = 1;
        if ( u8g_com_esp8266_ssd_start_sequence(u8g) == 0 )
            return platform_i2c_stop( ESP_I2C_ID ), 0;
        // ignore return value -> tolerate missing ACK
        if ( platform_i2c_send_byte( ESP_I2C_ID, arg_val) == 0 )
            ; //return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        // platform_i2c_send_stop( ESP_I2C_ID );
        break;
    
    case U8G_COM_MSG_WRITE_SEQ:
        //u8g->pin_list[U8G_PI_SET_A0] = 1;
        if ( u8g_com_esp8266_ssd_start_sequence(u8g) == 0 )
            return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        {
            register uint8_t *ptr = arg_ptr;
            while( arg_val > 0 )
            {
                // ignore return value -> tolerate missing ACK
                if ( platform_i2c_send_byte( ESP_I2C_ID, *ptr++) == 0 )
                    ; //return platform_i2c_send_stop( ESP_I2C_ID ), 0;
                arg_val--;
            }
        }
        // platform_i2c_send_stop( ESP_I2C_ID );
        break;

    case U8G_COM_MSG_WRITE_SEQ_P:
        //u8g->pin_list[U8G_PI_SET_A0] = 1;
        if ( u8g_com_esp8266_ssd_start_sequence(u8g) == 0 )
            return platform_i2c_send_stop( ESP_I2C_ID ), 0;
        {
            register uint8_t *ptr = arg_ptr;
            while( arg_val > 0 )
            {
                // ignore return value -> tolerate missing ACK
                if ( platform_i2c_send_byte( ESP_I2C_ID, u8g_pgm_read(ptr) ) == 0 )
                    ; //return 0;
                ptr++;
                arg_val--;
            }
        }
        // platform_i2c_send_stop( ESP_I2C_ID );
        break;
      
    case U8G_COM_MSG_ADDRESS:                     /* define cmd (arg_val = 0) or data mode (arg_val = 1) */
        u8g->pin_list[U8G_PI_A0_STATE] = arg_val;
        u8g->pin_list[U8G_PI_SET_A0] = 1;		/* force a0 to set again */
    
        break;
    }
    return 1;
}





// device constructors

// Lua: speed = u8g.ssd1306_128x64_i2c( i2c_addr )
static int lu8g_ssd1306_128x64_i2c( lua_State *L )
{
    unsigned addr = luaL_checkinteger( L, 1 );

    if (addr == 0)
        return luaL_error( L, "i2c address required" );

    lu8g_userdata_t *userdata = (lu8g_userdata_t *) lua_newuserdata( L, sizeof( lu8g_userdata_t ) );

    userdata->i2c_addr = (uint8_t)addr;

    u8g_InitI2C( &(userdata->u8g), &u8g_dev_ssd1306_128x64_i2c, U8G_I2C_OPT_NONE);


    // set its metatable
    luaL_getmetatable(L, "u8g.display");
    lua_setmetatable(L, -2);

    return 1;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE lu8g_display_map[] =
{
    { LSTRKEY( "setFont" ),  LFUNCVAL( lu8g_setFont ) },
    { LSTRKEY( "setFontRefHeightExtendedText" ),  LFUNCVAL( lu8g_setFontRefHeightExtendedText ) },
    { LSTRKEY( "setDefaultForegroundColor" ),  LFUNCVAL( lu8g_setDefaultForegroundColor ) },
    { LSTRKEY( "setFontPosTop" ),  LFUNCVAL( lu8g_setFontPosTop ) },
    { LSTRKEY( "drawStr" ),  LFUNCVAL( lu8g_drawStr ) },
    { LSTRKEY( "drawBox" ),  LFUNCVAL( lu8g_drawBox ) },
    { LSTRKEY( "drawFrame" ),  LFUNCVAL( lu8g_drawFrame ) },
    { LSTRKEY( "drawDisc" ),  LFUNCVAL( lu8g_drawDisc ) },
    { LSTRKEY( "drawCircle" ),  LFUNCVAL( lu8g_drawCircle ) },
    { LSTRKEY( "firstPage" ),  LFUNCVAL( lu8g_firstPage ) },
    { LSTRKEY( "nextPage" ),  LFUNCVAL( lu8g_nextPage ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "__index" ), LROVAL ( lu8g_display_map ) },
#endif
    { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE lu8g_map[] = 
{
    { LSTRKEY( "ssd1306_128x64_i2c" ), LFUNCVAL ( lu8g_ssd1306_128x64_i2c ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "font_6x10" ), LNUMVAL( LU8G_FONT_6X10 ) },
    { LSTRKEY( "__metatable" ), LROVAL( lu8g_map ) },
#endif
    { LNILKEY, LNILVAL }
};

LUALIB_API int ICACHE_FLASH_ATTR luaopen_u8g( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
    luaL_rometatable(L, "u8g.display", (void *)lu8g_display_map);  // create metatable
    return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
    int n;
    luaL_register( L, AUXLIB_U8G, lu8g_map );

    // Set it as its own metatable
    lua_pushvalue( L, -1 );
    lua_setmetatable( L, -2 );

    // Module constants  
    // Register fonts
    MOD_REG_NUMBER( L, "font_6x10", LU8G_FONT_6X10 ); 

    // create metatable
    luaL_newmetatable(L, "u8g.display");
    // metatable.__index = metatable
    lua_pushliteral(L, "__index");
    lua_pushvalue(L,-2);
    lua_rawset(L,-3);
    // Setup the methods inside metatable
    luaL_register( L, NULL, u8g_display_map );

    return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
