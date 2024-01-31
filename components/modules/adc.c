// Module for interfacing with adc hardware

#include "module.h"
#include "lauxlib.h"
#include "platform.h"


// Lua: config( adc_id, bits )
static int adc_set_width( lua_State *L )
{
  int adc_id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( adc, adc_id );
  int bits = luaL_checkinteger( L, 2 );

  if (!platform_adc_set_width( adc_id, bits ))
    luaL_error( L, "adc_set_width failed" );

  return 0;
}

// Lua: setup( adc_id, channel, atten )
static int adc_setup( lua_State *L )
{
  int adc_id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( adc, adc_id );
  int channel = luaL_checkinteger( L, 2 );
  if (!platform_adc_channel_exists( adc_id, channel ))
    luaL_error( L, "channel %d does not exist in ADC%d", ( unsigned )channel, ( unsigned )adc_id );

  int atten = luaL_checkinteger( L, 3 );  

  if (!platform_adc_setup( adc_id, channel, atten ))
    luaL_error( L, "adc_setup failed" );

  return 0;
}

// Lua: read( adc_id, channel )
static int adc_read( lua_State *L )
{
  int adc_id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( adc, adc_id );
  int channel = luaL_checkinteger( L, 2 );
  if (!platform_adc_channel_exists( adc_id, channel ))
    luaL_error( L, "channel %d does not exist in ADC%d", ( unsigned )channel, ( unsigned )adc_id );

  int sample = platform_adc_read( adc_id, channel );
  if (sample == -1)
    luaL_error( L, "adc_read failed" );
  lua_pushinteger( L, ( lua_Integer ) sample );
  return 1;
}

// Lua: read_hall_sensor( )
static int read_hall_sensor( lua_State *L )
{
  const char *msg = "ADC hall sensor no longer supported in IDF, sorry";
  platform_print_deprecation_note("msg", "IDFv5");
  return luaL_error(L, msg);
}

// Module function map
LROT_BEGIN(adc, NULL, 0)
  LROT_FUNCENTRY( setwidth,         adc_set_width )
  LROT_FUNCENTRY( setup,            adc_setup )
  LROT_FUNCENTRY( read,             adc_read )
  LROT_FUNCENTRY( read_hall_sensor, read_hall_sensor )
  LROT_NUMENTRY ( ATTEN_0db,        PLATFORM_ADC_ATTEN_0db )
  LROT_NUMENTRY ( ATTEN_2_5db,      PLATFORM_ADC_ATTEN_2_5db )
  LROT_NUMENTRY ( ATTEN_6db,        PLATFORM_ADC_ATTEN_6db )
  LROT_NUMENTRY ( ATTEN_11db,       PLATFORM_ADC_ATTEN_11db )
  LROT_NUMENTRY ( ADC1,             1 )
LROT_END(adc, NULL, 0)

NODEMCU_MODULE(ADC, "adc", adc, NULL);
