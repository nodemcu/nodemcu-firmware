// Module for interfacing with the DHTxx sensors (xx = 11-21-22-33-44).

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "httpclient.h"

// Lua: result = dht.request( method, header, body, callback )
static int http_lapi_request( lua_State *L )
{
  lua_pushnumber( L, 0 );
  return 1;
}

// Module function map
static const LUA_REG_TYPE http_map[] = {
  { LSTRKEY( "request" ),         LFUNCVAL( http_lapi_request ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(DHT, "http", http_map, NULL);
