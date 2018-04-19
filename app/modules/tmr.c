/*guys, srsly, turn on warnings in the makefile*/
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/*-------------------------------------
NEW TIMER API
---------------------------------------

tmr.wdclr() -- not changed
tmr.now()   -- not changed
tmr.time()  -- not changed
tmr.delay() -- not changed
tmr.alarm() -- not changed
tmr.stop()  -- changed, see below. use tmr.unregister for old functionality

tmr.register(id, interval, mode, function)
	bind function with timer and set the interval in ms
	the mode can be:
		tmr.ALARM_SINGLE for a single run alarm
		tmr.ALARM_SEMI for a multiple single run alarm
		tmr.ALARM_AUTO for a repating alarm
	tmr.register does NOT start the timer
	tmr.alarm is a tmr.register & tmr.start macro
tmr.unregister(id)
	stop alarm, unbind function and clean up memory
	not needed for ALARM_SINGLE, as it unregisters itself
tmr.start(id)
	ret: bool
	start a alarm, returns true on success
tmr.stop(id)
	ret: bool
	stops a alarm, returns true on success
	this call dose not free any memory, to do so use tmr.unregister
	stopped alarms can be started with start
tmr.interval(id, interval)
	set alarm interval, running alarm will be restarted
tmr.state(id)
	ret: (bool, int) or nil
	returns alarm status (true=started/false=stopped) and mode
	nil if timer is unregistered
tmr.softwd(int)
	set a negative value to stop the timer
	any other value starts the timer, when the
	countdown reaches zero, the device restarts
	the timer units are seconds
*/

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "user_interface.h"
#include "pm/swtimer.h"

#define TIMER_MODE_OFF 3
#define TIMER_MODE_SINGLE 0
#define TIMER_MODE_SEMI 2
#define TIMER_MODE_AUTO 1
#define TIMER_IDLE_FLAG (1<<7)


#define STRINGIFY_VAL(x) #x
#define STRINGIFY(x) STRINGIFY_VAL(x)

// assuming system_timer_reinit() has *not* been called
#define MAX_TIMEOUT_DEF 6870947  //SDK 1.5.3 limit (0x68D7A3)

static const uint32 MAX_TIMEOUT=MAX_TIMEOUT_DEF;
static const char* MAX_TIMEOUT_ERR_STR = "Range: 1-"STRINGIFY(MAX_TIMEOUT_DEF);

typedef struct{
	os_timer_t os;
	sint32_t lua_ref, self_ref;
	uint32_t interval;
	uint8_t mode;
}timer_struct_t;
typedef timer_struct_t* timer_t;

// The previous implementation extended the rtc counter to 64 bits, and then
// applied rtc2sec with the current calibration value to that 64 bit value.
// This means that *ALL* clock ticks since bootup are counted with the *current*
// clock period. In extreme cases (long uptime, sudden temperature change), this
// could result in tmr.time() going backwards....
// This implementation instead applies rtc2usec to short time intervals only (the
// longest being around 1 second), and then accumulates the resulting microseconds
// in a 64 bit counter. That's guaranteed to be monotonic, and should be a lot closer
// to representing an actual uptime.
static uint32_t rtc_time_cali=0;
static uint32_t last_rtc_time=0;
static uint64_t last_rtc_time_us=0;

static sint32_t soft_watchdog  = -1;
static timer_struct_t alarm_timers[NUM_TMR];
static os_timer_t rtc_timer;

static void alarm_timer_common(void* arg){
	timer_t tmr = (timer_t)arg;
	lua_State* L = lua_getstate();
	if(tmr->lua_ref == LUA_NOREF)
		return;
	lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	if (tmr->self_ref == LUA_REFNIL) {
		uint32_t id = tmr - alarm_timers;
		lua_pushinteger(L, id);
	} else {
		lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->self_ref);
	}
	//if the timer was set to single run we clean up after it
	if(tmr->mode == TIMER_MODE_SINGLE){
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
		tmr->lua_ref = LUA_NOREF;
		tmr->mode = TIMER_MODE_OFF;
	}else if(tmr->mode == TIMER_MODE_SEMI){
		tmr->mode |= TIMER_IDLE_FLAG;
	}
	if (tmr->mode != TIMER_MODE_AUTO && tmr->self_ref != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
		tmr->self_ref = LUA_NOREF;
	}
	lua_call(L, 1, 0);
}

