#include "module.h"
#include "lauxlib.h"
#include <stdint.h>

#include "platform.h"


static int htu21_read(lua_State *L)
{
  uint16_t rawT = platform_htu21_read(PLATFORM_HTU21_T_MEASUREMENT_HM);
  uint16_t rawRH = platform_htu21_read(PLATFORM_HTU21_RH_MEASUREMENT_HM);

  if (rawT == PLATFORM_HTU21_CRC_ERROR || rawRH == PLATFORM_HTU21_CRC_ERROR) {
    luaL_error(L, "htu21 invalid CRC");
  }

  lua_pushinteger(L, platform_htu21_temp_ticks_to_millicelsius(rawT));
  lua_pushinteger(L, platform_htu21_rh_ticks_to_per_cent_mille(rawRH));

  return 2;
}


static const LUA_REG_TYPE htu21_map[] = {
    {LSTRKEY("read"), LFUNCVAL(htu21_read)},
    {LNILKEY,         LNILVAL}
};

NODEMCU_MODULE(HTU21, "htu21", htu21_map, NULL);