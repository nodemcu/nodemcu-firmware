// Module for interfacing with the DHTxx sensors (xx = 11-21-22-33-44).

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "cpu_esp8266.h"
#include "dht/dht.h"

#define NUM_DHT GPIO_PIN_NUM

// ****************************************************************************
// DHT functions
int platform_dht_exists( unsigned id )
{
  return ((id < NUM_DHT) && (id > 0));
}

static void aux_read( lua_State *L )
{
  double temp = dht_getTemperature();
  double humi = dht_getHumidity();
  int tempdec = (int)((temp - (int)temp) * 1000);
  int humidec = (int)((humi - (int)humi) * 1000);
  lua_pushnumber( L, (lua_Float) temp ); 
  lua_pushnumber( L, (lua_Float) humi ); 
  lua_pushinteger( L, tempdec );
  lua_pushinteger( L, humidec );
}

// Lua: status, temp, humi, tempdec, humidec = dht.read( id )
static int dht_lapi_read( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read(id, DHT_NON11) );
  aux_read( L );
  return 5;
}

// Lua: status, temp, humi, tempdec, humidec = dht.read11( id ))
static int dht_lapi_read11( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read(id, DHT11) );
  aux_read( L );
  return 5;
}

// Lua: status, temp, humi, tempdec, humidec = dht.read12( id ))
static int dht_lapi_read12( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read(id, DHT12) );
  aux_read( L );
  return 5;
}

// Lua: status, temp, humi, tempdec, humidec = dht.readxx( id ))
static int dht_lapi_readxx( lua_State *L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( dht, id );
  lua_pushinteger( L, dht_read(id, DHT22) );
  aux_read( L );
  return 5;
}

// Module function map
LROT_BEGIN(dht, NULL, 0)
  LROT_FUNCENTRY( read, dht_lapi_read )
  LROT_FUNCENTRY( read11, dht_lapi_read11 )
  LROT_FUNCENTRY( read12, dht_lapi_read12 )
  LROT_FUNCENTRY( readxx, dht_lapi_read )
  LROT_NUMENTRY( OK, DHTLIB_OK )
  LROT_NUMENTRY( ERROR_CHECKSUM, DHTLIB_ERROR_CHECKSUM )
  LROT_NUMENTRY( ERROR_TIMEOUT, DHTLIB_ERROR_TIMEOUT )
LROT_END(dht, NULL, 0)


NODEMCU_MODULE(DHT, "dht", dht, NULL);
