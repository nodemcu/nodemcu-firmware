// Module for interfacing with sigma-delta hardware

#include "module.h"
#include "lauxlib.h"
#include "platform.h"


// Lua: setup( pin )
static int sigma_delta_setup( lua_State *L )
{
    int pin = luaL_checkinteger( L, 1 );

    MOD_CHECK_ID(sigma_delta, pin);

    platform_sigma_delta_setup( pin );

    return 0;
}

// Lua: close( pin )
static int sigma_delta_close( lua_State *L )
{
    int pin = luaL_checkinteger( L, 1 );

    MOD_CHECK_ID(sigma_delta, pin);

    platform_sigma_delta_close( pin );

    return 0;
}


// Lua: setpwmduty( duty_cycle )
static int sigma_delta_setpwmduty( lua_State *L )
{
    int duty = luaL_checkinteger( L, 1 );

    if (duty < 0 || duty > 255) {
        return luaL_error( L, "wrong arg range" );
    }

    platform_sigma_delta_set_pwmduty( duty );

    return 0;
}

// Lua: setprescale( value )
static int sigma_delta_setprescale( lua_State *L )
{
    int prescale = luaL_checkinteger( L, 1 );

    if (prescale < 0 || prescale > 255) {
        return luaL_error( L, "wrong arg range" );
    }

    platform_sigma_delta_set_prescale( prescale );

    return 0;
}

// Lua: settarget( value )
static int sigma_delta_settarget( lua_State *L )
{
    int target = luaL_checkinteger( L, 1 );

    if (target < 0 || target > 255) {
        return luaL_error( L, "wrong arg range" );
    }

    platform_sigma_delta_set_target( target );

    return 0;
}


// Module function map
static const LUA_REG_TYPE sigma_delta_map[] =
{
    { LSTRKEY( "setup" ),       LFUNCVAL( sigma_delta_setup ) },
    { LSTRKEY( "close" ),       LFUNCVAL( sigma_delta_close ) },
    { LSTRKEY( "setpwmduty" ),  LFUNCVAL( sigma_delta_setpwmduty ) },
    { LSTRKEY( "setprescale" ), LFUNCVAL( sigma_delta_setprescale ) },
    { LSTRKEY( "settarget" ),   LFUNCVAL( sigma_delta_settarget ) },
    { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SIGMA_DELTA, "sigma_delta", sigma_delta_map, NULL);
