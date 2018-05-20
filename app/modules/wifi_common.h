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

static inline void register_lua_cb(lua_State* L,int* cb_ref){
  int ref=luaL_ref(L, LUA_REGISTRYINDEX);
  if( *cb_ref != LUA_NOREF){
    luaL_unref(L, LUA_REGISTRYINDEX, *cb_ref);
  }
  *cb_ref = ref;
}

static inline void unregister_lua_cb(lua_State* L, int* cb_ref){
  if(*cb_ref != LUA_NOREF){
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
#define EVENT_DBG(fmt, ...) c_printf("\n EVENT_DBG(%s): "fmt"\n", __FUNCTION__, ##__VA_ARGS__)

#else
#define EVENT_DBG(...) //c_printf(__VA_ARGS__)
#endif

enum wifi_suspension_state{
  WIFI_AWAKE = 0,
  WIFI_SUSPENSION_PENDING = 1,
  WIFI_SUSPENDED = 2
};



#ifdef WIFI_SDK_EVENT_MONITOR_ENABLE
  extern const LUA_REG_TYPE wifi_event_monitor_map[];
  void wifi_eventmon_init();
  int wifi_event_monitor_register(lua_State* L);
#endif

#ifdef LUA_USE_MODULES_WIFI_MONITOR
  extern const LUA_REG_TYPE wifi_monitor_map[];
  int wifi_monitor_init(lua_State *L);
#endif

#endif /* APP_MODULES_WIFI_COMMON_H_ */
