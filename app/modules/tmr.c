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

void alarm_timer_common(lua_State* L, unsigned id){
  if(alarm_timer_cb_ref[id] == LUA_NOREF)
    return;
  lua_rawgeti(L, LUA_REGISTRYINDEX, alarm_timer_cb_ref[id]);
  lua_call(L, 0, 0);
}

void alarm_timer_cb0(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 0);
}

void alarm_timer_cb1(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 1);
}

void alarm_timer_cb2(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 2);
}

void alarm_timer_cb3(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 3);
}

void alarm_timer_cb4(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 4);
}

void alarm_timer_cb5(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 5);
}

void alarm_timer_cb6(void *arg){
  if( !arg )
    return;
  alarm_timer_common((lua_State*)arg, 6);
}

typedef void (*alarm_timer_callback)(void *arg);
static alarm_timer_callback alarm_timer_cb[NUM_TMR] = {alarm_timer_cb0,alarm_timer_cb1,alarm_timer_cb2,alarm_timer_cb3,alarm_timer_cb4,alarm_timer_cb5,alarm_timer_cb6};

// Lua: delay( us )
static int tmr_delay( lua_State* L )
{
  s32 us;
  us = luaL_checkinteger( L, 1 );
  if ( us <= 0 )
    return luaL_error( L, "wrong arg range" );
  if(us<1000000)
  {
    os_delay_us( us );
    WRITE_PERI_REG(0x60000914, 0x73);
    return 0;
  }  
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
static int tmr_now( lua_State* L )
{
  unsigned now = 0x7FFFFFFF & system_get_time();
  lua_pushinteger( L, now );
  return 1; 
}

// Lua: alarm( id, interval, repeat, function )
static int tmr_alarm( lua_State* L )
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
static int tmr_stop( lua_State* L )
{
  unsigned id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( tmr, id );

  os_timer_disarm(&(alarm_timer[id]));
  return 0;  
}

// extern void update_key_led();
// Lua: wdclr()
static int tmr_wdclr( lua_State* L )
{
  WRITE_PERI_REG(0x60000914, 0x73);
  // update_key_led();
  return 0;  
}

static os_timer_t rtc_timer_updator;
static uint32_t cur_count = 0;
static uint32_t rtc_10ms = 0;
void rtc_timer_update_cb(void *arg){
  uint32_t t = (uint32_t)system_get_rtc_time();
  uint32_t delta = 0;
  if(t>=cur_count){
    delta = t-cur_count;
  }else{
    delta = 0xFFFFFFF - cur_count + t + 1;
  }
  // uint64_t delta = (t>=cur_count)?(t - cur_count):(0x100000000 + t - cur_count);
  // NODE_ERR("%x\n",t);
  cur_count = t;
  uint32_t c = system_rtc_clock_cali_proc();
  uint32_t itg = c >> 12;  // ~=5
  uint32_t dec = c & 0xFFF; // ~=2ff
  rtc_10ms += (delta*itg + ((delta*dec)>>12)) / 10000;
  // TODO: store rtc_10ms to rtc memory.
}
// Lua: time() , return rtc time in second
static int tmr_time( lua_State* L )
{
  uint32_t local = rtc_10ms;
  lua_pushinteger( L, ((uint32_t)(local/100)) & 0x7FFFFFFF );
  return 1; 
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

LUALIB_API int luaopen_tmr( lua_State *L )
{
  int i = 0;
  for(i=0;i<NUM_TMR;i++){
    os_timer_disarm(&(alarm_timer[i]));
    os_timer_setfn(&(alarm_timer[i]), (os_timer_func_t *)(alarm_timer_cb[i]), L);
  }

  os_timer_disarm(&rtc_timer_updator);
  os_timer_setfn(&rtc_timer_updator, (os_timer_func_t *)(rtc_timer_update_cb), NULL);
  os_timer_arm(&rtc_timer_updator, 500, 1); 

#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_TMR, tmr_map );
  // Add constants

  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
