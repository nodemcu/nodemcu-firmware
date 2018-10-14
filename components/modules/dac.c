// Module for interfacing with dac hardware

#include <string.h>

#include "module.h"
#include "lauxlib.h"

#include "driver/gpio.h"
#include "driver/dac.h"


#define GET_CHN(idx) \
  int chn = luaL_checkint( L, idx ); \
  luaL_argcheck( L, chn >= DAC_CHANNEL_1 && chn <= DAC_CHANNEL_MAX, idx, "invalid channel" );

// Lua: enable( dac_channel )
static int ldac_enable( lua_State *L )
{
  GET_CHN(1);

  if (dac_output_enable( chn ) != ESP_OK)
    return luaL_error( L, "dac failed" );

  return 0;
}

// Lua: disable( dac_channel )
static int ldac_disable( lua_State *L )
{
  GET_CHN(1);

  if (dac_output_disable( chn ) != ESP_OK)
    return luaL_error( L, "dac failed" );

  return 0;
}

// Lua: write( dac_channel )
static int ldac_write( lua_State *L )
{
  GET_CHN(1);

  int data = luaL_checkint( L, 2 );
  luaL_argcheck( L, data >= 0 && data <= 255, 2, "out of range" );

  if (dac_output_voltage( chn, data ) != ESP_OK)
    return luaL_error( L, "dac failed" );

  return 0;
}



// Module function map
static const LUA_REG_TYPE dac_map[] =
{
  { LSTRKEY( "enable" ),  LFUNCVAL( ldac_enable ) },
  { LSTRKEY( "disable" ), LFUNCVAL( ldac_disable ) },
  { LSTRKEY( "write" ),   LFUNCVAL( ldac_write ) },

  { LSTRKEY( "CHANNEL_1" ), LNUMVAL( DAC_CHANNEL_1 ) },
  { LSTRKEY( "CHANNEL_2" ), LNUMVAL( DAC_CHANNEL_2 ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(DAC, "dac", dac_map, NULL);
