// Module for WPS
// by F.J. Exoo

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

static int wps_callback_ref;

// Lua: wps.disable()
static int ICACHE_FLASH_ATTR wps_disable(lua_State* L)
{
  wifi_wps_disable();
  return 0;
}

// Lua: wps.enable()
static int ICACHE_FLASH_ATTR wps_enable(lua_State* L)
{
  wifi_wps_enable(WPS_TYPE_PBC);
  return 0;
}

// WPS start callback function
LOCAL void ICACHE_FLASH_ATTR user_wps_status_cb(int status)
{
  lua_State *L = lua_getstate();

  if (wps_callback_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, wps_callback_ref);
    lua_pushinteger(L, status);
    lua_call(L, 1, 0);
  }
}

// Lua: wps.start( function())
static int ICACHE_FLASH_ATTR wps_start(lua_State* L)
{
  // retrieve callback arg (optional)
  luaL_unref(L, LUA_REGISTRYINDEX, wps_callback_ref);

  wps_callback_ref = LUA_NOREF;

  if (lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TLIGHTFUNCTION)
    wps_callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    return luaL_error (L, "Argument not a function");

  wifi_set_wps_cb(user_wps_status_cb);
  wifi_wps_start();
  return 0;
}

// Module function map
const LUA_REG_TYPE wps_map[] = {
  { LSTRKEY( "disable" ),  LFUNCVAL( wps_disable ) },
  { LSTRKEY( "enable" ),   LFUNCVAL( wps_enable ) },
  { LSTRKEY( "start" ),    LFUNCVAL( wps_start ) },
  { LSTRKEY( "SUCCESS" ),  LNUMVAL( WPS_CB_ST_SUCCESS ) },
  { LSTRKEY( "FAILED" ),   LNUMVAL( WPS_CB_ST_FAILED ) },
  { LSTRKEY( "TIMEOUT" ),  LNUMVAL( WPS_CB_ST_TIMEOUT ) },
  { LSTRKEY( "WEP" ),      LNUMVAL( WPS_CB_ST_WEP ) },
  { LSTRKEY( "SCAN_ERR" ), LNUMVAL( 4 ) }, // WPS_CB_ST_SCAN_ERR
  { LNILKEY, LNILVAL }
};

int luaopen_wps( lua_State *L )
{
  return 0;
}

NODEMCU_MODULE(WPS, "wps", wps_map, luaopen_wps);

