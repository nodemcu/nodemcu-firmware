// Module for RTC user memory access

#include "module.h"
#include "lauxlib.h"
#include "rtc/rtcaccess.h"

static int rtcmem_read32 (lua_State *L)
{
  int idx = luaL_checkinteger (L, 1);
  int n = 1;
  if (lua_isnumber (L, 2))
    n = lua_tointeger (L, 2);

  if (!lua_checkstack (L, n))
    return 0;

  int ret  = 0;
  while (n > 0 && idx >= 0 && idx < RTC_USER_MEM_NUM_DWORDS)
  {
    lua_pushinteger (L, rtc_mem_read (idx++));
    --n;
    ++ret;
  }
  return ret;
}


static int rtcmem_write32 (lua_State *L)
{
  int idx = luaL_checkinteger (L, 1);
  int n = lua_gettop (L) - 1;
  luaL_argcheck (
    L, idx + n <= RTC_USER_MEM_NUM_DWORDS, 1, "RTC mem would overrun");
  int src = 2;
  while (n-- > 0)
  {
    rtc_mem_write (idx++, lua_tointeger (L, src++));
  }
  return 0;
}


// Module function map
LROT_BEGIN(rtcmem, NULL, 0)
  LROT_FUNCENTRY( read32, rtcmem_read32 )
  LROT_FUNCENTRY( write32, rtcmem_write32 )
LROT_END(rtcmem, NULL, 0)


NODEMCU_MODULE(RTCMEM, "rtcmem", rtcmem, NULL);
