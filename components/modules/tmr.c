
#include "module.h"
#include "lauxlib.h"
#include <stdint.h>
#include "task/task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

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
  TimerHandle_t timer;
  int32_t cb_ref, self_ref;
  uint32_t interval;
  uint8_t mode;
} tmr_struct_t;
typedef tmr_struct_t *tmr_t;

static task_handle_t alarm_task_id;

static void timer_callback(TimerHandle_t xTimer)
{
  tmr_t tmr = (tmr_t)pvTimerGetTimerID(xTimer);

  task_post_high(alarm_task_id, (task_param_t)tmr);
}

static void alarm_timer_task(task_param_t param, task_prio_t prio)
{
  tmr_t tmr = (tmr_t)param;
  lua_State* L = lua_getstate();
  if (tmr->cb_ref == LUA_NOREF || tmr->self_ref == LUA_NOREF)
    return;
  lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->cb_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, tmr->self_ref);
  // if the timer was set to single run we clean up after it
  if (tmr->mode == TIMER_MODE_SINGLE) {
    luaL_unref(L, LUA_REGISTRYINDEX, tmr->cb_ref);
    tmr->cb_ref = LUA_NOREF;
    tmr->mode = TIMER_MODE_OFF;
  } else if (tmr->mode == TIMER_MODE_SEMI) {
    tmr->mode |= TIMER_IDLE_FLAG;
  }
  if (tmr->mode != TIMER_MODE_AUTO && tmr->self_ref != LUA_REFNIL) {
    luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
    tmr->self_ref = LUA_NOREF;
  }
  lua_call(L, 1, 0);
}

static tmr_t tmr_get( lua_State *L, int stack )
{
  return (tmr_t)luaL_checkudata(L, stack, "tmr.timer");
}

// Lua: tmr.register( self, interval, mode, function )
static int tmr_register(lua_State* L)
{
  int stack = 0;

  tmr_t tmr = tmr_get(L, ++stack);

  uint32_t interval = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, interval > 0 && interval <= MAX_TIMEOUT, stack, MAX_TIMEOUT_ERR_STR);

  uint8_t mode = luaL_checkinteger(L, ++stack);
  luaL_argcheck(L, mode == TIMER_MODE_SINGLE || mode == TIMER_MODE_SEMI || mode == TIMER_MODE_AUTO, stack, "Invalid mode");

  ++stack;
  luaL_argcheck(L, lua_type(L, stack) == LUA_TFUNCTION || lua_type(L, stack) == LUA_TLIGHTFUNCTION, stack, "Must be function");

  if (tmr->timer) {
    // delete previous timer since mode change could be requested here
    xTimerDelete(tmr->timer, portMAX_DELAY);
  }

  //get the lua function reference
  luaL_unref(L, LUA_REGISTRYINDEX, tmr->cb_ref);
  lua_pushvalue(L, stack);
  tmr->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  tmr->mode = mode | TIMER_IDLE_FLAG;
  tmr->interval = interval;
  tmr->timer = xTimerCreate("tmr",
			    pdMS_TO_TICKS(interval),
			    mode == TIMER_MODE_AUTO ? pdTRUE : pdFALSE,
			    (void *)tmr,
			    timer_callback);

  if (!tmr->timer) {
    luaL_error(L, "FreeRTOS timer creation failed");
  }

  return 0;  
}

