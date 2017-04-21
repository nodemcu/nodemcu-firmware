

#include "module.h"
#include "lauxlib.h"
#include <stdint.h>

#include "platform.h"


typedef enum {
  LDHT_OK = 0,
  LDHT_ERROR_CHECKSUM = -1,
  LDHT_ERROR_TIMEOUT = -2,
  LDHT_INVALID_VALUE = -999
} ldht_result_t;

typedef enum {
  LDHT11,
  LDHT2X
} ldht_type_t;

static int ldht_compute_data11( uint8_t *data, double *temp, double *humi )
{
  *humi = data[0];
  *temp = data[2];

  uint8_t sum = data[0] + data[1] + data[2] + data[3];
  return sum == data[4] ? LDHT_OK : LDHT_ERROR_CHECKSUM;
}

static int ldht_compute_data2x( uint8_t *data, double *temp, double *humi )
{
  *humi = (data[0] * 256 + data[1]) * 0.1;
  *temp = ((data[2] & 0x7f) * 256 + data[3]) * 0.1;

  if (data[2] & 0x80)
    *temp = - *temp;

  uint8_t sum = data[0] + data[1] + data[2] + data[3];
  return sum == data[4] ? LDHT_OK : LDHT_ERROR_CHECKSUM;
}

static int ldht_read_generic( lua_State *L, ldht_type_t type )
{
  int stack = 0;

  int gpio = luaL_checkint( L, ++stack );
  luaL_argcheck( L, platform_gpio_output_exists( gpio ), stack, "invalid gpio" );

  uint8_t data[5];
  if (platform_dht_read( gpio,
			 type == LDHT11 ? PLATFORM_DHT11_WAKEUP_MS : PLATFORM_DHT2X_WAKEUP_MS,
			 data ) != PLATFORM_OK) {
    lua_pushinteger( L, LDHT_ERROR_TIMEOUT );
    return 1;
  }

  double temp, humi;
  int res;
  switch (type) {
  case LDHT11:
    res = ldht_compute_data11( data, &temp, &humi );
    break;
  case LDHT2X:
    res = ldht_compute_data2x( data, &temp, &humi );
    break;
  default:
    res = LDHT_INVALID_VALUE;
    temp = humi = 0;
    break;
  }
  lua_pushinteger( L, res );
  lua_pushnumber( L, temp );
  lua_pushnumber( L, humi );
  int tempdec = (int)((temp - (int)temp) * 1000);
  int humidec = (int)((humi - (int)humi) * 1000);
  lua_pushinteger( L, tempdec );
  lua_pushinteger( L, humidec );

  return 5;
}

static int ldht_read11( lua_State *L )
{
  return ldht_read_generic( L, LDHT11 );
}

static int ldht_read2x( lua_State *L )
{
  return ldht_read_generic( L, LDHT2X );
}


static const LUA_REG_TYPE dht_map[] = {
  { LSTRKEY( "read11" ),         LFUNCVAL( ldht_read11 ) },
  { LSTRKEY( "read2x" ),         LFUNCVAL( ldht_read2x ) },
  { LSTRKEY( "OK" ),             LNUMVAL( LDHT_OK ) },
  { LSTRKEY( "ERROR_CHECKSUM" ), LNUMVAL( LDHT_ERROR_CHECKSUM ) },
  { LSTRKEY( "ERROR_TIMEOUT" ),  LNUMVAL( LDHT_ERROR_TIMEOUT ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(DHT, "dht", dht_map, NULL);
