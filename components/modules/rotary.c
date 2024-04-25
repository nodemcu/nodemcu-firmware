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
#include "task/task.h"
#include "esp_timer.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "rotary_driver.h"

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

typedef struct {
  int lastpos;
  int last_recent_event_was_press : 1;
  int last_recent_event_was_release : 1;
  int timer_running : 1;
  int possible_dbl_click : 1;
  struct rotary_driver_handle *handle;
  int click_delay_us;
  int longpress_delay_us;
  uint32_t last_event_time;
  int callback[CALLBACK_COUNT];
  esp_timer_handle_t timer_handle;
  int self_ref;
} DATA;

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

static void callback_free(lua_State* L, DATA *d, int mask)
{
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
  if (lua_isfunction(L, arg_number)) {
    lua_pushvalue(L, arg_number);  // copy argument (func) to the top of stack
    callback_free_one(L, cb_ptr);
    *cb_ptr = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
  }

  return -1;
}

static int callback_set(lua_State* L, DATA *d, int mask, int arg_number)
{
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

    luaL_pcallx(L, 3, 0);
  }
}

static void callback_call(lua_State* L, DATA *d, int cbnum, int arg, uint32_t time)
{
  if (d) {
    callback_callOne(L, d->callback[cbnum], 1 << cbnum, arg, time);
  }
}

// Lua: setup(phase_a, phase_b [, press])
static int lrotary_setup( lua_State* L )
{
  int nargs = lua_gettop(L);

  DATA *d = (DATA *)lua_newuserdata(L, sizeof(DATA));
  if (!d) return luaL_error(L, "not enough memory");
  memset(d, 0, sizeof(*d));
  luaL_getmetatable(L, "rotary.switch");
  lua_setmetatable(L, -2);

  esp_timer_create_args_t timer_args = {
    .callback = lrotary_timer_done,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "rotary_timer",
    .arg = d
  };

  esp_timer_create(&timer_args, &d->timer_handle);

  int i;
  for (i = 0; i < CALLBACK_COUNT; i++) {
    d->callback[i] = LUA_NOREF;
  }

  d->self_ref = LUA_NOREF;

  d->click_delay_us = CLICK_DELAY_US;
  d->longpress_delay_us = LONGPRESS_DELAY_US;

  int phase_a = luaL_checkinteger(L, 1);
  luaL_argcheck(L, platform_gpio_exists(phase_a), 1, "Invalid pin");
  int phase_b = luaL_checkinteger(L, 2);
  luaL_argcheck(L, platform_gpio_exists(phase_b), 2, "Invalid pin");
  int press;
  if (nargs >= 3) {
    press = luaL_checkinteger(L, 3);
    luaL_argcheck(L, platform_gpio_exists(press), 3, "Invalid pin");
  } else {
    press = -1;
  }

  if (nargs >= 4) {
    d->longpress_delay_us = 1000 * luaL_checkinteger(L, 4);
    luaL_argcheck(L, d->longpress_delay_us > 0, 4, "Invalid timeout");
  }

  if (nargs >= 5) {
    d->click_delay_us = 1000 * luaL_checkinteger(L, 5);
    luaL_argcheck(L, d->click_delay_us > 0, 5, "Invalid timeout");
  }

  d->handle = rotary_setup(phase_a, phase_b, press, tasknumber, d);
  if (!d->handle) {
    return luaL_error(L, "Unable to setup rotary switch.");
  }
  return 1;
}

