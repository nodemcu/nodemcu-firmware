/*
 * Module for interfacing with cheap rotary switches that
 * are much used in the automtive industry as the cntrols for 
 * CD players and the like.
 *
 * Philip Gladstone, N1DQ
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "user_interface.h"
#include "driver/rotary.h"
#include "../libc/c_stdlib.h"

#define MASK(x)		(1 << ROTARY_ ## x ## _INDEX)

#define ROTARY_PRESS_INDEX	0
#define ROTARY_LONGPRESS_INDEX	1
#define ROTARY_RELEASE_INDEX	2
#define ROTARY_TURN_INDEX	3
#define ROTARY_CLICK_INDEX	4
#define ROTARY_DBLCLICK_INDEX	5

#define ROTARY_ALL		0x3f

#define LONGPRESS_DELAY_US 	500000
#define CLICK_DELAY_US 		500000

#define CALLBACK_COUNT	6

#ifdef LUA_USE_MODULES_ROTARY
#if !defined(GPIO_INTERRUPT_ENABLE) || !defined(GPIO_INTERRUPT_HOOK_ENABLE)
#error Must have GPIO_INTERRUPT and GPIO_INTERRUPT_HOOK if using ROTARY module
#endif
#endif

typedef struct {
  int lastpos;
  int last_recent_event_was_press : 1;
  int last_recent_event_was_release : 1;
  int timer_running : 1;
  int possible_dbl_click : 1;
  uint8_t id;
  int click_delay_us;
  int longpress_delay_us;
  uint32_t last_event_time;
  int callback[CALLBACK_COUNT];
  ETSTimer timer;
} DATA;

static DATA *data[ROTARY_CHANNEL_COUNT];
static task_handle_t tasknumber;
static void lrotary_timer_done(void *param);
static void lrotary_check_timer(DATA *d, uint32_t time_us, bool dotimer);

static void callback_free_one(lua_State *L, int *cb_ptr) 
{
  if (*cb_ptr != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, *cb_ptr);
    *cb_ptr = LUA_NOREF;
  }
}

static void callback_free(lua_State* L, unsigned int id, int mask) 
{
  DATA *d = data[id];

  if (d) {
    int i;
    for (i = 0; i < CALLBACK_COUNT; i++) {
      if (mask & (1 << i)) {
	callback_free_one(L, &d->callback[i]);
      }
    }
  }
}

static int callback_setOne(lua_State* L, int *cb_ptr, int arg_number) 
{
  if (lua_type(L, arg_number) == LUA_TFUNCTION || lua_type(L, arg_number) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, arg_number);  // copy argument (func) to the top of stack
    callback_free_one(L, cb_ptr);
    *cb_ptr = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
  }

  return -1;
}

static int callback_set(lua_State* L, int id, int mask, int arg_number) 
{
  DATA *d = data[id];
  int result = 0;

  int i;
  for (i = 0; i < CALLBACK_COUNT; i++) {
    if (mask & (1 << i)) {
      result |= callback_setOne(L, &d->callback[i], arg_number);
    }
  }

  return result;
}

static void callback_callOne(lua_State* L, int cb, int mask, int arg, uint32_t time) 
{
  if (cb != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, cb);

    lua_pushinteger(L, mask);
    lua_pushinteger(L, arg);
    lua_pushinteger(L, time);

    lua_call(L, 3, 0);
  }
}

static void callback_call(lua_State* L, DATA *d, int cbnum, int arg, uint32_t time) 
{
  if (d) {
    callback_callOne(L, d->callback[cbnum], 1 << cbnum, arg, time);
  }
}

int platform_rotary_exists( unsigned int id )
{
  return (id < ROTARY_CHANNEL_COUNT);
}

#include "pm/swtimer.h"

// Lua: setup(id, phase_a, phase_b [, press])
static int lrotary_setup( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( rotary, id );

  if (rotary_close(id)) {
    return luaL_error( L, "Unable to close switch." );
  }
  callback_free(L, id, ROTARY_ALL);

  if (!data[id]) {
    data[id] = (DATA *) c_zalloc(sizeof(DATA));
    if (!data[id]) {
      return -1;
    } 
  }

  DATA *d = data[id];
  memset(d, 0, sizeof(*d));

  d->id = id;

  os_timer_setfn(&d->timer, lrotary_timer_done, (void *) d);
  SWTIMER_REG_CB(lrotary_timer_done, SWTIMER_RESUME);
    //lrotary_timer_done checks time elapsed since last event
    //My guess: Since proper functionality relies on some variables to be reset via timer callback and state would be invalid anyway.
      //It is probably best to resume this timer so it can reset it's state variables

  
  int i;
  for (i = 0; i < CALLBACK_COUNT; i++) {
    d->callback[i] = LUA_NOREF;
  }

  d->click_delay_us = CLICK_DELAY_US;
  d->longpress_delay_us = LONGPRESS_DELAY_US;

  int phase_a = luaL_checkinteger(L, 2);
  luaL_argcheck(L, platform_gpio_exists(phase_a) && phase_a > 0, 2, "Invalid pin");
  int phase_b = luaL_checkinteger(L, 3);
  luaL_argcheck(L, platform_gpio_exists(phase_b) && phase_b > 0, 3, "Invalid pin");
  int press;
  if (lua_gettop(L) >= 4) {
    press = luaL_checkinteger(L, 4);
    luaL_argcheck(L, platform_gpio_exists(press) && press > 0, 4, "Invalid pin");
  } else {
    press = -1;
  }

  if (lua_gettop(L) >= 5) {
    d->longpress_delay_us = 1000 * luaL_checkinteger(L, 5);
    luaL_argcheck(L, d->longpress_delay_us > 0, 5, "Invalid timeout");
  }

  if (lua_gettop(L) >= 6) {
    d->click_delay_us = 1000 * luaL_checkinteger(L, 6);
    luaL_argcheck(L, d->click_delay_us > 0, 6, "Invalid timeout");
  }

  if (rotary_setup(id, phase_a, phase_b, press, tasknumber)) {
    return luaL_error(L, "Unable to setup rotary switch.");
  }
  return 0;  
}

// Lua: close( id )
static int lrotary_close( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( rotary, id );
  callback_free(L, id, ROTARY_ALL);

  DATA *d = data[id];
  if (d) {
    data[id] = NULL;
    c_free(d);
  }

  if (rotary_close( id )) {
    return luaL_error( L, "Unable to close switch." );
  }
  return 0;  
}

// Lua: on( id, mask[, cb] )
static int lrotary_on( lua_State* L )
{
  unsigned int id;
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( rotary, id );

  int mask = luaL_checkinteger(L, 2);

  if (lua_gettop(L) >= 3) {
    if (callback_set(L, id, mask, 3)) {
      return luaL_error( L, "Unable to set callback." );
    }
  } else {
    callback_free(L, id, mask);
  }

  return 0;  
}

// Lua: getpos( id ) -> pos, PRESS/RELEASE
static int lrotary_getpos( lua_State* L )
{
  unsigned int id;
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( rotary, id );

  int pos = rotary_getpos(id);

  if (pos == -1) {
    return 0;
  }

  lua_pushnumber(L, (pos << 1) >> 1);
  lua_pushnumber(L, (pos & 0x80000000) ? MASK(PRESS) : MASK(RELEASE));

  return 2;  
}

// Returns TRUE if there maybe/is more stuff to do
static bool lrotary_dequeue_single(lua_State* L, DATA *d)
{
  bool something_pending = FALSE;

  if (d) {
    // This chnnel is open
    rotary_event_t result;

    if (rotary_getevent(d->id, &result)) {
      int pos = result.pos;

      lrotary_check_timer(d, result.time_us, 0);

      if (pos != d->lastpos) {
	// We have something to enqueue
	if ((pos ^ d->lastpos) & 0x7fffffff) {
	  // Some turning has happened
	  callback_call(L, d, ROTARY_TURN_INDEX, (pos << 1) >> 1, result.time_us);
	}
	if ((pos ^ d->lastpos) & 0x80000000) {
	  // pressing or releasing has happened
	  callback_call(L, d, (pos & 0x80000000) ? ROTARY_PRESS_INDEX : ROTARY_RELEASE_INDEX, (pos << 1) >> 1, result.time_us);
	  if (pos & 0x80000000) {
	    // Press
	    if (d->last_recent_event_was_release && result.time_us - d->last_event_time < d->click_delay_us) {
	      d->possible_dbl_click = 1;
	    }
	    d->last_recent_event_was_press = 1;
	    d->last_recent_event_was_release = 0;
	  } else {
	    // Release
	    d->last_recent_event_was_press = 0;
	    if (d->possible_dbl_click) {
	      callback_call(L, d, ROTARY_DBLCLICK_INDEX, (pos << 1) >> 1, result.time_us);
	      d->possible_dbl_click = 0;
	      // Do this to suppress the CLICK event
	      d->last_recent_event_was_release = 0;
	    } else {
	      d->last_recent_event_was_release = 1;
	    }
	  }
	  d->last_event_time = result.time_us;
	}

	d->lastpos = pos;
      }

      something_pending = rotary_has_queued_event(d->id);
    }

    lrotary_check_timer(d, system_get_time(), 1);
  }

  return something_pending;
}

static void lrotary_timer_done(void *param)
{
  DATA *d = (DATA *) param;

  d->timer_running = 0;

  lrotary_check_timer(d, system_get_time(), 1);
}

static void lrotary_check_timer(DATA *d, uint32_t time_us, bool dotimer)
{
  uint32_t delay = time_us - d->last_event_time;
  if (d->timer_running) {
    os_timer_disarm(&d->timer);
    d->timer_running = 0;
  }

  int timeout = -1;

  if (d->last_recent_event_was_press) {
    if (delay > d->longpress_delay_us) {
      callback_call(lua_getstate(), d, ROTARY_LONGPRESS_INDEX, (d->lastpos << 1) >> 1, d->last_event_time + d->longpress_delay_us);
      d->last_recent_event_was_press = 0;
    } else {
      timeout = (d->longpress_delay_us - delay) / 1000;
    }
  }
  if (d->last_recent_event_was_release) {
    if (delay > d->click_delay_us) {
      callback_call(lua_getstate(), d, ROTARY_CLICK_INDEX, (d->lastpos << 1) >> 1, d->last_event_time + d->click_delay_us);
      d->last_recent_event_was_release = 0;
    } else {
      timeout = (d->click_delay_us - delay) / 1000;
    }
  }

  if (dotimer && timeout >= 0) {
    d->timer_running = 1;
    os_timer_arm(&d->timer, timeout + 1, 0);
  }
}

static void lrotary_task(os_param_t param, uint8_t prio) 
{
  (void) param;
  (void) prio;

  uint8_t *task_queue_ptr = (uint8_t*) param;
  if (task_queue_ptr) {
    // Signal that new events may need another task post
    *task_queue_ptr = 0;
  }

  int id;
  bool need_to_post = FALSE;
  lua_State *L = lua_getstate();

  for (id = 0; id < ROTARY_CHANNEL_COUNT; id++) {
    DATA *d = data[id];
    if (d) {
      if (lrotary_dequeue_single(L, d)) {
	need_to_post = TRUE;
      }
    }
  }
  
  if (need_to_post) {
    // If there is pending stuff, queue another task
    task_post_medium(tasknumber, 0);
  }
}

static int rotary_open(lua_State *L) 
{
  tasknumber = task_get_id(lrotary_task);
  return 0;
}

// Module function map
static const LUA_REG_TYPE rotary_map[] = {
  { LSTRKEY( "setup" ),    LFUNCVAL( lrotary_setup ) },
  { LSTRKEY( "close" ),    LFUNCVAL( lrotary_close ) },
  { LSTRKEY( "on" ),       LFUNCVAL( lrotary_on    ) },
  { LSTRKEY( "getpos" ),   LFUNCVAL( lrotary_getpos) },
  { LSTRKEY( "TURN" ),     LNUMVAL( MASK(TURN)    ) },
  { LSTRKEY( "PRESS" ),    LNUMVAL( MASK(PRESS)   ) },
  { LSTRKEY( "RELEASE" ),  LNUMVAL( MASK(RELEASE) ) },
  { LSTRKEY( "LONGPRESS" ),LNUMVAL( MASK(LONGPRESS) ) },
  { LSTRKEY( "CLICK" ),    LNUMVAL( MASK(CLICK)   ) },
  { LSTRKEY( "DBLCLICK" ), LNUMVAL( MASK(DBLCLICK)) },
  { LSTRKEY( "ALL" ),      LNUMVAL( ROTARY_ALL     ) },

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(ROTARY, "rotary", rotary_map, rotary_open);
