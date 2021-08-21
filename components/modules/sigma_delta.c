// Module for interfacing with sigma-delta hardware

#include "module.h"
#include "lauxlib.h"
#include "platform.h"


// Lua: setup( channel, pin )
static int sigma_delta_setup( lua_State *L )
{
  int channel = luaL_checkinteger( L, 1 );
  int pin = luaL_checkinteger( L, 2 );

  MOD_CHECK_ID(sigma_delta, channel);

  if (!platform_sigma_delta_setup( channel, pin ))
    luaL_error( L, "command failed" );

  return 0;
}

// Lua: close( channel )
static int sigma_delta_close( lua_State *L )
{
  int channel = luaL_checkinteger( L, 1 );

  MOD_CHECK_ID(sigma_delta, channel);

  platform_sigma_delta_close( channel );

  return 0;
}

#if 0
// Lua: setpwmduty( channel, duty_cycle )
static int sigma_delta_setpwmduty( lua_State *L )
{
  int channel = luaL_checkinteger( L, 1 );
  int duty = luaL_checkinteger( L, 2 );

  MOD_CHECK_ID(sigma_delta, channel);

  if (!platform_sigma_delta_set_pwmduty( channel, duty ))
    luaL_error( L, "command failed" );

  return 0;
}
#endif

// Lua: setprescale( channel, value )
static int sigma_delta_setprescale( lua_State *L )
{
  int channel = luaL_checkinteger( L, 1 );
  int prescale = luaL_checkinteger( L, 2 );

  MOD_CHECK_ID(sigma_delta, channel);

  if (!platform_sigma_delta_set_prescale( channel, prescale ))
    luaL_error( L, "command failed" );

  return 0;
}

// Lua: setduty( channel, value )
static int sigma_delta_setduty( lua_State *L )
{
  int channel = luaL_checkinteger( L, 1 );
  int duty = luaL_checkinteger( L, 2 );

  MOD_CHECK_ID(sigma_delta, channel);

  if (!platform_sigma_delta_set_duty( channel, duty ))
    luaL_error( L, "command failed" );

  return 0;
}


// Module function map
LROT_BEGIN(sigma_delta, NULL, 0)
  LROT_FUNCENTRY( setup,       sigma_delta_setup )
  LROT_FUNCENTRY( close,       sigma_delta_close )
  //LROT_FUNCENTRY( setpwmduty,  sigma_delta_setpwmduty )
  LROT_FUNCENTRY( setprescale, sigma_delta_setprescale )
  LROT_FUNCENTRY( setduty,     sigma_delta_setduty )
LROT_END(sigma_delta, NULL, 0)

NODEMCU_MODULE(SIGMA_DELTA, "sigma_delta", sigma_delta, NULL);
