// Module for interfacing with adc

#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrodefs.h"

#include "c_types.h"
#include "user_interface.h"

// Lua: read(id) , return system adc
static int adc_sample( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( adc, id );
  unsigned val = 0xFFFF & system_adc_read();
  lua_pushinteger( L, val );
  return 1; 
}

// Lua: readvdd33()
static int adc_readvdd33( lua_State* L )
{
  lua_pushinteger(L, system_get_vdd33 ());
  return 1;
}

// Module function map
const LUA_REG_TYPE adc_map[] = 
{
  { LSTRKEY( "read" ), LFUNCVAL( adc_sample ) },
  { LSTRKEY( "readvdd33" ), LFUNCVAL( adc_readvdd33) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_adc( lua_State *L )
{
  return 0;
}
