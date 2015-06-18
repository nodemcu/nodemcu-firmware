// Module for interfacing with the DHTxx sensors (xx = 11-21-22-33-44).

#include "lualib.h"
#include "lauxlib.h"
#include "auxmods.h"
#include "lrotable.h"
#include "cpu_esp8266.h"
#include "dht.h"

#define NUM_DHT GPIO_PIN_NUM

// ****************************************************************************
// DHT functions
int platform_dht_exists( unsigned id )
{
  return ((id < NUM_DHT) && (id > 0));
}

// Lua: result = dht.read( id )
static int dht_lapi_read( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read(id) );
  return 1;
}

// Lua: result = dht.read11( id )
static int dht_lapi_read11( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read11(id) );
  return 1;
}

// Lua: result = dht.humidity()
static int dht_lapi_humidity( lua_State *L )
{
  lua_pushinteger( L, dht_getHumidity() );
  return 1;
}

// Lua: result = dht.temperature()
static int dht_lapi_temperature( lua_State *L )
{
  lua_pushinteger( L, dht_getTemperature() );
  return 1;
}


// Module function map
#define MIN_OPT_LEVEL   2
#include "lrodefs.h"
const LUA_REG_TYPE dht_map[] =
{
  { LSTRKEY( "read" ),  LFUNCVAL( dht_lapi_read ) },
  { LSTRKEY( "read11" ), LFUNCVAL( dht_lapi_read11 ) },
  { LSTRKEY( "humidity" ),  LFUNCVAL( dht_lapi_humidity ) },
  { LSTRKEY( "temperature" ), LFUNCVAL( dht_lapi_temperature ) },
#if LUA_OPTIMIZE_MEMORY > 0
  { LSTRKEY( "OK" ), LNUMVAL( DHTLIB_OK ) },
  { LSTRKEY( "ERROR_CHECKSUM" ), LNUMVAL( DHTLIB_ERROR_CHECKSUM ) },
  { LSTRKEY( "ERROR_TIMEOUT" ), LNUMVAL( DHTLIB_ERROR_TIMEOUT ) },
#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_dht( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_DHT, dht_map );

  // Add the constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0
}
