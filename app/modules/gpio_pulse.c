#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "user_interface.h"
#include "c_types.h"
#include "c_string.h"
#include "gpio.h"
#include "hw_timer.h"
#include "pin_map.h"

#define TIMER_OWNER 'P'

typedef struct {
  uint32_t gpio_set;
  uint32_t gpio_clr;
  uint32_t delay;
  uint32_t count;
  uint32_t count_left;
  uint16_t loop;
  uint8_t adjustable;
} pulse_entry_t;

typedef struct {
  uint32_t entry_count;
  volatile uint16_t entry_pos;
  volatile int16_t stop_pos;  // -1 is stop nowhere, -2 is stop everywhere, otherwise is stop point
  pulse_entry_t *entry;
  volatile uint32_t expected_end_time;
  volatile int32_t next_adjust;
  volatile int cb_ref;
} pulse_t;

static int active_pulser_ref;
static pulse_t *active_pulser;
static task_handle_t tasknumber;

static int gpio_pulse_push_state(lua_State *L, pulse_t *pulser) {
  uint32_t now;
  uint32_t expected_end_time;
  uint32_t entry_pos;
  do {
    now = 0x7FFFFFFF & system_get_time();
    expected_end_time = pulser->expected_end_time;
    entry_pos = pulser->entry_pos;
  } while (expected_end_time != pulser->expected_end_time ||
           entry_pos != pulser->entry_pos);

  if (entry_pos >= pulser->entry_count) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, entry_pos + 1);    // Lua is 1 offset
  }
  int32_t diff = (expected_end_time - now) & 0x7fffffff;
  lua_pushinteger(L, (diff << 1) >> 1);
  lua_pushinteger(L, now);
  return 3;
}

static int gpio_pulse_getstate(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (pulser != active_pulser) {
    return 0;
  }

  return gpio_pulse_push_state(L, pulser);
}

static int gpio_pulse_stop(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (pulser != active_pulser) {
    return 0;
  }

  int argno = 2;

  int32_t stop_pos = -2;

  if (lua_type(L, argno) == LUA_TNUMBER) {
    stop_pos = luaL_checkinteger(L, 2);  
    if (stop_pos != -2) {
      if (stop_pos < 1 || stop_pos > pulser->entry_count) {
        return luaL_error( L, "bad stop position: %d", stop_pos );
      }
      stop_pos = stop_pos - 1;
    }
    argno++;
  }

  if (lua_type(L, argno) == LUA_TFUNCTION || lua_type(L, argno) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, argno);
  } else {
    return luaL_error( L, "missing callback" );
  }

  int new_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  int cb_ref = pulser->cb_ref;

  pulser->cb_ref = LUA_NOREF;
  pulser->stop_pos = -1;
  pulser->cb_ref = new_cb_ref;
  pulser->stop_pos = stop_pos;

  luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);

  lua_pushboolean(L, 1);
  return 1;
}

static int gpio_pulse_delete(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (pulser == active_pulser) {
    return 0;
  }

  luaL_unref(L, LUA_REGISTRYINDEX, pulser->cb_ref);
  return 0;
}

static int gpio_pulse_build(lua_State *L) {
  // Take a table argument
  luaL_checktype(L, 1, LUA_TTABLE);

  // First figure out how big we need the block to be
  size_t size = luaL_getn(L, 1);

  if (size > 100) {
    return luaL_error(L, "table is too large");
  }

  size_t memsize = sizeof(pulse_t) + size * sizeof(pulse_entry_t);
  pulse_t *pulser = (pulse_t *) lua_newuserdata(L, memsize);
  memset(pulser, 0, memsize);
  //
  // Associate its metatable
  luaL_getmetatable(L, "gpio.pulse");
  lua_setmetatable(L, -2);

  pulser->entry = (pulse_entry_t *) (pulser + 1);
  pulser->entry_count = size;

  size_t i;
  for (i = 0; i < size; i++) {
    pulse_entry_t *entry = pulser->entry + i;

    lua_rawgeti(L, 1, i + 1);

    if (lua_type(L, -1) != LUA_TTABLE) {
      return luaL_error(L, "All entries must be tables");
    }

    lua_pushnil(L);             // key position
    while (lua_next(L, -2)) {
      // stack now contains: -1 => value; -2 => key; -3 => table
      if (lua_type(L, -2) == LUA_TNUMBER) {
        int pin = lua_tonumber(L, -2);
        int value = lua_tonumber(L, -1);

        if (pin < 0 || pin >= GPIO_PIN_NUM) {
          luaL_error(L, "pin number %d must be in range", pin);
        }

        if (value) {
          entry->gpio_set |= BIT(pin_num[pin]);
        } else {
          entry->gpio_clr |= BIT(pin_num[pin]);
        }
      } else {
        const char *str = lua_tostring(L, -2);

        if (strcmp(str, "delay") == 0) {
          entry->delay = lua_tonumber(L, -1);
        } else if (strcmp(str, "adjustable") == 0) {
          entry->delay = lua_tonumber(L, -1);
          entry->adjustable = 1;
        } else if (strcmp(str, "count") == 0) {
          entry->count = lua_tonumber(L, -1);
        } else if (strcmp(str, "loop") == 0) {
          entry->loop = lua_tonumber(L, -1);
        } else {
          return luaL_error(L, "Unrecognized key found: %s", str);
        }
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }

  return 1;
}

static int gpio_pulse_adjust(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser != pulser) {
    return 0;
  }

  int offset = luaL_checkinteger(L, 2);
  // This will alter the next adjustable
  pulser->next_adjust += offset;

  gpio_pulse_push_state(L, active_pulser);

  return 3;
}

static int gpio_pulse_cancel(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser != pulser) {
    return 0;
  }

  // Shut off the timer
  platform_hw_timer_close(TIMER_OWNER);

  gpio_pulse_push_state(L, active_pulser);

  active_pulser = NULL;

  int pulser_ref = active_pulser_ref;
  active_pulser_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, pulser_ref);

  return 3;
}

