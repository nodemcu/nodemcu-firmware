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

#define MIN_OPT_LEVEL 2

#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "lrodefs.h"
#include "c_types.h"

#define TIMER_MODE_OFF 3
#define TIMER_MODE_SINGLE 0
#define TIMER_MODE_SEMI 2
#define TIMER_MODE_AUTO 1
#define TIMER_IDLE_FLAG (1<<7) 

//well, the following are my assumptions
//why, oh why is there no good documentation
//chinese companies should learn from Atmel
extern void ets_timer_arm_new(os_timer_t* t, uint32_t milliseconds, uint32_t repeat_flag, uint32_t isMstimer);
extern void ets_timer_disarm(os_timer_t* t);
extern void ets_timer_setfn(os_timer_t* t, os_timer_func_t *f, void *arg);
extern void ets_delay_us(uint32_t us);
extern uint32_t system_get_time();
extern uint32_t platform_tmr_exists(uint32_t t);
extern uint32_t system_rtc_clock_cali_proc();
extern uint32_t system_get_rtc_time();
extern void system_restart();
extern void system_soft_wdt_feed();

//in fact lua_State is constant, it's pointless to pass it around
//but hey, whatever, I'll just pass it, still we waste 28B here
typedef struct{
	os_timer_t os;
	lua_State* L;
	sint32_t lua_ref;
	uint32_t interval;
	uint8_t mode;
}timer_struct_t;
typedef timer_struct_t* timer_t;

//everybody just love unions! riiiiight?
static union {
	uint64_t block;
	uint32_t part[2];
} rtc_time;
static sint32_t soft_watchdog  = -1;
static timer_struct_t alarm_timers[NUM_TMR];
static os_timer_t rtc_timer;

static void alarm_timer_common(void* arg){
	timer_t tmr = &alarm_timers[(uint32_t)arg];
	if(tmr->lua_ref == LUA_NOREF || tmr->L == NULL)
		return;
	lua_rawgeti(tmr->L, LUA_REGISTRYINDEX, tmr->lua_ref);
	//if the timer was set to single run we clean up after it
	if(tmr->mode == TIMER_MODE_SINGLE){
		luaL_unref(tmr->L, LUA_REGISTRYINDEX, tmr->lua_ref);
		tmr->lua_ref = LUA_NOREF;
		tmr->mode = TIMER_MODE_OFF;
	}else if(tmr->mode == TIMER_MODE_SEMI){
		tmr->mode |= TIMER_IDLE_FLAG;
	}
	lua_call(tmr->L, 0, 0);
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

// Lua: tmr.register( id, interval, mode, function )
static int tmr_register(lua_State* L){
	uint32_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr, id);
	sint32_t interval = luaL_checkinteger(L, 2);
	uint8_t mode = luaL_checkinteger(L, 3);
	//validate arguments
	uint8_t args_valid = interval <= 0
		|| (mode != TIMER_MODE_SINGLE && mode != TIMER_MODE_SEMI && mode != TIMER_MODE_AUTO)
		|| (lua_type(L, 4) != LUA_TFUNCTION && lua_type(L, 4) != LUA_TLIGHTFUNCTION);
	if(args_valid)
		return luaL_error(L, "wrong arg range");
	//get the lua function reference
	lua_pushvalue(L, 4);
	sint32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
	timer_t tmr = &alarm_timers[id];
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		ets_timer_disarm(&tmr->os);
	//there was a bug in this part, the second part of the following condition was missing
	if(tmr->lua_ref != LUA_NOREF && tmr->lua_ref != ref)
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	tmr->lua_ref = ref;
	tmr->mode = mode|TIMER_IDLE_FLAG;
	tmr->interval = interval;
	tmr->L = L; 
	ets_timer_setfn(&tmr->os, alarm_timer_common, (void*)id);
	return 0;  
}

// Lua: tmr.start( id )
static int tmr_start(lua_State* L){
	uint8_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr,id);
	timer_t tmr = &alarm_timers[id];
	//we return false if the timer is not idle
	if(!(tmr->mode&TIMER_IDLE_FLAG)){
		lua_pushboolean(L, 0);
	}else{
		tmr->mode &= ~TIMER_IDLE_FLAG;
		ets_timer_arm_new(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO, 1);
		lua_pushboolean(L, 1);
	}
	return 1;
}

// Lua: tmr.alarm( id, interval, repeat, function )
static int tmr_alarm(lua_State* L){
	tmr_register(L);
	return tmr_start(L);
}

// Lua: tmr.stop( id )
static int tmr_stop(lua_State* L){
	uint8_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr,id);
	timer_t tmr = &alarm_timers[id];
	//we return false if the timer is idle (of not registered)
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF){
		tmr->mode |= TIMER_IDLE_FLAG;
		ets_timer_disarm(&tmr->os);
		lua_pushboolean(L, 1);
	}else{
		lua_pushboolean(L, 0);
	}
	return 1;  
}

// Lua: tmr.unregister( id )
static int tmr_unregister(lua_State* L){
	uint8_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr,id);
	timer_t tmr = &alarm_timers[id];
	if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
		ets_timer_disarm(&tmr->os);
	if(tmr->lua_ref != LUA_NOREF)
		luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
	tmr->lua_ref = LUA_NOREF;
	tmr->mode = TIMER_MODE_OFF; 
	return 0;
}