// Lua: tmr.delay( us )
static int tmr_delay( lua_State* L ){
	sint32_t us = luaL_checkinteger(L, 1);
	if(us <= 0)
		return luaL_error(L, "wrong arg range");
	while(us >= 1000000){
		us -= 1000000;
		os_delay_us(1000000);
		system_soft_wdt_feed ();
	}
	if(us>0){
		os_delay_us(us);
		system_soft_wdt_feed ();
	}
	return 0; 
}

// Lua: tmr.now() , return system timer in us
static int tmr_now(lua_State* L){
	uint32_t now = 0x7FFFFFFF & system_get_time();
	lua_pushinteger(L, now);
	return 1; 
}

static timer_t tmr_get( lua_State *L, int stack ) {
	// Deprecated: static 0-6 timers control by index.
	luaL_argcheck(L, (lua_isuserdata(L, stack) || lua_isnumber(L, stack)), 1, "timer object or numerical ID expected");
	if (lua_isuserdata(L, stack)) {
		return (timer_t)luaL_checkudata(L, stack, "tmr.timer");
	} else {
		uint32_t id = luaL_checkinteger(L, 1);
		luaL_argcheck(L, platform_tmr_exists(id), 1, "invalid timer index");
		return &alarm_timers[id];
	}
	return 0;
}

// Lua: tmr.register( id / ref, interval, mode, function )
static int tmr_register(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

	uint32_t interval = luaL_checkinteger(L, 2);
	uint8_t mode = luaL_checkinteger(L, 3);

	luaL_argcheck(L, (interval > 0 && interval <= MAX_TIMEOUT), 2, MAX_TIMEOUT_ERR_STR);
	luaL_argcheck(L, (mode == TIMER_MODE_SINGLE || mode == TIMER_MODE_SEMI || mode == TIMER_MODE_AUTO), 3, "Invalid mode");
	luaL_argcheck(L, (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION), 4, "Must be function");
	//get the lua function reference
	lua_pushvalue(L, 4);
	sint32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		os_timer_disarm(&tmr->os);
	//there was a bug in this part, the second part of the following condition was missing
	if(tmr->lua_ref != LUA_NOREF && tmr->lua_ref != ref)
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	tmr->lua_ref = ref;
	tmr->mode = mode|TIMER_IDLE_FLAG;
	tmr->interval = interval;
	os_timer_setfn(&tmr->os, alarm_timer_common, tmr);
	return 0;  
}

// Lua: tmr.start( id / ref )
static int tmr_start(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

	if (tmr->self_ref == LUA_NOREF) {
		lua_pushvalue(L, 1);
		tmr->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	//we return false if the timer is not idle
	if(!(tmr->mode&TIMER_IDLE_FLAG)){
		lua_pushboolean(L, 0);
	}else{
		tmr->mode &= ~TIMER_IDLE_FLAG;
		os_timer_arm(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO);
		lua_pushboolean(L, 1);
	}
	return 1;
}

// Lua: tmr.alarm( id / ref, interval, repeat, function )
static int tmr_alarm(lua_State* L){
	tmr_register(L);
	return tmr_start(L);
}

// Lua: tmr.stop( id / ref )
static int tmr_stop(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

	if (tmr->self_ref != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
		tmr->self_ref = LUA_NOREF;
	}

	//we return false if the timer is idle (of not registered)
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF){
		tmr->mode |= TIMER_IDLE_FLAG;
		os_timer_disarm(&tmr->os);
		lua_pushboolean(L, 1);
	}else{
		lua_pushboolean(L, 0);
	}
	return 1;  
}

#ifdef TIMER_SUSPEND_ENABLE

#define TMR_SUSPEND_REMOVED_MSG "This feature has been removed, we apologize for any inconvenience this may have caused."
static int tmr_suspend(lua_State* L){
  return luaL_error(L, TMR_SUSPEND_REMOVED_MSG);
}

static int tmr_resume(lua_State* L){
  return luaL_error(L, TMR_SUSPEND_REMOVED_MSG);
}

