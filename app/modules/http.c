// Module for interfacing with the DHTxx sensors (xx = 11-21-22-33-44).

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "httpclient.h"

static int http_callback_handle  = LUA_NOREF;
static lua_State * http_client_L = NULL;

static void http_callback( char * response, int http_status, char * full_response )
{
  os_printf( "http_status=%d\n", http_status );
  if ( http_status != HTTP_STATUS_GENERIC_ERROR )
  {
    os_printf( "strlen(full_response)=%d\n", strlen( full_response ) );
    os_printf( "response=%s<EOF>\n", response );
  }
  if (http_callback_handle != LUA_NOREF)
  {
    lua_rawgeti(http_client_L, LUA_REGISTRYINDEX, http_callback_handle);

    lua_pushnumber(http_client_L, http_status);
    lua_pushstring(http_client_L, response);
    lua_call(http_client_L, 2, 0);

    luaL_unref(http_client_L, LUA_REGISTRYINDEX, http_callback_handle);
    http_callback_handle = LUA_NOREF;
  }
}

// Lua: result = http.request( method, header, body, callback )
static int http_lapi_request( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * method  = luaL_checklstring(L, 1, &length);
  const char * headers = luaL_checklstring(L, 1, &length);
  const char * body    = luaL_checklstring(L, 1, &length);

  http_request(url, method, headers, body, NULL);

  lua_pushnumber( L, 0 );
  return 1;
}

// Module function map
static const LUA_REG_TYPE http_map[] = {
  { LSTRKEY( "request" ),         LFUNCVAL( http_lapi_request ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(HTTP, "http", http_map, NULL);
