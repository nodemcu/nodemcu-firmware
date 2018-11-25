#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "lwip/apps/sntp.h"


#define ADD_TABLE_ITEM(L, key, val) \
  lua_pushinteger (L, val);      \
  lua_setfield (L, -2, key);
  
void inline time_tmToTable(lua_State *L, struct tm *date)
{ 
  lua_createtable (L, 0, 9);
  
  ADD_TABLE_ITEM (L, "yday", date->tm_yday + 1);
  ADD_TABLE_ITEM (L, "wday", date->tm_wday + 1);
  ADD_TABLE_ITEM (L, "year", date->tm_year + 1900);
  ADD_TABLE_ITEM (L, "mon",  date->tm_mon + 1);
  ADD_TABLE_ITEM (L, "day",  date->tm_mday);
  ADD_TABLE_ITEM (L, "hour", date->tm_hour);
  ADD_TABLE_ITEM (L, "min",  date->tm_min);
  ADD_TABLE_ITEM (L, "sec",  date->tm_sec);
  ADD_TABLE_ITEM (L, "dst",  date->tm_isdst);
}

static int time_get(lua_State *L)
{
  struct timeval  tv;
  gettimeofday (&tv, NULL);
  lua_pushnumber (L, tv.tv_sec);
  lua_pushnumber (L, tv.tv_usec);
  return 2;
}

static int time_set(lua_State *L)
{
  uint32_t sec = luaL_checknumber (L, 1);
  uint32_t usec = 0;
  if (lua_isnumber (L, 2))
    usec = lua_tonumber (L, 2);

  struct timeval tv = { sec, usec };
  settimeofday (&tv, NULL);

  return 0;
}

static int time_getLocal(lua_State *L)
{
  time_t now;
  struct tm date;
  
  time(&now);  
  localtime_r(&now, &date);
  
    /* construct Lua table */
  time_tmToTable(L, &date);

  return 1;
}

static int time_setTimezone(lua_State *L)
{
  const char *timezone = luaL_checkstring(L, 1);
  
  setenv("TZ", timezone, 1);
  tzset();
  
  return 0;
}

static int time_initNTP(lua_State *L)
{
  size_t l;
  const char *server = luaL_optlstring (L, 1, "pool.ntp.org", &l);
  
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, server);
  sntp_init();
  
  return 0;
}

static int time_ntpEnabled(lua_State *L)
{
  lua_pushboolean(L, sntp_enabled());
  
  return 1;
}

static int time_ntpStop(lua_State *L)
{
  sntp_stop();
  
  return 0;
}

static int time_epoch2cal(lua_State *L)
{
  struct tm *date;
  
  time_t now = luaL_checkint (L, 1);
  luaL_argcheck (L, now >= 0, 1, "wrong arg range");

  date = gmtime (&now);

  /* construct Lua table */
  time_tmToTable(L, date);

  return 1;
}

static int time_cal2epoc(lua_State *L)
{
  struct tm date;
  luaL_checkanytable (L, 1);
  
  lua_getfield (L, 1, "year");
  date.tm_year = luaL_optinteger(L, -1, 1900) - 1900;
  
  lua_getfield (L, 1, "mon");
  date.tm_mon = luaL_optinteger(L, -1, 1) - 1;
  
  lua_getfield (L, 1, "day");
  date.tm_mday = luaL_optinteger(L, -1, 0);
  
  lua_getfield (L, 1, "hour");
  date.tm_hour = luaL_optinteger(L, -1, 0);
  
  lua_getfield (L, 1, "min");
  date.tm_min = luaL_optinteger(L, -1, 0);
  
  lua_getfield (L, 1, "sec");
  date.tm_sec = luaL_optinteger(L, -1, 0);
  
  lua_pushnumber (L, mktime(&date));
  
  return 1;
}

static const LUA_REG_TYPE time_map[] = {
  { LSTRKEY("set"),              LFUNCVAL(time_set) },
  { LSTRKEY("get"),              LFUNCVAL(time_get) },
  { LSTRKEY("getlocal"),         LFUNCVAL(time_getLocal) },
  { LSTRKEY("settimezone"),      LFUNCVAL(time_setTimezone) },
  { LSTRKEY("initntp"),          LFUNCVAL(time_initNTP)  },
  { LSTRKEY("ntpenabled"),       LFUNCVAL(time_ntpEnabled) },
  { LSTRKEY("ntpstop"),          LFUNCVAL(time_ntpStop) },
  { LSTRKEY("epoch2cal"),        LFUNCVAL(time_epoch2cal) },
  { LSTRKEY("cal2epoch"),        LFUNCVAL(time_cal2epoc) },
  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(TIME, "time", time_map, NULL);