// Lua: tmr.start( self )
static int tmr_start(lua_State* L)
{
  tmr_t tmr = tmr_get(L, 1);

  if (tmr->self_ref == LUA_NOREF) {
    lua_pushvalue(L, 1);
    tmr->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  if (!tmr->timer || tmr->cb_ref == LUA_NOREF) {
    luaL_error(L, "timer not registered");
  }

  // we return false if the timer is not idle
  if(!(tmr->mode & TIMER_IDLE_FLAG)) {
    lua_pushboolean(L, 0);
  } else {
    tmr->mode &= ~TIMER_IDLE_FLAG;
    if (xTimerStart(tmr->timer, portMAX_DELAY) != pdPASS) {
      luaL_error(L, "timer start failed");
    }
    lua_pushboolean(L, 1);
  }
  return 1;
}

// Lua: tmr.alarm( self, interval, repeat, function )
static int tmr_alarm(lua_State* L){
  tmr_register(L);
  return tmr_start(L);
}

// Lua: tmr.stop( id / ref )
static int tmr_stop(lua_State* L)
{
  tmr_t tmr = tmr_get(L, 1);

  luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
  tmr->self_ref = LUA_NOREF;

  //we return false if the timer is idle (of not registered)
  if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF){
    tmr->mode |= TIMER_IDLE_FLAG;
    xTimerStop(tmr->timer, portMAX_DELAY);
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;  
}

// Lua: tmr.unregister( self )
static int tmr_unregister(lua_State* L){
  tmr_t tmr = tmr_get(L, 1);

  if (tmr->self_ref != LUA_REFNIL) {
    luaL_unref(L, LUA_REGISTRYINDEX, tmr->self_ref);
    tmr->self_ref = LUA_NOREF;
  }

  if(!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF) {
    if (tmr->timer) {
      xTimerStop(tmr->timer, portMAX_DELAY);
    }
  }
  if (tmr->timer) {
    xTimerDelete(tmr->timer, portMAX_DELAY);
  }
  tmr->timer = NULL;
  luaL_unref(L, LUA_REGISTRYINDEX, tmr->cb_ref);
  tmr->cb_ref = LUA_NOREF;
  tmr->mode = TIMER_MODE_OFF; 
  return 0;
}

// Lua: tmr.interval( self, interval )
static int tmr_interval(lua_State* L)
{
  tmr_t tmr = tmr_get(L, 1);

  uint32_t interval = luaL_checkinteger(L, 2);
  luaL_argcheck(L, interval > 0 && interval <= MAX_TIMEOUT, 2, MAX_TIMEOUT_ERR_STR);
  if (tmr->mode != TIMER_MODE_OFF) {
    tmr->interval = interval;
    if (xTimerChangePeriod(tmr->timer,
			   pdMS_TO_TICKS(tmr->interval),
			   portMAX_DELAY) != pdPASS) {
      luaL_error(L, "cannot change period");
    }
    if (tmr->mode & TIMER_IDLE_FLAG) {
      // xTimerChangePeriod will start a dormant timer, thus force stop if it was dormant
      xTimerStop(tmr->timer, portMAX_DELAY);
    }
  }
  return 0;
}

// Lua: tmr.state( self, state )
static int tmr_state(lua_State* L)
{
  tmr_t tmr = tmr_get(L, 1);

  if (tmr->mode == TIMER_MODE_OFF) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushboolean(L, (tmr->mode & TIMER_IDLE_FLAG) == 0);
  lua_pushinteger(L, tmr->mode & (~TIMER_IDLE_FLAG));
  return 2;
}

// Lua: tmr.create()
static int tmr_create( lua_State *L ) {
  tmr_t ud = (tmr_t)lua_newuserdata(L, sizeof(tmr_struct_t));
  if (!ud) return luaL_error(L, "not enough memory");
  luaL_getmetatable(L, "tmr.timer");
  lua_setmetatable(L, -2);
  ud->cb_ref = LUA_NOREF;
  ud->self_ref = LUA_NOREF;
  ud->mode = TIMER_MODE_OFF;
  ud->timer = NULL;
  return 1;
}


// Module function map

LROT_BEGIN(tmr_dyn)
  LROT_FUNCENTRY( register,    tmr_register )
  LROT_FUNCENTRY( alarm,       tmr_alarm )
  LROT_FUNCENTRY( start,       tmr_start )
  LROT_FUNCENTRY( stop,        tmr_stop )
  LROT_FUNCENTRY( unregister,  tmr_unregister )
  LROT_FUNCENTRY( interval,    tmr_interval)
  LROT_FUNCENTRY( state,       tmr_state )
  LROT_FUNCENTRY( __gc,        tmr_unregister )
  LROT_TABENTRY ( __index,     tmr_dyn )
LROT_END(tmr_dyn, NULL, 0)

LROT_BEGIN(tmr)
  LROT_FUNCENTRY( create,       tmr_create )
  LROT_NUMENTRY ( ALARM_SINGLE, TIMER_MODE_SINGLE )
  LROT_NUMENTRY ( ALARM_SEMI,   TIMER_MODE_SEMI )
  LROT_NUMENTRY ( ALARM_AUTO,   TIMER_MODE_AUTO )
LROT_END(tmr, NULL, 0)

static int luaopen_tmr( lua_State *L ){
  luaL_rometatable(L, "tmr.timer", (void *)tmr_dyn_map);

  alarm_task_id = task_get_id(alarm_timer_task);

  return 0;
}

NODEMCU_MODULE(TMR, "tmr", tmr, luaopen_tmr);
