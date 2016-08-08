/*
 * Module for interfacing with Switec instrument steppers (and
 * similar devices). These are the steppers that are used in automotive 
 * instrument panels and the like. Run off 5 volts at low current.
 *
 * Code inspired by:
 *
 * SwitecX25 Arduino Library
 *  Guy Carpenter, Clearwater Software - 2012
 *
 *  Licensed under the BSD2 license, see license.txt for details.
 *
 * NodeMcu integration by Philip Gladstone, N1DQ
 */

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "task/task.h"
#include "driver/switec.h"

// This is the reference to the callbacks for when the pointer
// stops moving.
static int stopped_callback[SWITEC_CHANNEL_COUNT] = { LUA_NOREF, LUA_NOREF, LUA_NOREF };
static task_handle_t tasknumber;

static void callback_free(lua_State* L, unsigned int id) 
{
  luaL_unref(L, LUA_REGISTRYINDEX, stopped_callback[id]);
  stopped_callback[id] = LUA_NOREF;
}

static void callback_set(lua_State* L, unsigned int id, int argNumber) 
{
  if (lua_type(L, argNumber) == LUA_TFUNCTION || lua_type(L, argNumber) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, argNumber);  // copy argument (func) to the top of stack
    callback_free(L, id);
    stopped_callback[id] = luaL_ref(L, LUA_REGISTRYINDEX);
  }
}

static void callback_execute(lua_State* L, unsigned int id) 
{
  if (stopped_callback[id] != LUA_NOREF) {
    int callback = stopped_callback[id];
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
    callback_free(L, id);

    lua_call(L, 0, 0);
  }
}

int platform_switec_exists( unsigned int id )
{
  return (id < SWITEC_CHANNEL_COUNT);
}

// Lua: setup(id, P1, P2, P3, P4, maxSpeed)
static int lswitec_setup( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( switec, id );
  int pin[4];

  if (switec_close(id)) {
    return luaL_error( L, "Unable to setup stepper." );
  }

  int i;
  for (i = 0; i < 4; i++) {
    uint32_t gpio = luaL_checkinteger(L, 2 + i);

    luaL_argcheck(L, platform_gpio_exists(gpio), 2 + i, "Invalid pin");

    pin[i] = pin_num[gpio];

    platform_gpio_mode(gpio, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
  }

  int deg_per_sec = 0;
  if (lua_gettop(L) >= 6) {
    deg_per_sec = luaL_checkinteger(L, 6);
  }

  if (switec_setup(id, pin, deg_per_sec, tasknumber)) {
    return luaL_error(L, "Unable to setup stepper.");
  }
  return 0;  
}

// Lua: close( id )
static int lswitec_close( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( switec, id );
  callback_free(L, id);
  if (switec_close( id )) {
    return luaL_error( L, "Unable to close stepper." );
  }
  return 0;  
}

// Lua: reset( id )
static int lswitec_reset( lua_State* L )
{
  unsigned int id;
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( switec, id );
  if (switec_reset( id )) {
    return luaL_error( L, "Unable to reset stepper." );
  }
  return 0;  
}

// Lua: moveto( id, pos [, cb] )
static int lswitec_moveto( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( switec, id );
  int pos;
  pos = luaL_checkinteger( L, 2 );

  if (lua_gettop(L) >= 3) {
    callback_set(L, id, 3);
  } else {
    callback_free(L, id);
  }

  if (switec_moveto( id, pos )) {
    return luaL_error( L, "Unable to move stepper." );
  }
  return 0;
}

// Lua: getpos( id ) -> position, moving
static int lswitec_getpos( lua_State* L )
{
  unsigned int id;
  
  id = luaL_checkinteger( L, 1 );
  MOD_CHECK_ID( switec, id );
  int32_t pos;
  int32_t dir;
  int32_t target;
  if (switec_getpos( id, &pos, &dir, &target )) {
    return luaL_error( L, "Unable to get position." );
  }
  lua_pushnumber(L, pos);
  lua_pushnumber(L, dir);
  return 2;
}

static int lswitec_dequeue(lua_State* L)
{
  int id;

  for (id = 0; id < SWITEC_CHANNEL_COUNT; id++) {
    if (stopped_callback[id] != LUA_NOREF) {
      int32_t pos;
      int32_t dir;
      int32_t target;
      if (!switec_getpos( id, &pos, &dir, &target )) {
	if (dir == 0 && pos == target) {
	  callback_execute(L, id);
	}
      }
    }
  }

  return 0;
}

static void lswitec_task(os_param_t param, uint8_t prio) 
{
  (void) param;
  (void) prio;

  lswitec_dequeue(lua_getstate());
}

static int switec_open(lua_State *L)
{
  (void) L;

  tasknumber = task_get_id(lswitec_task);

  return 0;
}


// Module function map
static const LUA_REG_TYPE switec_map[] = {
  { LSTRKEY( "setup" ),    LFUNCVAL( lswitec_setup ) },
  { LSTRKEY( "close" ),    LFUNCVAL( lswitec_close ) },
  { LSTRKEY( "reset" ),    LFUNCVAL( lswitec_reset ) },
  { LSTRKEY( "moveto" ),   LFUNCVAL( lswitec_moveto) },
  { LSTRKEY( "getpos" ),   LFUNCVAL( lswitec_getpos) },
#ifdef SQITEC_DEBUG
  { LSTRKEY( "dequeue" ),  LFUNCVAL( lswitec_dequeue) },
#endif

  { LNILKEY, LNILVAL }
};

NODEMCU_MODULE(SWITEC, "switec", switec_map, switec_open);
