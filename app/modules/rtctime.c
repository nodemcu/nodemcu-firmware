// Module for RTC user memory access

#include "lauxlib.h"
#include "rtc/rtctime.h"

//  rtctime.settimeofday (sec, usec)
static int rtctime_settimeofday (lua_State *L)
{
  if (!rtc_time_check_magic ())
    rtc_time_prepare ();

  uint32_t sec = luaL_checknumber (L, 1);
  uint32_t usec = 0;
  if (lua_isnumber (L, 2))
    usec = lua_tonumber (L, 2);

  struct rtc_timeval tv = { sec, usec };
  rtc_time_settimeofday (&tv);
  return 0;
}


// sec, usec = rtctime.gettimeofday ()
static int rtctime_gettimeofday (lua_State *L)
{
  struct rtc_timeval tv;
  rtc_time_gettimeofday (&tv, CPU_DEFAULT_MHZ);
  lua_pushnumber (L, tv.tv_sec);
  lua_pushnumber (L, tv.tv_usec);
  return 2;
}

static void do_sleep_opt (lua_State *L, int idx)
{
  if (lua_isnumber (L, idx))
  {
    uint32_t opt = lua_tonumber (L, idx);
    if (opt < 0 || opt > 4)
      luaL_error (L, "unknown sleep option");
    deep_sleep_set_option (opt);
  }
}

// rtctime.dsleep (usec, option)
static int rtctime_dsleep (lua_State *L)
{
  uint32_t us = luaL_checknumber (L, 1);
  do_sleep_opt (L, 2);
  // does not return
  rtc_time_deep_sleep_us (us, CPU_DEFAULT_MHZ);
  return 0;
}


// rtctime.dsleep_aligned (aligned_usec, min_usec, option)
static int rtctime_dsleep_aligned (lua_State *L)
{
  if (!rtc_time_have_time ())
    return luaL_error (L, "time not available, unable to align");

  uint32_t align_us = luaL_checknumber (L, 1);
  uint32_t min_us = luaL_checknumber (L, 2);
  do_sleep_opt (L, 3);
  // does not return
  rtc_time_deep_sleep_until_aligned (align_us, min_us, CPU_DEFAULT_MHZ);
  return 0;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE rtctime_map[] =
{
  { LSTRKEY("settimeofday"), LFUNCVAL(rtctime_settimeofday) },
  { LSTRKEY("gettimeofday"), LFUNCVAL(rtctime_gettimeofday) },
  { LSTRKEY("dsleep"),  LFUNCVAL(rtctime_dsleep)  },
  { LSTRKEY("dsleep_aligned"), LFUNCVAL(rtctime_dsleep_aligned) },
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_rtctime (lua_State *L)
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register (L, AUXLIB_RTCTIME, rtctime_map);
  return 1;
#endif
}
