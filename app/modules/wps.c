// Module for WPS
// by F.J. Exoo

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "user_interface.h"

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
    luaL_pcallx(L, 1, 0);
  }
}

// Lua: wps.start( function())
static int ICACHE_FLASH_ATTR wps_start(lua_State* L)
{
  // retrieve callback arg (optional)
  luaL_unref(L, LUA_REGISTRYINDEX, wps_callback_ref);

  wps_callback_ref = LUA_NOREF;

  if (lua_isfunction(L, 1))
    wps_callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    return luaL_error (L, "Argument not a function");

  wifi_set_wps_cb(user_wps_status_cb);
  wifi_wps_start();
  return 0;
}

// Module function map
LROT_BEGIN(wps, NULL, 0)
  LROT_FUNCENTRY( disable, wps_disable )
  LROT_FUNCENTRY( enable, wps_enable )
  LROT_FUNCENTRY( start, wps_start )
  LROT_NUMENTRY( SUCCESS, WPS_CB_ST_SUCCESS )
  LROT_NUMENTRY( FAILED, WPS_CB_ST_FAILED )
  LROT_NUMENTRY( TIMEOUT, WPS_CB_ST_TIMEOUT )
  LROT_NUMENTRY( WEP, WPS_CB_ST_WEP )
  LROT_NUMENTRY( SCAN_ERR, 4 )
LROT_END(wps, NULL, 0)


int luaopen_wps( lua_State *L )
{
  return 0;
}

NODEMCU_MODULE(WPS, "wps", wps, luaopen_wps);

