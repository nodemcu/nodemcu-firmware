#include "module.h"
#include "lauxlib.h"
#include <stdint.h>

#include "platform.h"


static int bh1750_setup(lua_State *L)
{
  platform_bh1750_mode_t mode = (platform_bh1750_mode_t) luaL_optinteger( L, 1, PLATFORM_BH1750_CONTINUOUS_AUTO );
  uint8_t sensitivity = (uint8_t) luaL_optinteger( L, 2, PLATFORM_BH1750_DEFAULT_SENSITIVITY );

  luaL_argcheck( L,
      mode == PLATFORM_BH1750_CONTINUOUS_AUTO ||
      mode == PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE ||
      mode == PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 ||
      mode == PLATFORM_BH1750_CONTINUOUS_LOW_RES_MODE ||
      mode == PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE ||
      mode == PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 ||
      mode == PLATFORM_BH1750_ONE_TIME_LOW_RES_MODE, 1, "wrong mode" );

  luaL_argcheck( L, sensitivity >= PLATFORM_BH1750_MIN_SENSITIVITY && sensitivity <= PLATFORM_BH1750_MAX_SENSITIVITY, 2,
                 "invalid sensitivity" );

  platform_bh1750_setup(mode, sensitivity);

  return 0;
}

static int bh1750_power_down(lua_State *L)
{
  platform_bh1750_power_down();
  return 0;
}

static int bh1750_read(lua_State *L)
{
  lua_pushinteger(L, platform_bh1750_read());
  return 1;
}


static const LUA_REG_TYPE bh1750_map[] = {
    { LSTRKEY( "setup" ),                      LFUNCVAL( bh1750_setup ) },
    { LSTRKEY( "power_down" ),                 LFUNCVAL( bh1750_power_down ) },
    { LSTRKEY( "read" ),                       LFUNCVAL( bh1750_read ) },
    { LSTRKEY( "CONTINUOUS_AUTO" ),            LNUMVAL( PLATFORM_BH1750_CONTINUOUS_AUTO ) },
    { LSTRKEY( "CONTINUOUS_HIGH_RES_MODE" ),   LNUMVAL( PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE ) },
    { LSTRKEY( "CONTINUOUS_HIGH_RES_MODE_2" ), LNUMVAL( PLATFORM_BH1750_CONTINUOUS_HIGH_RES_MODE_2 ) },
    { LSTRKEY( "CONTINUOUS_LOW_RES_MODE" ),    LNUMVAL( PLATFORM_BH1750_CONTINUOUS_LOW_RES_MODE ) },
    { LSTRKEY( "ONE_TIME_HIGH_RES_MODE" ),     LNUMVAL( PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE ) },
    { LSTRKEY( "ONE_TIME_HIGH_RES_MODE_2" ),   LNUMVAL( PLATFORM_BH1750_ONE_TIME_HIGH_RES_MODE_2 ) },
    { LSTRKEY( "ONE_TIME_LOW_RES_MODE" ),      LNUMVAL( PLATFORM_BH1750_ONE_TIME_LOW_RES_MODE ) },
    {LNILKEY,         LNILVAL}
};

NODEMCU_MODULE(BH1750, "bh1750", bh1750_map, NULL);