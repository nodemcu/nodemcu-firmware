// Module for interfacing with PWM

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"

// Lua: realfrequency = setup( id, frequency, duty )
static int lpwm_setup( lua_State* L )
{
  s32 freq;	  // signed, to error check for negative values
  unsigned duty;
  unsigned id;
  
  id = luaL_checkinteger( L, 1 );
  if(id==0)
    return luaL_error( L, "no pwm for D0" );
  MOD_CHECK_ID( pwm, id );
  freq = luaL_checkinteger( L, 2 );
  if ( freq <= 0 )
    return luaL_error( L, "wrong arg range" );
  duty = luaL_checkinteger( L, 3 );
  if ( duty > NORMAL_PWM_DEPTH )
    // Negative values will turn out > 100, so will also fail.
    return luaL_error( L, "wrong arg range" );
  freq = platform_pwm_setup( id, (u32)freq, duty );
  if(freq==0)
    return luaL_error( L, "too many pwms." );
  lua_pushinteger( L, freq );
  return 1;  
}

// Lua: close( id )
static int lpwm_close( lua_State* L )
{
  unsigned id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  platform_pwm_close( id );
  return 0;  
}

// Lua: start( id )
static int lpwm_start( lua_State* L )
{
  unsigned id;
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  if (!platform_pwm_start( id )) {
    return luaL_error(L, "Unable to start PWM output");
  }
  return 0;  
}

// Lua: stop( id )
static int lpwm_stop( lua_State* L )
{
  unsigned id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  platform_pwm_stop( id );
  return 0;  
}

// Lua: realclock = setclock( id, clock )
static int lpwm_setclock( lua_State* L )
{
  unsigned id;
  s32 clk;	// signed to error-check for negative values
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  clk = luaL_checkinteger( L, 2 );
  if ( clk <= 0 )
    return luaL_error( L, "wrong arg range" );
  clk = platform_pwm_set_clock( id, (u32)clk );
  lua_pushinteger( L, clk );
  return 1;
}

// Lua: clock = getclock( id )
static int lpwm_getclock( lua_State* L )
{
  unsigned id;
  u32 clk;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  clk = platform_pwm_get_clock( id );
  lua_pushinteger( L, clk );
  return 1;
}

// Lua: realduty = setduty( id, duty )
static int lpwm_setduty( lua_State* L )
{
  unsigned id;
  s32 duty;  // signed to error-check for negative values
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  duty = luaL_checkinteger( L, 2 );
  if ( duty > NORMAL_PWM_DEPTH )
    return luaL_error( L, "wrong arg range" );
  duty = platform_pwm_set_duty( id, (u32)duty );
  lua_pushinteger( L, duty );
  return 1;
}

// Lua: duty = getduty( id )
static int lpwm_getduty( lua_State* L )
{
  unsigned id;
  u32 duty;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( pwm, id );
  duty = platform_pwm_get_duty( id );
  lua_pushinteger( L, duty );
  return 1;
}

int lpwm_open( lua_State *L ) {
  platform_pwm_init();
  return 0;
}

// Module function map
static const LUA_REG_TYPE pwm_map[] = {
  { LSTRKEY( "setup" ),    LFUNCVAL( lpwm_setup ) },
  { LSTRKEY( "close" ),    LFUNCVAL( lpwm_close ) },
  { LSTRKEY( "start" ),    LFUNCVAL( lpwm_start ) },
  { LSTRKEY( "stop" ),     LFUNCVAL( lpwm_stop ) },
  { LSTRKEY( "setclock" ), LFUNCVAL( lpwm_setclock ) },
  { LSTRKEY( "getclock" ), LFUNCVAL( lpwm_getclock ) },
  { LSTRKEY( "setduty" ),  LFUNCVAL( lpwm_setduty ) },
  { LSTRKEY( "getduty" ),  LFUNCVAL( lpwm_getduty ) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(PWM, "pwm", pwm_map, lpwm_open);
