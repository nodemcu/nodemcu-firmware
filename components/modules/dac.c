// Module for interfacing with dac hardware

#include <string.h>

#include "module.h"
#include "lauxlib.h"

#include "driver/gpio.h"
#include "driver/dac.h"


#define GET_CHN(idx) \
  int chn = luaL_checkint( L, idx ); \
  luaL_argcheck( L, chn >= DAC_CHAN_0 && chn <= DAC_CHAN_1, idx, "invalid channel" );

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
LROT_BEGIN(dac, NULL, 0)
  LROT_FUNCENTRY( enable,    ldac_enable )
  LROT_FUNCENTRY( disable,   ldac_disable )
  LROT_FUNCENTRY( write,     ldac_write )
  LROT_NUMENTRY ( CHANNEL_1, DAC_CHAN_0 )
  LROT_NUMENTRY ( CHANNEL_2, DAC_CHAN_1 )
LROT_END(dac, NULL, 0)

NODEMCU_MODULE(DAC, "dac", dac, NULL);
