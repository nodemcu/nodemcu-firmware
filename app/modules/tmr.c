/*guys, srsly, turn on warnings in the makefile*/
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/* See docs/modules/tmr.md for documentaiton o current API */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include <stdint.h>
#include "user_interface.h"
#include "pm/swtimer.h"

#define TIMER_MODE_SINGLE 0
#define TIMER_MODE_AUTO   1
#define TIMER_MODE_SEMI   2
#define TIMER_MODE_OFF    3
#define TIMER_IDLE_FLAG (1<<7)

#define STRINGIFY_VAL(x) #x
#define STRINGIFY(x) STRINGIFY_VAL(x)

// assuming system_timer_reinit() has *not* been called
#define MAX_TIMEOUT_DEF 0x68D7A3  // SDK specfied limit

static const uint32 MAX_TIMEOUT=MAX_TIMEOUT_DEF;
static const char* MAX_TIMEOUT_ERR_STR = "Range: 1-"STRINGIFY(MAX_TIMEOUT_DEF);

typedef struct{
	os_timer_t os;
	sint32_t lua_ref;	  /* Reference to registered callback function */
	sint32_t self_ref;	/* Reference to UD registered slot */
	uint32_t interval;
	uint8_t mode;
} tmr_t;

// The previous implementation extended the rtc counter to 64 bits, and then
// applied rtc2sec with the current calibration value to that 64 bit value.
// This means that *ALL* clock ticks since bootup are counted with the
// *current* clock period. In extreme cases (long uptime, sudden temperature
// change), this could result in tmr.time() going backwards....
//
// This implementation instead applies rtc2usec to short time intervals only
// (the longest being around 1 second), and then accumulates the resulting
// microseconds in a 64 bit counter. That's guaranteed to be monotonic, and
// should be a lot closer to representing an actual uptime.

static uint32_t rtc_time_cali=0;
static uint32_t last_rtc_time=0;
static uint64_t last_rtc_time_us=0;

static sint32_t soft_watchdog  = -1;
static os_timer_t rtc_timer;

static void alarm_timer_common(void* arg){
  tmr_t *tmr = (tmr_t *) arg;
  if(tmr->lua_ref > 0) {
    lua_State* L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->lua_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->self_ref);
    if (tmr->mode != TIMER_MODE_AUTO) {
    if(tmr->mode == TIMER_MODE_SINGLE) {
      luaL_unref2(L, LUA_REGISTRYINDEX, tmr->lua_ref);
      luaL_unref2(L, LUA_REGISTRYINDEX, tmr->self_ref);
      tmr->mode = TIMER_MODE_OFF;
    } else if (tmr->mode == TIMER_MODE_SEMI) {
      tmr->mode |= TIMER_IDLE_FLAG;
      luaL_unref2(L, LUA_REGISTRYINDEX, tmr->self_ref);
      }
    }
    luaL_pcallx(L, 1, 0);
  }
}

// Lua: tmr.delay( us )
static int tmr_delay( lua_State* L ){
	sint32_t us = luaL_checkinteger(L, 1);
  luaL_argcheck(L, us>0, 1, "wrong arg range");
	while(us > 0){
	  os_delay_us(us >= 1000000 ? 1000000 : us);
	  system_soft_wdt_feed ();
	  us -= 1000000;
	}
  return 0;
}

// Lua: tmr.now() , return system timer in us
static int tmr_now(lua_State* L){
	lua_pushinteger(L, (uint32_t) (0x7FFFFFFF & system_get_time()));
	return 1;
}

// Lua: tmr.ccount() , returns CCOUNT register
static int tmr_ccount(lua_State* L){
	lua_pushinteger(L, CCOUNT_REG);
	return 1;
}

/*
** Health warning: this is also called DIRECTLY from alarm() which assumes that the Lua
** stack is preserved for the following start(), so the stack MUST be balanced here.
*/

