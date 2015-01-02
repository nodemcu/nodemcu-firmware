// Module for interfacing with timer

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"

static os_timer_t alarm_timer[NUM_TMR];
static int alarm_timer_cb_ref[NUM_TMR] = {LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF,LUA_NOREF};

void ICACHE_FLASH_ATTR alarm_timer_common(lua_State* L, unsigned id){
  if(alarm_timer_cb_ref[id] == LUA_NOREF)
    return;
  lua_rawgeti(L, LUA_REGISTRYINDEX, alarm_timer_cb_ref[id]);
  lua_call(L, 0, 0);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb0(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 0);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb1(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 1);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb2(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 2);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb3(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 3);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb4(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 4);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb5(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 5);
}

void ICACHE_FLASH_ATTR
alarm_timer_cb6(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 6);
}

typedef void (*alarm_timer_callback)(void *arg);
static alarm_timer_callback alarm_timer_cb[NUM_TMR] = {alarm_timer_cb0,alarm_timer_cb1,alarm_timer_cb2,alarm_timer_cb3,alarm_timer_cb4,alarm_timer_cb5,alarm_timer_cb6};

// Lua: delay( us )
static int ICACHE_FLASH_ATTR tmr_delay( lua_State* L )
{
  s32 us;
  us = luaL_checkinteger( L, 1 );
  if ( us <= 0 )
    return luaL_error( L, "wrong arg range" );
  unsigned sec = (unsigned)us / 1000000;
  unsigned remain = (unsigned)us % 1000000;
  int i = 0;
  for(i=0;i<sec;i++){
    os_delay_us( 1000000 );
    WRITE_PERI_REG(0x60000914, 0x73);
  }
  if(remain>0)
    os_delay_us( remain );
  return 0;  
}

// Lua: now() , return system timer in us
static int ICACHE_FLASH_ATTR tmr_now( lua_State* L )
{
  unsigned now = 0x7FFFFFFF & system_get_time();
  lua_pushinteger( L, now );
  return 1; 
}

// Lua: alarm( id, interval, repeat, function )
static int ICACHE_FLASH_ATTR tmr_alarm( lua_State* L )
{
  s32 interval;
  unsigned repeat = 0;
  int stack = 1;
  
  unsigned id = luaL_checkinteger( L, stack );
  stack++;
  MOD_CHECK_ID( tmr, id );

  interval = luaL_checkinteger( L, stack );
  stack++;
  if ( interval <= 0 )
    return luaL_error( L, "wrong arg range" );

  if ( lua_isnumber(L, stack) ){
    repeat = lua_tointeger(L, stack);
    stack++;
    if ( repeat != 1 && repeat != 0 )
      return luaL_error( L, "wrong arg type" );
  }

  // luaL_checkanyfunction(L, stack);
  if (lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION){
    lua_pushvalue(L, stack);  // copy argument (func) to the top of stack
    if(alarm_timer_cb_ref[id] != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, alarm_timer_cb_ref[id]);
    alarm_timer_cb_ref[id] = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  os_timer_disarm(&(alarm_timer[id]));
  os_timer_setfn(&(alarm_timer[id]), (os_timer_func_t *)(alarm_timer_cb[id]), L);
  os_timer_arm(&(alarm_timer[id]), interval, repeat); 
  return 0;  
}

// Lua: stop( id )
static int ICACHE_FLASH_ATTR tmr_stop( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( tmr, id );

  os_timer_disarm(&(alarm_timer[id]));
  return 0;  
}

// extern void update_key_led();
// Lua: wdclr()
static int ICACHE_FLASH_ATTR tmr_wdclr( lua_State* L )
{
  WRITE_PERI_REG(0x60000914, 0x73);
  // update_key_led();
  return 0;  
}

// Lua: time() , return rtc time in us
static int ICACHE_FLASH_ATTR tmr_time( lua_State* L )
{
  unsigned t = 0xFFFFFFFF & system_get_rtc_time();
  unsigned c = 0xFFFFFFFF & system_rtc_clock_cali_proc();
  lua_pushinteger( L, t );
  lua_pushinteger( L, c );
  return 2; 
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE tmr_map[] = 
{
  { LSTRKEY( "delay" ), LFUNCVAL( tmr_delay ) },
  { LSTRKEY( "now" ), LFUNCVAL( tmr_now ) },
  { LSTRKEY( "alarm" ), LFUNCVAL( tmr_alarm ) },
  { LSTRKEY( "stop" ), LFUNCVAL( tmr_stop ) },
  { LSTRKEY( "wdclr" ), LFUNCVAL( tmr_wdclr ) },
  { LSTRKEY( "time" ), LFUNCVAL( tmr_time ) },
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int ICACHE_FLASH_ATTR luaopen_tmr( lua_State *L )
{
  int i = 0;
  for(i=0;i<NUM_TMR;i++){
    os_timer_disarm(&(alarm_timer[i]));
    os_timer_setfn(&(alarm_timer[i]), (os_timer_func_t *)(alarm_timer_cb[i]), L);
  }

#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_TMR, tmr_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
