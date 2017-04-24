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
  int sample = platform_adc_read_hall_sensor( );
  lua_pushinteger( L, ( lua_Integer ) sample );
  return 1;
}

// Module function map
static const LUA_REG_TYPE adc_map[] =
{
  { LSTRKEY( "setwidth" ),       LFUNCVAL( adc_set_width ) },
  { LSTRKEY( "setup" ),       LFUNCVAL( adc_setup ) },
  { LSTRKEY( "read" ),        LFUNCVAL( adc_read ) },
  { LSTRKEY( "read_hall_sensor" ),        LFUNCVAL( read_hall_sensor ) },
  { LSTRKEY( "ATTEN_0db" ),   LNUMVAL( PLATFORM_ADC_ATTEN_0db ) },
  { LSTRKEY( "ATTEN_2_5db" ), LNUMVAL( PLATFORM_ADC_ATTEN_2_5db ) },
  { LSTRKEY( "ATTEN_6db" ),   LNUMVAL( PLATFORM_ADC_ATTEN_6db ) },
  { LSTRKEY( "ATTEN_11db" ),  LNUMVAL( PLATFORM_ADC_ATTEN_11db ) },
  { LSTRKEY( "ADC1" ),  LNUMVAL( 1 ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(ADC, "adc", adc_map, NULL);