// Lua: tmr.interval( id, interval )
static int tmr_interval(lua_State* L){
	uint8_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr,id);
	timer_t tmr = &alarm_timers[id];
	sint32_t interval = luaL_checkinteger(L, 2);
	if(interval <= 0)
		return luaL_error(L, "wrong arg range");
	if(tmr->mode != TIMER_MODE_OFF){	
		tmr->interval = interval;
		if(!(tmr->mode&TIMER_IDLE_FLAG)){
			ets_timer_disarm(&tmr->os);
			ets_timer_arm_new(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO, 1);
		}
	}
	return 0;
}

// Lua: tmr.state( id )
static int tmr_state(lua_State* L){
	uint8_t id = luaL_checkinteger(L, 1);
	MOD_CHECK_ID(tmr,id);
	timer_t tmr = &alarm_timers[id];
	if(tmr->mode == TIMER_MODE_OFF){
		lua_pushnil(L);
		return 1;
	}
	lua_pushboolean(L, (tmr->mode&TIMER_IDLE_FLAG)==0);
	lua_pushinteger(L, tmr->mode&(~TIMER_IDLE_FLAG));
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
static uint32_t rtc2sec(uint64_t rtc){
	uint64_t aku = system_rtc_clock_cali_proc();
	aku *= rtc;
	return (aku>>12)/1000000;
}

//the following function workes, I just wrote it and didn't use it.
/*static uint64_t sec2rtc(uint32_t sec){
	uint64_t aku = (1<<20)/system_rtc_clock_cali_proc();
	aku *= sec;
	return (aku>>8)*1000000;
}*/

inline static void rtc_timer_update(){
	uint32_t current = system_get_rtc_time();
	if(rtc_time.part[0] > current) //overflow check
		rtc_time.part[1]++;
	rtc_time.part[0] = current;
}

void rtc_callback(void *arg){
	rtc_timer_update();
	if(soft_watchdog > 0){
		soft_watchdog--;
		if(soft_watchdog == 0)
			system_restart();
	}
}

// Lua: tmr.time() , return rtc time in second
static int tmr_time( lua_State* L ){
	rtc_timer_update();
	lua_pushinteger(L, rtc2sec(rtc_time.block));
	return 1; 
}

// Lua: tmr.softwd( value )
static int tmr_softwd( lua_State* L ){
	soft_watchdog = luaL_checkinteger(L, 1);
	return 0; 
}

// Module function map

const LUA_REG_TYPE tmr_map[] = {
	{ LSTRKEY( "delay" ), LFUNCVAL( tmr_delay ) },
	{ LSTRKEY( "now" ), LFUNCVAL( tmr_now ) },
	{ LSTRKEY( "wdclr" ), LFUNCVAL( tmr_wdclr ) },
	{ LSTRKEY( "softwd" ), LFUNCVAL( tmr_softwd ) },
	{ LSTRKEY( "time" ), LFUNCVAL( tmr_time ) },
	{ LSTRKEY( "register" ), LFUNCVAL ( tmr_register ) },
	{ LSTRKEY( "alarm" ), LFUNCVAL( tmr_alarm ) },
	{ LSTRKEY( "start" ), LFUNCVAL ( tmr_start ) },
	{ LSTRKEY( "stop" ), LFUNCVAL ( tmr_stop ) },
	{ LSTRKEY( "unregister" ), LFUNCVAL ( tmr_unregister ) },
	{ LSTRKEY( "state" ), LFUNCVAL ( tmr_state ) },
	{ LSTRKEY( "interval" ), LFUNCVAL ( tmr_interval) }, 
#if LUA_OPTIMIZE_MEMORY > 0
	{ LSTRKEY( "ALARM_SINGLE" ), LNUMVAL( TIMER_MODE_SINGLE ) },
	{ LSTRKEY( "ALARM_SEMI" ), LNUMVAL( TIMER_MODE_SEMI ) },
	{ LSTRKEY( "ALARM_AUTO" ), LNUMVAL( TIMER_MODE_AUTO ) },
#endif
	{ LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_tmr( lua_State *L ){
	int i;	
	for(i=0; i<NUM_TMR; i++){
		alarm_timers[i].lua_ref = LUA_NOREF;
		alarm_timers[i].mode = TIMER_MODE_OFF;
		ets_timer_disarm(&alarm_timers[i].os);
	}
	rtc_time.block = 0;
	ets_timer_disarm(&rtc_timer);
	ets_timer_setfn(&rtc_timer, rtc_callback, NULL);
	ets_timer_arm_new(&rtc_timer, 1000, 1, 1);

#if LUA_OPTIMIZE_MEMORY > 0
	return 0;
#else
	luaL_register( L, AUXLIB_TMR, tmr_map );
	lua_pushvalue( L, -1 );
	lua_setmetatable( L, -2 );
	MOD_REG_NUMBER( L, "ALARM_SINGLE", TIMER_MODE_SINGLE );
	MOD_REG_NUMBER( L, "ALARM_SEMI", TIMER_MODE_SEMI );
	MOD_REG_NUMBER( L, "ALARM_AUTO", TIMER_MODE_AUTO );
	return 1;
#endif
}

