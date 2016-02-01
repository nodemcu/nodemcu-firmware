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

extern lua_State* gL;

extern void lua_table_add_string(lua_State* L, char* name, char* string, ...);
extern void lua_table_add_int(lua_State* L, char* name, lua_Integer integer);
int vsprintf (char *d, const char *s, va_list ap);


#ifdef LUA_USE_MODULES_WIFI_EVENTMON
  #ifdef USE_WIFI_SDK_EVENT_MONITOR
	extern const LUA_REG_TYPE wifi_eventmon_map[];
  #endif
  #ifdef USE_WIFI_STATION_STATUS_MONITOR
//	extern void ets_timer_arm_new(os_timer_t* t, uint32_t milliseconds, uint32_t repeat_flag, uint32_t isMstimer);
//	extern void ets_timer_disarm(os_timer_t* t);
//	extern void ets_timer_setfn(os_timer_t* t, os_timer_func_t *f, void *arg);
	extern int wifi_station_event_mon_start(lua_State* L);
	extern int wifi_station_event_mon_reg(lua_State* L);
	extern void wifi_station_event_mon_stop(lua_State* L);
  #endif
#endif

#endif /* APP_MODULES_WIFI_COMMON_H_ */
