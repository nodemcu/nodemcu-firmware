// Cron module

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "user_interface.h"
#include "c_types.h"
#include "c_string.h"
#include "ets_sys.h"
#include "time.h"
#include "rtc/rtctime.h"
#include "stdlib.h"
#include "mem.h"

struct cronent_desc {
  uint64_t min;  // Minutes repeat - bits 0-59
  uint32_t hour; // Hours repeat - bits 0-23
  uint32_t dom;  // Day-of-month repeat - bits 0-30
  uint16_t mon;  // Monthly repeat - bits 0-11
  uint8_t  dow;  // Day-of-week repeat - bits 0-6
};

typedef struct cronent_ud {
  struct cronent_desc desc;
  int cb_ref;
} cronent_ud_t;

static ETSTimer cron_timer;

static int *cronent_list = 0;
static size_t cronent_count = 0;

static uint64_t lcron_parsepart(lua_State *L, char *str, char **end, uint8_t min, uint8_t max) {
  uint64_t res = 0;
  if (str[0] == '*') {
    uint32_t each = 1;
    *end = str + 1;
    if (str[1] == '/') {
      each = strtol(str + 2, end, 10);
      if (end != 0)
      if (each == 0 || each >= max - min) {
        return luaL_error(L, "invalid spec (each %d)", each);
      }
    }
    for (int i = 0; i <= (max - min); i++) {
      if (((min + i) % each) == 0) res |= (uint64_t)1 << i;
    }
  } else {
    uint32_t val;
    while (1) {
      val = strtol(str, end, 10);
      if (val < min || val > max) {
        return luaL_error(L, "invalid spec (val %d out of range %d..%d)", val, min, max);
      }
      res |= (uint64_t)1 << (val - min);
      if (**end != ',') break;
      str = *end + 1;
    }
  }
  return res;
}

static int lcron_parsedesc(lua_State *L, char *str, struct cronent_desc *desc) {
  char *s = str;
  desc->min = lcron_parsepart(L, s, &s, 0, 59);
  if (*s != ' ') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->hour = lcron_parsepart(L, s + 1, &s, 0, 23);
  if (*s != ' ') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->dom = lcron_parsepart(L, s + 1, &s, 1, 31);
  if (*s != ' ') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->mon = lcron_parsepart(L, s + 1, &s, 1, 12);
  if (*s != ' ') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->dow = lcron_parsepart(L, s + 1, &s, 0, 6);
  if (*s != 0) return luaL_error(L, "invalid spec (trailing @%d)", s - str);
  return 0;
}

static int lcron_create(lua_State *L) {
  // Check arguments
  char *strdesc = (char*)luaL_checkstring(L, 1);
  luaL_checkanyfunction(L, 2);
  // Parse description
  struct cronent_desc desc;
  lcron_parsedesc(L, strdesc, &desc);
  // Allocate userdata
  cronent_ud_t *ud = lua_newuserdata(L, sizeof(cronent_ud_t));
  // Set metatable
  luaL_getmetatable(L, "cron.entry");
  lua_setmetatable(L, -2);
  // Set callback
  lua_pushvalue(L, 2);
  ud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Set entry
  ud->desc = desc;
  // Store entry
  lua_pushvalue(L, -1);
  cronent_list = os_realloc(cronent_list, sizeof(int) * (cronent_count + 1));
  cronent_list[cronent_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
  return 1;
}

static size_t lcron_findindex(lua_State *L, cronent_ud_t *ud) {
  cronent_ud_t *eud;
  size_t i;
  for (i = 0; i < cronent_count; i++) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_list[i]);
    eud = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (eud == ud) break;
  }
  if (i == cronent_count) return -1;
  return i;
}

static int lcron_schedule(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  char *strdesc = (char*)luaL_checkstring(L, 2);
  struct cronent_desc desc;
  lcron_parsedesc(L, strdesc, &desc);
  ud->desc = desc;
  size_t i = lcron_findindex(L, ud);
  if (i == -1) {
    lua_pushvalue(L, 1);
    cronent_list = os_realloc(cronent_list, sizeof(int) * (cronent_count + 1));
    cronent_list[cronent_count++] = lua_ref(L, LUA_REGISTRYINDEX);
  }
  return 0;
}