static void update_self_ref(lua_State *L, DATA *d, int argnum) {
  bool have_callback = false;
  for (int i = 0; i < CALLBACK_COUNT; i++) {
    if (d->callback[i] != LUA_NOREF) {
      have_callback = true;
      break;
    }
  }
  if (have_callback) {
    if (d->self_ref == LUA_NOREF && argnum > 0) {
      lua_pushvalue(L, argnum);
      d->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
  } else {
    if (d->self_ref != LUA_NOREF && !rotary_has_queued_event(d->handle)) {
      luaL_unref(L, LUA_REGISTRYINDEX, d->self_ref);
      d->self_ref = LUA_NOREF;
    }
  }
}

// Lua: close( )
static int lrotary_close( lua_State* L )
{
  DATA *d = (DATA *)luaL_checkudata(L, 1, "rotary.switch");

  if (d->handle) {
    callback_free(L, d, ROTARY_ALL);

    if (!rotary_has_queued_event(d->handle)) {
      update_self_ref(L, d, 1);
    }

    if (rotary_close( d->handle )) {
      return luaL_error( L, "Unable to close switch." );
    }

    d->handle = NULL;
  }

  if (d->timer_handle) {
    esp_timer_stop(d->timer_handle);
    esp_timer_delete(d->timer_handle);
    d->timer_handle = NULL;
  } 

  return 0;
}

// Lua: on( mask[, cb] )
static int lrotary_on( lua_State* L )
{
  DATA *d = (DATA *)luaL_checkudata(L, 1, "rotary.switch");

  int mask = luaL_checkinteger(L, 2);

  if (lua_gettop(L) >= 3) {
    if (callback_set(L, d, mask, 3)) {
      return luaL_error( L, "Unable to set callback." );
    }
  } else {
    callback_free(L, d, mask);
  }

  update_self_ref(L, d, 1);

  return 0;
}

// Lua: getpos( ) -> pos, PRESS/RELEASE
static int lrotary_getpos( lua_State* L )
{
  DATA *d = (DATA *)luaL_checkudata(L, 1, "rotary.switch");
 
  int pos = rotary_getpos(d->handle);

  if (pos == -1) {
    return 0;
  }

  lua_pushinteger(L, (pos << 1) >> 1);
  lua_pushboolean(L, (pos & 0x80000000));

  return 2;
}

// Returns TRUE if there maybe/is more stuff to do
static bool lrotary_dequeue_single(lua_State* L, DATA *d)
{
  bool something_pending = false;

  if (d) {
    rotary_event_t result;

    if (rotary_getevent(d->handle, &result)) {
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

      rotary_event_handled(d->handle);
      something_pending = rotary_has_queued_event(d->handle);
    }

    lrotary_check_timer(d, esp_timer_get_time(), 1);
  }

  return something_pending;
}

static void lrotary_timer_done(void *param)
{
  DATA *d = (DATA *) param;

  d->timer_running = 0;

  lrotary_check_timer(d, esp_timer_get_time(), 1);
}

static void lrotary_check_timer(DATA *d, uint32_t time_us, bool dotimer)
{
  uint32_t delay = time_us - d->last_event_time;
  if (d->timer_running) {
    esp_timer_stop(d->timer_handle);
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
    esp_timer_start_once(d->timer_handle, timeout + 1);
  }
}

static void lrotary_task(task_param_t param, task_prio_t prio)
{
  (void) prio;

  bool need_to_post = false;
  lua_State *L = lua_getstate();

  DATA *d = (DATA *) param;
  if (d) {
    if (lrotary_dequeue_single(L, d)) {
      need_to_post = true;
    }
  }

  if (need_to_post) {
    // If there is pending stuff, queue another task
    task_post_medium(tasknumber, param);
  } else if (d) {
    update_self_ref(L, d, -1);
  }
}


// Module function map
LROT_BEGIN(rotary, NULL, 0)
  LROT_FUNCENTRY( setup, lrotary_setup )
  LROT_NUMENTRY( TURN, MASK(TURN) )
  LROT_NUMENTRY( PRESS, MASK(PRESS) )
  LROT_NUMENTRY( RELEASE, MASK(RELEASE) )
  LROT_NUMENTRY( LONGPRESS, MASK(LONGPRESS) )
  LROT_NUMENTRY( CLICK, MASK(CLICK) )
  LROT_NUMENTRY( DBLCLICK, MASK(DBLCLICK) )
  LROT_NUMENTRY( ALL, ROTARY_ALL )
LROT_END(rotary, NULL, 0)

// Module function map
LROT_BEGIN(rotary_switch, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY(__gc, lrotary_close)
  LROT_TABENTRY(__index, rotary_switch)
  LROT_FUNCENTRY(on, lrotary_on)
  LROT_FUNCENTRY(close, lrotary_close)
  LROT_FUNCENTRY(getpos, lrotary_getpos)
LROT_END(rotary_switch, NULL, LROT_MASK_GC_INDEX)

static int rotary_open(lua_State *L) {
  luaL_rometatable(L, "rotary.switch",
                    LROT_TABLEREF(rotary_switch));  // create metatable
  tasknumber = task_get_id(lrotary_task);
  return 0;
}

NODEMCU_MODULE(ROTARY, "rotary", rotary, rotary_open);
