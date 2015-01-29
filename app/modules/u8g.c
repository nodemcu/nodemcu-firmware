// Module for U8glib

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_string.h"
#include "c_stdlib.h"

#include "c_types.h"
#include "mem.h"
#include "espconn.h"

typedef struct lu8g_userdata
{
    int some_data;
} lu8g_userdata_t;


static int lu8g_new( lua_State *L )
{
    lu8g_userdata_t *userdata = (lu8g_userdata_t *) lua_newuserdata( L, sizeof( lu8g_userdata_t ) );

    userdata->some_data = 100;

    // set its metatable
    luaL_getmetatable(L, "u8g.display");
    lua_setmetatable(L, -2);

    return 1;
}

// Lua: u8g.setup( self, id )
static int lu8g_setup( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 2 );

  //MOD_CHECK_ID( u8g, id );

  if (id == 0)
    return luaL_error( L, "ID 0 not supported!" );

  return 0;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"

static const LUA_REG_TYPE u8g_display_map[] =
{
    { LSTRKEY( "setup" ),  LFUNCVAL( lu8g_setup ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "__index" ), LROVAL ( u8g_display_map ) },
#endif
    { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE u8g_map[] = 
{
    { LSTRKEY( "new" ), LFUNCVAL ( lu8g_new ) },
#if LUA_OPTIMIZE_MEMORY > 0
    { LSTRKEY( "__metatable" ), LROVAL( u8g_map ) },
#endif
    { LNILKEY, LNILVAL }
};

LUALIB_API int ICACHE_FLASH_ATTR luaopen_u8g( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
    luaL_rometatable(L, "u8g.display", (void *)u8g_display_map);  // create metatable
    return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
    int n;
    luaL_register( L, AUXLIB_U8G, u8g_map );

    // Set it as its own metatable
    lua_pushvalue( L, -1 );
    lua_setmetatable( L, -2 );

    // Module constants  
    // MOD_REG_NUMBER( L, "TCP", TCP );

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
