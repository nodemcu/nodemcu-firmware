#ifndef APP_MODULES_WIFI_COMMON_H_
#define APP_MODULES_WIFI_COMMON_H_

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "c_string.h"
#include "c_stdlib.h"
#include "c_types.h"
#include "user_interface.h"
#include "user_config.h"
#include "c_stdio.h"
#include "task/task.h"

//#define WIFI_DEBUG
//#define EVENT_DEBUG

void wifi_add_sprintf_field(lua_State* L, char* name, char* string, ...);
void wifi_add_int_field(lua_State* L, char* name, lua_Integer integer);

static inline void register_lua_cb(lua_State* L,int* cb_ref)
{
  int ref=luaL_ref(L, LUA_REGISTRYINDEX);
  if( *cb_ref != LUA_NOREF)
  {
	luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  }
  *cb_ref = ref;
}

static inline void unregister_lua_cb(lua_State* L, int* cb_ref)
{
  if(*cb_ref != LUA_NOREF)
  {
	luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  	*cb_ref = LUA_NOREF;
  }
}

void wifi_change_default_host_name(void);

#if defined(WIFI_DEBUG) || defined(NODE_DEBUG)
#define WIFI_DBG(...) c_printf(__VA_ARGS__)
#else
#define WIFI_DBG(...) //c_printf(__VA_ARGS__)
#endif

#if defined(EVENT_DEBUG) || defined(NODE_DEBUG)
#define EVENT_DBG(...) c_printf(__VA_ARGS__)
#else
#define EVENT_DBG(...) //c_printf(__VA_ARGS__)
#endif

#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE
  extern const LUA_REG_TYPE wifi_event_monitor_map[];
  void wifi_eventmon_init();
#endif
#ifdef WIFI_STATION_STATUS_MONITOR_ENABLE
  int wifi_station_event_mon_start(lua_State* L);
  int wifi_station_event_mon_reg(lua_State* L);
  void wifi_station_event_mon_stop(lua_State* L);
#endif

#endif /* APP_MODULES_WIFI_COMMON_H_ */
