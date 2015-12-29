// Module for interfacing with the DHTxx sensors (xx = 11-21-22-33-44).

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "httpclient.h"

static int http_callback_registry  = LUA_NOREF;
static lua_State * http_client_L = NULL;

static void http_callback( char * response, int http_status, char * full_response )
{
  os_printf( "http_status=%d\n", http_status );
  if ( http_status != HTTP_STATUS_GENERIC_ERROR )
  {
    os_printf( "strlen(full_response)=%d\n", strlen( full_response ) );
    os_printf( "response=%s<EOF>\n", response );
  }
  if (http_callback_registry != LUA_NOREF)
  {
    lua_rawgeti(http_client_L, LUA_REGISTRYINDEX, http_callback_registry);

    lua_pushnumber(http_client_L, http_status);
    if ( http_status != HTTP_STATUS_GENERIC_ERROR )
    {
      lua_pushstring(http_client_L, response);
    }
    else
    {
      lua_pushnil(http_client_L);
    }
    lua_call(http_client_L, 2, 0); // With 2 arguments and 0 result

    luaL_unref(http_client_L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = LUA_NOREF;
  }
}

// Lua: result = http.request( method, header, body, function(status, reponse) )
static int http_lapi_request( lua_State *L )
{
  int length;
  http_client_L        = L;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * method  = luaL_checklstring(L, 2, &length);
  const char * headers = luaL_checklstring(L, 3, &length);
  const char * body    = luaL_checklstring(L, 4, &length);
  if ((url == NULL) || (method == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_type(L, 5) == LUA_TFUNCTION || lua_type(L, 5) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 5);  // copy argument (func) to the top of stack
    if (http_callback_registry != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_request(url, method, headers, body, http_callback);
  return 0;
}

// Module function map
static const LUA_REG_TYPE http_map[] = {
  { LSTRKEY( "request" ),         LFUNCVAL( http_lapi_request ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(HTTP, "http", http_map, NULL);