static void ICACHE_RAM_ATTR gpio_pulse_timeout(os_param_t p) {
  (void) p;

  int delay;

  do {
    if (!active_pulser || active_pulser->entry_pos >= active_pulser->entry_count) {
      platform_hw_timer_close(TIMER_OWNER);
      task_post_low(tasknumber, (task_param_t)0);
      return;
    }
    pulse_entry_t *entry = active_pulser->entry + active_pulser->entry_pos;

    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, entry->gpio_set);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, entry->gpio_clr);

    int16_t stop = active_pulser->stop_pos;
    if (stop == -2 || stop == active_pulser->entry_pos) {
      platform_hw_timer_close(TIMER_OWNER);
      task_post_low(tasknumber, (task_param_t)0);
      return;
    }

    if (entry->loop) {
      if (entry->count_left == 0) {
        entry->count_left = entry->count + 1;
      }
      if (--entry->count_left >= 1) {
        active_pulser->entry_pos = entry->loop - 1;       // zero offset
      } else {
        active_pulser->entry_pos++;
      }
    } else {
      active_pulser->entry_pos++;
    }

    delay = entry->delay;

    if (entry->adjustable) {
      int offset = active_pulser->next_adjust;
      active_pulser->next_adjust = 0;
      int delay_offset = 0x7fffffff & (system_get_time() - (active_pulser->expected_end_time + offset));
      delay -= (delay_offset << 1) >> 1;
    }

    active_pulser->expected_end_time += delay;
  } while (delay < 3);
  
  platform_hw_timer_arm_us(TIMER_OWNER, delay);
}

static int gpio_pulse_start(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser) {
    return luaL_error(L, "pulse operation already in progress");
  }

  if (lua_type(L, 2) == LUA_TFUNCTION || lua_type(L, 2) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, 2);
  } else {
    return luaL_error( L, "missing callback" );
  }

  luaL_unref(L, LUA_REGISTRYINDEX, pulser->cb_ref);
  pulser->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  active_pulser = pulser;
  lua_pushvalue(L, 1);
  active_pulser_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  size_t i;
  for (i = 0; i < pulser->entry_count; i++) {
    pulser->entry[i].count_left = 0;
  }
  pulser->entry_pos = 0;
  pulser->stop_pos = -1;

  // Now start things up
  if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, TRUE)) {
    // Failed to init the timer
    luaL_error(L, "Unable to initialize timer");
  }

  active_pulser->expected_end_time = 0x7fffffff & system_get_time();
  platform_hw_timer_set_func(TIMER_OWNER, gpio_pulse_timeout, 0);
  gpio_pulse_timeout(0);

  return 0;
}

static void gpio_pulse_task(os_param_t param, uint8_t prio) 
{
  (void) param;
  (void) prio;

  if (active_pulser) {
    lua_State *L = lua_getstate();
    // Invoke the callback
    lua_rawgeti(L, LUA_REGISTRYINDEX, active_pulser->cb_ref);

    gpio_pulse_push_state(L, active_pulser);

    active_pulser = NULL;
    int pulser_ref = active_pulser_ref;
    active_pulser_ref = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, pulser_ref);

    lua_call(L, 3, 0);
  }
}

static const LUA_REG_TYPE pulse_map[] = {
  { LSTRKEY( "getstate" ),            LFUNCVAL( gpio_pulse_getstate ) },
  { LSTRKEY( "stop" ),                LFUNCVAL( gpio_pulse_stop ) },
  { LSTRKEY( "cancel" ),              LFUNCVAL( gpio_pulse_cancel ) },
  { LSTRKEY( "start" ),               LFUNCVAL( gpio_pulse_start ) },
  { LSTRKEY( "adjust" ),              LFUNCVAL( gpio_pulse_adjust ) },
  { LSTRKEY( "__gc" ),                LFUNCVAL( gpio_pulse_delete ) },
  { LSTRKEY( "__index" ),             LROVAL( pulse_map ) },
  { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE gpio_pulse_map[] =
{
  { LSTRKEY( "build" ),               LFUNCVAL( gpio_pulse_build ) },
  { LNILKEY, LNILVAL }
};

int gpio_pulse_init(lua_State *L)
{
  luaL_rometatable(L, "gpio.pulse", (void *)pulse_map);
  tasknumber = task_get_id(gpio_pulse_task);
  return 0;
}

//NODEMCU_MODULE(GPIOPULSE, "gpiopulse", gpio_pulse_map, gpio_pulse_init);