static int tmr_suspend_all (lua_State *L){
  return luaL_error(L, TMR_SUSPEND_REMOVED_MSG);
}

static int tmr_resume_all (lua_State *L){
  return luaL_error(L, TMR_SUSPEND_REMOVED_MSG);
}


#endif

// Lua: tmr.unregister( id / ref )
static int tmr_unregister(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

	if (tmr->self_ref != LUA_REFNIL) {
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
		tmr->self_ref = LUA_NOREF;
	}

	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		os_timer_disarm(&tmr->os);
	if(tmr->lua_ref != LUA_NOREF)
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	tmr->lua_ref = LUA_NOREF;
	tmr->mode = TIMER_MODE_OFF; 
	return 0;
}

// Lua: tmr.interval( id / ref, interval )
static int tmr_interval(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

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

// Lua: tmr.state( id / ref )
static int tmr_state(lua_State* L){
	timer_t tmr = tmr_get(L, 1);

	if(tmr->mode == TIMER_MODE_OFF){
		lua_pushnil(L);
		return 1;
	}

  lua_pushboolean(L, (tmr->mode & TIMER_IDLE_FLAG) == 0);
  lua_pushinteger(L, tmr->mode & (~TIMER_IDLE_FLAG));
	return 2;
}

/*I left the led comments 'couse I don't know
why they are here*/

// extern void update_key_led();
// Lua: tmr.wdclr()
static int tmr_wdclr( lua_State* L ){
	system_soft_wdt_feed ();
	// update_key_led();
	return 0;  
}

//system_rtc_clock_cali_proc() returns
//a fixed point value (12 bit fraction part)
//it tells how many rtc clock ticks represent 1us.
//the high 64 bits of the uint64_t multiplication
//are unnedded (I did the math)
static uint32_t rtc2usec(uint64_t rtc){
	return (rtc*rtc_time_cali)>>12;
}

// This returns the number of microseconds uptime. Note that it relies on the rtc clock,
// which is notoriously temperature dependent
inline static uint64_t rtc_timer_update(bool do_calibration){
	if (do_calibration || rtc_time_cali==0)
		rtc_time_cali=system_rtc_clock_cali_proc();

	uint32_t current = system_get_rtc_time();
	uint32_t since_last=current-last_rtc_time; // This will transparently deal with wraparound
	uint32_t us_since_last=rtc2usec(since_last);
	uint64_t now=last_rtc_time_us+us_since_last;

	// Only update if at least 100ms has passed since we last updated.
	// This prevents the rounding errors in rtc2usec from accumulating
	if (us_since_last>=100000)
	{
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
	soft_watchdog = luaL_checkinteger(L, 1);
	return 0; 
}

// Lua: tmr.create()
static int tmr_create( lua_State *L ) {
	timer_t ud = (timer_t)lua_newuserdata(L, sizeof(timer_struct_t));
	if (!ud) return luaL_error(L, "not enough memory");
	luaL_getmetatable(L, "tmr.timer");
	lua_setmetatable(L, -2);
	ud->lua_ref = LUA_NOREF;
	ud->self_ref = LUA_NOREF;
	ud->mode = TIMER_MODE_OFF;
	os_timer_disarm(&ud->os);
	return 1;
}


#if defined(ENABLE_TIMER_SUSPEND) && defined(SWTMR_DEBUG)
static void tmr_printRegistry(lua_State* L){
  swtmr_print_registry();
}

static void tmr_printSuspended(lua_State* L){
  swtmr_print_suspended();
}

static void tmr_printTimerlist(lua_State* L){
  swtmr_print_timer_list();
}


#endif

// Module function map

static const LUA_REG_TYPE tmr_dyn_map[] = {
	{ LSTRKEY( "register" ),    LFUNCVAL( tmr_register ) },
	{ LSTRKEY( "alarm" ),       LFUNCVAL( tmr_alarm ) },
	{ LSTRKEY( "start" ),       LFUNCVAL( tmr_start ) },
	{ LSTRKEY( "stop" ),        LFUNCVAL( tmr_stop ) },
	{ LSTRKEY( "unregister" ),  LFUNCVAL( tmr_unregister ) },
	{ LSTRKEY( "state" ),       LFUNCVAL( tmr_state ) },
	{ LSTRKEY( "interval" ),    LFUNCVAL( tmr_interval) },
#ifdef TIMER_SUSPEND_ENABLE
	{ LSTRKEY( "suspend" ),      LFUNCVAL( tmr_suspend ) },
  { LSTRKEY( "resume" ),       LFUNCVAL( tmr_resume ) },
#endif
	{ LSTRKEY( "__gc" ),        LFUNCVAL( tmr_unregister ) },
	{ LSTRKEY( "__index" ),     LROVAL( tmr_dyn_map ) },
	{ LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE tmr_map[] = {
	{ LSTRKEY( "delay" ),        LFUNCVAL( tmr_delay ) },
	{ LSTRKEY( "now" ),          LFUNCVAL( tmr_now ) },
	{ LSTRKEY( "wdclr" ),        LFUNCVAL( tmr_wdclr ) },
	{ LSTRKEY( "softwd" ),       LFUNCVAL( tmr_softwd ) },
	{ LSTRKEY( "time" ),         LFUNCVAL( tmr_time ) },
	{ LSTRKEY( "register" ),     LFUNCVAL( tmr_register ) },
	{ LSTRKEY( "alarm" ),        LFUNCVAL( tmr_alarm ) },
	{ LSTRKEY( "start" ),        LFUNCVAL( tmr_start ) },
  { LSTRKEY( "stop" ),         LFUNCVAL( tmr_stop ) },
#ifdef TIMER_SUSPEND_ENABLE
  { LSTRKEY( "suspend" ),      LFUNCVAL( tmr_suspend ) },
  { LSTRKEY( "suspend_all" ),  LFUNCVAL( tmr_suspend_all ) },
  { LSTRKEY( "resume" ),       LFUNCVAL( tmr_resume ) },
  { LSTRKEY( "resume_all" ),   LFUNCVAL( tmr_resume_all ) },
#endif
	{ LSTRKEY( "unregister" ),   LFUNCVAL( tmr_unregister ) },
	{ LSTRKEY( "state" ),        LFUNCVAL( tmr_state ) },
	{ LSTRKEY( "interval" ),     LFUNCVAL( tmr_interval ) },
	{ LSTRKEY( "create" ),       LFUNCVAL( tmr_create ) },
	{ LSTRKEY( "ALARM_SINGLE" ), LNUMVAL( TIMER_MODE_SINGLE ) },
	{ LSTRKEY( "ALARM_SEMI" ),   LNUMVAL( TIMER_MODE_SEMI ) },
	{ LSTRKEY( "ALARM_AUTO" ),   LNUMVAL( TIMER_MODE_AUTO ) },
	{ LNILKEY, LNILVAL }
};

#include "pm/swtimer.h"
int luaopen_tmr( lua_State *L ){
	int i;	

	luaL_rometatable(L, "tmr.timer", (void *)tmr_dyn_map);

	for(i=0; i<NUM_TMR; i++){
		alarm_timers[i].lua_ref = LUA_NOREF;
		alarm_timers[i].self_ref = LUA_REFNIL;
		alarm_timers[i].mode = TIMER_MODE_OFF;
		os_timer_disarm(&alarm_timers[i].os);
	}
	last_rtc_time=system_get_rtc_time(); // Right now is time 0
	last_rtc_time_us=0;

	os_timer_disarm(&rtc_timer);
	os_timer_setfn(&rtc_timer, rtc_callback, NULL);
	os_timer_arm(&rtc_timer, 1000, 1);

  SWTIMER_REG_CB(rtc_callback, SWTIMER_RESUME);
  //The function rtc_callback calls the a function that calibrates the SoftRTC for drift in the esp8266's clock.
  //My guess: after the duration of light_sleep there's bound to be some drift in the clock, so a calibration is due.
  SWTIMER_REG_CB(alarm_timer_common, SWTIMER_RESUME);
  //The function alarm_timer_common handles timers created by the developer via tmr.create().
  //No reason not to resume the timers, so resume em'.


	return 0;
}

NODEMCU_MODULE(TMR, "tmr", tmr_map, luaopen_tmr);
