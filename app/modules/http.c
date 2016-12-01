/******************************************************************************
 * HTTP module for NodeMCU
 * vowstar@gmail.com
 * 2015-12-29
*******************************************************************************/
#include <c_stdlib.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "httpclient.h"

static int http_callback_registry  = LUA_NOREF;

static void http_callback( char * response, int http_status, char ** full_response_p )
{
  const char *full_response = full_response_p ? *full_response_p : NULL;

#if defined(HTTPCLIENT_DEBUG_ON)
  dbg_printf( "http_status=%d\n", http_status );
  if ( http_status != HTTP_STATUS_GENERIC_ERROR )
  {
    if (full_response != NULL) {
      dbg_printf( "strlen(full_response)=%d\n", strlen( full_response ) );
    }
    dbg_printf( "response=%s<EOF>\n", response );
  }
#endif
  if (http_callback_registry != LUA_NOREF)
  {
    lua_State *L = lua_getstate();

    lua_rawgeti(L, LUA_REGISTRYINDEX, http_callback_registry);

    lua_pushnumber(L, http_status);
    if ( http_status != HTTP_STATUS_GENERIC_ERROR && response)
    {
      lua_pushstring(L, response);
      lua_newtable(L);

      const char *p = full_response;

      // Need to skip the HTTP/1.1 header line
      while (*p && *p != '\n') {
        p++;
      }
      if (*p == '\n') {
        p++;
      }

      while (*p && *p != '\r' && *p != '\n') {
        const char *eol = p;
        while (*eol && *eol != '\r') {
          eol++;
        }

        const char *colon = p;
        while (*colon != ':' && colon < eol) {
          colon++;
        }

        if (*colon != ':') {
          break;
        }

        const char *value = colon + 1;
        while (*value == ' ') {
          value++;
        }

        luaL_Buffer b;
        luaL_buffinit(L, &b);
        while (p < colon) {
          luaL_addchar(&b, tolower((unsigned char) *p));
          p++;
        }
        luaL_pushresult(&b);

        lua_pushlstring(L, value, eol - value);
        lua_settable(L, -3);

        p = eol + 1;
        if (*p == '\n') {
          p++;
        }
      }
    }
    else
    {
      lua_pushnil(L);
      lua_pushnil(L);
    }

    if (full_response_p && *full_response_p) {
      c_free(*full_response_p);
      *full_response_p = NULL;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = LUA_NOREF;

    lua_call(L, 3, 0); // With 3 arguments and 0 result
  }
}

// Lua: http.request( url, method, header, body, function(status, reponse) end )
static int http_lapi_request( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * method  = luaL_checklstring(L, 2, &length);
  const char * headers = NULL;
  const char * body    = NULL;

  // Check parameter
  if ((url == NULL) || (method == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_isstring(L, 3))
  {
    headers = luaL_checklstring(L, 3, &length);
  }
  if (lua_isstring(L, 4))
  {
    body = luaL_checklstring(L, 4, &length);
  }

  if (lua_type(L, 5) == LUA_TFUNCTION || lua_type(L, 5) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 5);  // copy argument (func) to the top of stack
    luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_request(url, method, headers, body, http_callback, 0);
  return 0;
}

// Lua: http.post( url, header, body, function(status, reponse) end )
static int http_lapi_post( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * headers = NULL;
  const char * body    = NULL;

  // Check parameter
  if ((url == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_isstring(L, 2))
  {
    headers = luaL_checklstring(L, 2, &length);
  }
  if (lua_isstring(L, 3))
  {
    body = luaL_checklstring(L, 3, &length);
  }

  if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 4);  // copy argument (func) to the top of stack
    if (http_callback_registry != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_post(url, headers, body, http_callback);
  return 0;
}

// Lua: http.put( url, header, body, function(status, reponse) end )
static int http_lapi_put( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * headers = NULL;
  const char * body    = NULL;

  // Check parameter
  if ((url == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_isstring(L, 2))
  {
    headers = luaL_checklstring(L, 2, &length);
  }
  if (lua_isstring(L, 3))
  {
    body = luaL_checklstring(L, 3, &length);
  }

  if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 4);  // copy argument (func) to the top of stack
    if (http_callback_registry != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_put(url, headers, body, http_callback);
  return 0;
}

// Lua: http.delete( url, header, body, function(status, reponse) end )
static int http_lapi_delete( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * headers = NULL;
  const char * body    = NULL;

  // Check parameter
  if ((url == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_isstring(L, 2))
  {
    headers = luaL_checklstring(L, 2, &length);
  }
  if (lua_isstring(L, 3))
  {
    body = luaL_checklstring(L, 3, &length);
  }

  if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 4);  // copy argument (func) to the top of stack
    if (http_callback_registry != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_delete(url, headers, body, http_callback);
  return 0;
}

// Lua: http.get( url, header, function(status, reponse) end )
static int http_lapi_get( lua_State *L )
{
  int length;
  const char * url     = luaL_checklstring(L, 1, &length);
  const char * headers = NULL;

  // Check parameter
  if ((url == NULL))
  {
    return luaL_error( L, "wrong arg type" );
  }

  if (lua_isstring(L, 2))
  {
    headers = luaL_checklstring(L, 2, &length);
  }

  if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 3);  // copy argument (func) to the top of stack
    if (http_callback_registry != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, http_callback_registry);
    http_callback_registry = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  http_get(url, headers, http_callback);
  return 0;
}

// Module function map
static const LUA_REG_TYPE http_map[] = {
  { LSTRKEY( "request" ),         LFUNCVAL( http_lapi_request ) },
  { LSTRKEY( "post" ),            LFUNCVAL( http_lapi_post ) },
  { LSTRKEY( "put" ),             LFUNCVAL( http_lapi_put ) },
  { LSTRKEY( "delete" ),          LFUNCVAL( http_lapi_delete ) },
  { LSTRKEY( "get" ),             LFUNCVAL( http_lapi_get ) },

  { LSTRKEY( "OK" ),              LNUMVAL( 0 ) },
  { LSTRKEY( "ERROR" ),           LNUMVAL( HTTP_STATUS_GENERIC_ERROR ) },
  
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(HTTP, "http", http_map, NULL);