static int lcron_handler(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  luaL_checkanyfunction(L, 2);
  lua_pushvalue(L, 2);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_ref);
  ud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

static int lcron_unschedule(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  size_t i = lcron_findindex(L, ud);
  if (i == -1) return 0;
  luaL_unref(L, LUA_REGISTRYINDEX, cronent_list[i]);
  memmove(cronent_list + i, cronent_list + i + 1, sizeof(int) * (cronent_count - i - 1));
  cronent_count--;
  return 0;
}

static int lcron_delete(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  lcron_unschedule(L);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_ref);
  return 0;
}

static int lcron_reset(lua_State *L) {
  for (size_t i = 0; i < cronent_count; i++) {
    luaL_unref(L, LUA_REGISTRYINDEX, cronent_list[i]);
  }
  cronent_count = 0;
  os_free(cronent_list);
  cronent_list = 0;
  return 0;
}

static void cron_handle_time(uint8_t mon, uint8_t dom, uint8_t dow, uint8_t hour, uint8_t min) {
  lua_State *L = lua_getstate();
  struct cronent_desc desc;
  desc.mon  = (uint16_t)1 << (mon - 1);
  desc.dom  = (uint32_t)1 << (dom - 1);
  desc.dow  = ( uint8_t)1 << dow;
  desc.hour = (uint32_t)1 << hour;
  desc.min  = (uint64_t)1 << min;
  for (size_t i = 0; i < cronent_count; i++) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_list[i]);
    cronent_ud_t *ent = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if ((ent->desc.mon  & desc.mon ) == 0) continue;
    if ((ent->desc.dom  & desc.dom ) == 0) continue;
    if ((ent->desc.dow  & desc.dow ) == 0) continue;
    if ((ent->desc.hour & desc.hour) == 0) continue;
    if ((ent->desc.min  & desc.min ) == 0) continue;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ent->cb_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_list[i]);
    lua_call(L, 1, 0);
  }
}

static void cron_handle_tmr() {
  struct rtc_timeval tv;
  rtctime_gettimeofday(&tv);
  if (tv.tv_sec == 0) { // Wait for RTC time
    os_timer_arm(&cron_timer, 1000, 0);
    return;
  }
  time_t t = tv.tv_sec;
  struct tm tm;
  gmtime_r(&t, &tm);
  uint32_t diff = 60000 - tm.tm_sec * 1000 - tv.tv_usec / 1000;
  if (tm.tm_sec == 59) {
    t++;
    diff += 60000;
    gmtime_r(&t, &tm);
  }
  os_timer_arm(&cron_timer, diff, 0);
  cron_handle_time(tm.tm_mon + 1, tm.tm_mday, tm.tm_wday, tm.tm_hour, tm.tm_min);
}

static const LUA_REG_TYPE cronent_map[] = {
  { LSTRKEY( "schedule" ),   LFUNCVAL( lcron_schedule ) },
  { LSTRKEY( "handler" ),    LFUNCVAL( lcron_handler ) },
  { LSTRKEY( "unschedule" ), LFUNCVAL( lcron_unschedule ) },
  { LSTRKEY( "__gc" ),       LFUNCVAL( lcron_delete ) },
  { LSTRKEY( "__index" ),    LROVAL( cronent_map ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE cron_map[] = {
  { LSTRKEY( "schedule" ),   LFUNCVAL( lcron_create ) },
  { LSTRKEY( "reset" ),      LFUNCVAL( lcron_reset ) },
  { LNILKEY, LNILVAL }
};
#include "pm/swtimer.h"

int luaopen_cron( lua_State *L ) {
  os_timer_disarm(&cron_timer);
  os_timer_setfn(&cron_timer, cron_handle_tmr, 0);
  SWTIMER_REG_CB(cron_handle_tmr, SWTIMER_RESTART);
    //cron_handle_tmr determines when to execute a scheduled cron job
    //My guess: To be sure to give the other modules required by cron enough time to get to a ready state, restart cron_timer.
  os_timer_arm(&cron_timer, 1000, 0);
  luaL_rometatable(L, "cron.entry", (void *)cronent_map);
  return 0;
}

NODEMCU_MODULE(CRON, "cron", cron_map, luaopen_cron);