// Lua: t:register( interval, mode, function )
static int tmr_register(lua_State* L) {
	tmr_t *tmr = (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
	uint32_t interval = luaL_checkinteger(L, 2);
	uint8_t mode = luaL_checkinteger(L, 3);

	luaL_argcheck(L, (interval > 0 && interval <= MAX_TIMEOUT), 2, MAX_TIMEOUT_ERR_STR);
	luaL_argcheck(L, (mode == TIMER_MODE_SINGLE || mode == TIMER_MODE_SEMI || mode == TIMER_MODE_AUTO), 3, "Invalid mode");
	luaL_argcheck(L, lua_isfunction(L, 4), 4, "Must be function");

	//get the lua function reference
	lua_pushvalue(L, 4);
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		os_timer_disarm(&tmr->os);
	luaL_reref(L, LUA_REGISTRYINDEX, &tmr->lua_ref);
	tmr->mode = mode|TIMER_IDLE_FLAG;
	tmr->interval = interval;
	os_timer_setfn(&tmr->os, alarm_timer_common, tmr);
	return 0;
}

// Lua: t:start( [restart] )
static int tmr_start(lua_State* L){
	tmr_t *tmr = (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
	lua_settop(L, 2);
	luaL_argcheck(L, lua_isboolean(L, 2) || lua_isnil(L, 2), 2, "boolean expected");
	int restart = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : 0;

	lua_settop(L, 1);  /* ignore any args after the userdata */
	if (tmr->self_ref == LUA_NOREF)
		tmr->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	//we return false if the timer is not idle
	int idle = tmr->mode&TIMER_IDLE_FLAG;
	if(!(idle || restart)){
		lua_pushboolean(L, 0);
	}else{
		if (!idle) {os_timer_disarm(&tmr->os);}
		tmr->mode &= ~TIMER_IDLE_FLAG;
		os_timer_arm(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO);
	}
	lua_pushboolean(L, !idle); /* false if the timer is not idle */
	return 1;
}

// Lua: t:alarm( interval, repeat, function )
static int tmr_alarm(lua_State* L){
	tmr_register(L);
	return tmr_start(L);
}

// Lua: t:stop()
static int tmr_stop(lua_State* L){
	tmr_t *tmr = (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
  int idle = tmr->mode == TIMER_MODE_OFF || (tmr->mode & TIMER_IDLE_FLAG);
  luaL_unref2(L, LUA_REGISTRYINDEX, tmr->self_ref);

	if(!idle)
		os_timer_disarm(&tmr->os);
	tmr->mode |= TIMER_IDLE_FLAG;
	lua_pushboolean(L, !idle);  /* return false if the timer is idle (or not registered) */
	return 1;
}

#ifdef TIMER_SUSPEND_ENABLE

#define TMR_SUSPEND_REMOVED_MSG "This feature has been removed, we apologize for any inconvenience this may have caused."
#define tmr_suspend     tmr_suspend_removed
#define tmr_resume      tmr_suspend_removed
#define tmr_suspend_all tmr_suspend_removed
#define tmr_resume_all  tmr_suspend_removed
static int tmr_suspend_removed(lua_State* L){
  return luaL_error(L, TMR_SUSPEND_REMOVED_MSG);
}
#endif

// Lua: t:unregister()
static int tmr_unregister(lua_State* L){
	tmr_t *tmr =  (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
  luaL_unref2(L, LUA_REGISTRYINDEX, tmr->self_ref);
	luaL_unref2(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		os_timer_disarm(&tmr->os);
	tmr->mode = TIMER_MODE_OFF;
	return 0;
}

// Lua: t:interval( interval )
static int tmr_interval(lua_State* L){
	tmr_t *tmr = (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
	uint32_t interval = luaL_checkinteger(L, 2);
	luaL_argcheck(L, (interval > 0 && interval <= MAX_TIMEOUT), 2, MAX_TIMEOUT_ERR_STR);
	if(tmr->mode != TIMER_MODE_OFF){
		tmr->interval = interval;
		if(!(tmr->mode&TIMER_IDLE_FLAG)){
			os_timer_disarm(&tmr->os);
			os_timer_arm(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO);
		}
	}
	return 0;
}

// Lua: t:state()
static int tmr_state(lua_State* L){
	tmr_t *tmr = (tmr_t *) luaL_checkudata(L, 1, "tmr.timer");
	if(tmr->mode == TIMER_MODE_OFF){
		lua_pushnil(L);
		return 1;
	}

  lua_pushboolean(L, (tmr->mode & TIMER_IDLE_FLAG) == 0);
  lua_pushinteger(L, tmr->mode & (~TIMER_IDLE_FLAG));
	return 2;
}

// Lua: tmr.wdclr()
static int tmr_wdclr( lua_State* L ){
	system_soft_wdt_feed ();
	return 0;
}

// The on ESP8266 system_rtc_clock_cali_proc() returns a fixed point value 
// (12 bit fraction part), giving how many rtc clock ticks represent 1us.
//  The high 64 bits of the uint64_t multiplication are not needed)
static uint32_t rtc2usec(uint64_t rtc){
	return (rtc*rtc_time_cali)>>12;
}

// This returns the number of microseconds uptime. Note that it relies on
// the rtc clock, which is notoriously temperature dependent
inline static uint64_t rtc_timer_update(bool do_calibration){
	if (do_calibration || rtc_time_cali==0)
		rtc_time_cali=system_rtc_clock_cali_proc();

	uint32_t current = system_get_rtc_time();
	uint32_t since_last=current-last_rtc_time; // This will transparently deal with wraparound
	uint32_t us_since_last=rtc2usec(since_last);
	uint64_t now=last_rtc_time_us+us_since_last;

	// Only update if at least 100ms has passed since we last updated.
	// This prevents the rounding errors in rtc2usec from accumulating
	if (us_since_last>=100000){
		last_rtc_time=current;
		last_rtc_time_us=now;
	}
	return now;
}

void rtc_callback(void *arg){
	rtc_timer_update(true);
	if(soft_watchdog > 0){
		soft_watchdog--;
		if(soft_watchdog == 0)
			system_restart();
	}
}

// Lua: tmr.time() , return rtc time in second
static int tmr_time( lua_State* L ){
	uint64_t us=rtc_timer_update(false);
	lua_pushinteger(L, us/1000000);
	return 1;
}

// Lua: tmr.softwd( value )
static int tmr_softwd( lua_State* L ){
	int t = luaL_checkinteger(L, 1);
	luaL_argcheck(L, t>0 , 2, "invalid time");
  soft_watchdog = t;
	return 0;
}

// Lua: tmr.create()
static int tmr_create( lua_State *L ) {
	tmr_t *ud = (tmr_t *)lua_newuserdata(L, sizeof(*ud));
	luaL_getmetatable(L, "tmr.timer");
	lua_setmetatable(L, -2);
	*ud = (tmr_t) {{0}, LUA_NOREF, LUA_NOREF, 0, TIMER_MODE_OFF};
	return 1;
}


// Module function map

LROT_BEGIN(tmr_dyn, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc, tmr_unregister )
  LROT_TABENTRY(  __index, tmr_dyn )
  LROT_FUNCENTRY( register, tmr_register )
  LROT_FUNCENTRY( alarm, tmr_alarm )
  LROT_FUNCENTRY( start, tmr_start )
  LROT_FUNCENTRY( stop, tmr_stop )
  LROT_FUNCENTRY( unregister, tmr_unregister )
  LROT_FUNCENTRY( state, tmr_state )
  LROT_FUNCENTRY( interval, tmr_interval )
#ifdef TIMER_SUSPEND_ENABLE
  LROT_FUNCENTRY( suspend, tmr_suspend )
  LROT_FUNCENTRY( resume, tmr_resume )
#endif
LROT_END(tmr_dyn, NULL, LROT_MASK_GC_INDEX)


LROT_BEGIN(tmr, NULL, 0)
  LROT_FUNCENTRY( delay, tmr_delay )
  LROT_FUNCENTRY( now, tmr_now )
  LROT_FUNCENTRY( wdclr, tmr_wdclr )
  LROT_FUNCENTRY( softwd, tmr_softwd )
  LROT_FUNCENTRY( time, tmr_time )
  LROT_FUNCENTRY( ccount, tmr_ccount )
#ifdef TIMER_SUSPEND_ENABLE
  LROT_FUNCENTRY( suspend_all, tmr_suspend_all )
  LROT_FUNCENTRY( resume_all, tmr_resume_all )
#endif
  LROT_FUNCENTRY( create, tmr_create )
  LROT_NUMENTRY( ALARM_SINGLE, TIMER_MODE_SINGLE )
  LROT_NUMENTRY( ALARM_SEMI, TIMER_MODE_SEMI )
  LROT_NUMENTRY( ALARM_AUTO, TIMER_MODE_AUTO )
LROT_END(tmr, NULL, 0)


#include "pm/swtimer.h"
int luaopen_tmr( lua_State *L ){
	luaL_rometatable(L, "tmr.timer", LROT_TABLEREF(tmr_dyn));

	last_rtc_time=system_get_rtc_time(); // Right now is time 0
	last_rtc_time_us=0;

	os_timer_disarm(&rtc_timer);
	os_timer_setfn(&rtc_timer, rtc_callback, NULL);
	os_timer_arm(&rtc_timer, 1000, 1);

  // The function rtc_callback calls the a function that calibrates the SoftRTC
  // for drift in the esp8266's clock.  My guess: after the duration of light_sleep
  // there is bound to be some drift in the clock, so a calibration is due.
  SWTIMER_REG_CB(rtc_callback, SWTIMER_RESUME);

  // The function alarm_timer_common handles timers created by the developer via
  // tmr.create().  No reason not to resume the timers, so resume em'.
  SWTIMER_REG_CB(alarm_timer_common, SWTIMER_RESUME);

	return 0;
}

NODEMCU_MODULE(TMR, "tmr", tmr, luaopen_tmr);
