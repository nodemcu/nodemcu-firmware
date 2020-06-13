// Cron module

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "user_interface.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ets_sys.h"
#include "time.h"
#include "rtc/rtctime.h"
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

static int cronent_table_ref;

static uint64_t lcron_parsepart(lua_State *L, char *str, char **end, uint8_t min, uint8_t max) {
  uint64_t res = 0;

  /* Gobble whitespace before potential stars; no strtol on that path */
  while (*str != '\0' && (*str == ' ' || *str == '\t')) {
    str++;
  }

  if (str[0] == '*') {
    uint32_t each = 1;
    *end = str + 1;
    if (str[1] == '/') {
      each = strtol(str + 2, end, 10);
      if (each == 0 || each >= max - min) {
        return luaL_error(L, "invalid spec (each %d)", each);
      }
    }
    for (int i = 0; i <= (max - min); i++) {
      if ((i % each) == 0) res |= (uint64_t)1 << i;
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
  if (*s != ' ' && *s != '\t') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->hour = lcron_parsepart(L, s + 1, &s, 0, 23);
  if (*s != ' ' && *s != '\t') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->dom = lcron_parsepart(L, s + 1, &s, 1, 31);
  if (*s != ' ' && *s != '\t') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->mon = lcron_parsepart(L, s + 1, &s, 1, 12);
  if (*s != ' ' && *s != '\t') return luaL_error(L, "invalid spec (separator @%d)", s - str);
  desc->dow = lcron_parsepart(L, s + 1, &s, 0, 6);
  while (*s != '\0' && (*s == ' ' || *s == '\t')) {
    s++;
  }
  if (*s != 0) return luaL_error(L, "invalid spec (trailing @%d)", s - str);
  return 0;
}

static int lcron_create(lua_State *L) {
  // Check arguments
  char *strdesc = (char*)luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);

  // Grab the table of all entries onto the stack
  lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_table_ref);

  // Find a free index
  int ix = 1;
  while(lua_rawgeti(L, -1, ix) != LUA_TNIL) {
    lua_pop(L, 1);
    ix++;
  }
  lua_pop(L, 1); // pop the nil off the stack

  // Allocate userdata onto the stack
  cronent_ud_t *ud = lua_newuserdata(L, sizeof(cronent_ud_t));
  // Set metatable
  luaL_getmetatable(L, "cron.entry");
  lua_setmetatable(L, -2);
  // Set callback
  lua_pushvalue(L, 2);
  ud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  // Set entry
  lcron_parsedesc(L, strdesc, &ud->desc);

  // Store entry to table while preserving userdata
  lua_pushvalue(L, -1); // clone userdata
  lua_rawseti(L, -3, ix); // -userdata; userdata, cronent table, cb, desc

  return 1; // just the userdata
}

// Finds index, leaves table at top of stack for convenience
static size_t lcron_findindex(lua_State *L, cronent_ud_t *ud) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_table_ref);
  size_t count = lua_objlen(L, -1);

  for (size_t i = 1; i <= count; i++) {
    lua_rawgeti(L, -1, i);
    cronent_ud_t *eud = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (eud == ud) {
      return i;
    }
  }

  return count + 1;
}

static int lcron_schedule(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  char *strdesc = (char*)luaL_checkstring(L, 2);
  struct cronent_desc desc;
  lcron_parsedesc(L, strdesc, &desc);
  ud->desc = desc;

  size_t i = lcron_findindex(L, ud);

  lua_pushvalue(L, 1); // copy ud to top of stack
  lua_rawseti(L, -2, i); // install into table

  return 0;
}

static int lcron_handler(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_pushvalue(L, 2);
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_ref);
  ud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

static int lcron_unschedule(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  size_t i = lcron_findindex(L, ud);

  lua_pushnil(L);
  lua_rawseti(L, -2, i);

  return 0;
}

// scheduled entries are pinned, so we cannot arrive at the __gc metamethod
static int lcron_delete(lua_State *L) {
  cronent_ud_t *ud = luaL_checkudata(L, 1, "cron.entry");
  luaL_unref(L, LUA_REGISTRYINDEX, ud->cb_ref);
  return 0;
}

static int lcron_reset(lua_State *L) {
  lua_newtable(L);
  luaL_unref(L, LUA_REGISTRYINDEX, cronent_table_ref);
  cronent_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

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

  lua_rawgeti(L, LUA_REGISTRYINDEX, cronent_table_ref);
  size_t count = lua_objlen(L, -1);

  for (size_t i = 1; i <= count; i++) {
    lua_rawgeti(L, -1, i);
    cronent_ud_t *ent = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if ((ent->desc.mon  & desc.mon ) == 0) continue;
    if ((ent->desc.dom  & desc.dom ) == 0) continue;
    if ((ent->desc.dow  & desc.dow ) == 0) continue;
    if ((ent->desc.hour & desc.hour) == 0) continue;
    if ((ent->desc.min  & desc.min ) == 0) continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ent->cb_ref);
    lua_rawgeti(L, -2, i); // get ud again
    luaL_pcallx(L, 1, 0);
  }

  lua_pop(L, 1); // pop table
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



LROT_BEGIN(cronent, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc, lcron_delete )
  LROT_TABENTRY(  __index, cronent )
  LROT_FUNCENTRY( schedule, lcron_schedule )
  LROT_FUNCENTRY( handler, lcron_handler )
  LROT_FUNCENTRY( unschedule, lcron_unschedule )
LROT_END(cronent, NULL, LROT_MASK_GC_INDEX)


LROT_BEGIN(cron, NULL, 0)
  LROT_FUNCENTRY( schedule, lcron_create )
  LROT_FUNCENTRY( reset, lcron_reset )
LROT_END(cron, NULL, 0)

#include "pm/swtimer.h"

int luaopen_cron( lua_State *L ) {
  os_timer_disarm(&cron_timer);
  os_timer_setfn(&cron_timer, cron_handle_tmr, 0);
  SWTIMER_REG_CB(cron_handle_tmr, SWTIMER_RESTART);
    //cron_handle_tmr determines when to execute a scheduled cron job
    //My guess: To be sure to give the other modules required by cron enough time to get to a ready state, restart cron_timer.
  os_timer_arm(&cron_timer, 1000, 0);
  luaL_rometatable(L, "cron.entry", LROT_TABLEREF(cronent));

  cronent_table_ref = LUA_NOREF;
  lcron_reset(L);

  return 0;
}

NODEMCU_MODULE(CRON, "cron", cron, luaopen_cron);
