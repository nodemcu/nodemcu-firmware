// Module for RTC sample FIFO storage

#include "module.h"
#include "lauxlib.h"
#include "user_modules.h"
#include "rtc/rtctime.h"
#define RTCTIME_SLEEP_ALIGNED rtctime_deep_sleep_until_aligned_us
#include "rtc/rtcfifo.h"

// rtcfifo.prepare ([{sensor_count=n, interval_us=m, storage_begin=x, storage_end=y}])
static int rtcfifo_prepare (lua_State *L)
{
  uint32_t sensor_count = RTC_DEFAULT_TAGCOUNT;
  uint32_t interval_us = 0;
  int first = -1, last = -1;

  if (lua_istable (L, 1))
  {
#ifdef LUA_USE_MODULES_RTCTIME
    lua_getfield (L, 1, "interval_us");
    if (lua_isnumber (L, -1))
      interval_us = lua_tonumber (L, -1);
    lua_pop (L, 1);
#endif

    lua_getfield (L, 1, "sensor_count");
    if (lua_isnumber (L, -1))
      sensor_count = lua_tonumber (L, -1);
    lua_pop (L, 1);

    lua_getfield (L, 1, "storage_begin");
    if (lua_isnumber (L, -1))
      first = lua_tonumber (L, -1);
    lua_pop (L, 1);
    lua_getfield (L, 1, "storage_end");
    if (lua_isnumber (L, -1))
      last = lua_tonumber (L, -1);
    lua_pop (L, 1);
  }
  else if (!lua_isnone (L, 1))
    return luaL_error (L, "expected table as arg #1");

  rtc_fifo_prepare (0, interval_us, sensor_count);

  if (first != -1 && last != -1)
    rtc_fifo_put_loc (first, last, sensor_count);

  return 0;
}


// ready = rtcfifo.ready ()
static int rtcfifo_ready (lua_State *L)
{
  lua_pushnumber (L, rtc_fifo_check_magic ());
  return 1;
}

static void check_fifo_magic (lua_State *L)
{
  if (!rtc_fifo_check_magic ())
    luaL_error (L, "rtcfifo not prepared!");
}


// rtcfifo.put (timestamp, value, decimals, sensor_name)
static int rtcfifo_put (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  s.timestamp = luaL_checknumber (L, 1);
  s.value = luaL_checknumber (L, 2);
  s.decimals = luaL_checknumber (L, 3);
  size_t len;
  const char *str = luaL_checklstring (L, 4, &len);
  union {
    uint32_t u;
    char s[4];
  } conv = { 0 };
  strncpy (conv.s, str, len > 4 ? 4 : len);
  s.tag = conv.u;

  rtc_fifo_store_sample (&s);
  return 0;
}


static int extract_sample (lua_State *L, const sample_t *s)
{
  lua_pushnumber (L, s->timestamp);
  lua_pushnumber (L, s->value);
  lua_pushnumber (L, s->decimals);
  union {
    uint32_t u;
    char s[4];
  } conv = { s->tag };
  if (conv.s[3] == 0)
    lua_pushstring (L, conv.s);
  else
    lua_pushlstring (L, conv.s, 4);
  return 4;
}


// timestamp, value, decimals, sensor_name = rtcfifo.pop ()
static int rtcfifo_pop (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  if (!rtc_fifo_pop_sample (&s))
    return 0;
  else
    return extract_sample (L, &s);
}


// timestamp, value, decimals, sensor_name = rtcfifo.peek ([offset])
static int rtcfifo_peek (lua_State *L)
{
  check_fifo_magic (L);

  sample_t s;
  uint32_t offs = 0;
  if (lua_isnumber (L, 1))
    offs = lua_tonumber (L, 1);
  if (!rtc_fifo_peek_sample (&s, offs))
    return 0;
  else
    return extract_sample (L, &s);
}


// rtcfifo.drop (num)
static int rtcfifo_drop (lua_State *L)
{
  check_fifo_magic (L);

  rtc_fifo_drop_samples (luaL_checknumber (L, 1));
  return 0;
}


// num = rtcfifo.count ()
static int rtcfifo_count (lua_State *L)
{
  check_fifo_magic (L);

  lua_pushnumber (L, rtc_fifo_get_count ());
  return 1;
}


#ifdef LUA_USE_MODULES_RTCTIME
// rtcfifo.dsleep_until_sample (min_sleep_us)
static int rtcfifo_dsleep_until_sample (lua_State *L)
{
  check_fifo_magic (L);

  uint32_t min_us = luaL_checknumber (L, 1);
  rtc_fifo_deep_sleep_until_sample (min_us); // no return
  return 0;
}
#endif

// Module function map
static const LUA_REG_TYPE rtcfifo_map[] = {
  { LSTRKEY("prepare"),             LFUNCVAL(rtcfifo_prepare) },
  { LSTRKEY("ready"),               LFUNCVAL(rtcfifo_ready) },
  { LSTRKEY("put"),                 LFUNCVAL(rtcfifo_put) },
  { LSTRKEY("pop"),                 LFUNCVAL(rtcfifo_pop) },
  { LSTRKEY("peek"),                LFUNCVAL(rtcfifo_peek) },
  { LSTRKEY("drop"),                LFUNCVAL(rtcfifo_drop) },
  { LSTRKEY("count"),               LFUNCVAL(rtcfifo_count) },
#ifdef LUA_USE_MODULES_RTCTIME
  { LSTRKEY("dsleep_until_sample"), LFUNCVAL(rtcfifo_dsleep_until_sample) },
#endif
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(RTCFIFO, "rtcfifo", rtcfifo_map, NULL);
