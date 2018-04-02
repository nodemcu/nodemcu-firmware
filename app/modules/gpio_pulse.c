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
#include "driver/gpio16.h"

#define TIMER_OWNER 'P'

#define xstr(s) str(s)
#define str(s) #s

// Maximum delay in microseconds
#define DELAY_LIMIT 64000000

typedef struct {
  uint32_t gpio_set;
  uint32_t gpio_clr;
  int32_t delay;
  int32_t delay_min;
  int32_t delay_max;
  uint32_t count;
  uint32_t count_left;
  uint16_t loop;
} pulse_entry_t;

typedef struct {
  uint32_t entry_count;
  volatile uint32_t steps;
  volatile uint16_t entry_pos;
  volatile uint16_t active_pos;
  volatile int16_t stop_pos;  // -1 is stop nowhere, -2 is stop everywhere, otherwise is stop point
  pulse_entry_t *entry;
  volatile uint32_t expected_end_time;
  volatile uint32_t desired_end_time;
  volatile int32_t next_adjust;
  volatile int32_t pending_delay;
  volatile int cb_ref;
} pulse_t;

static int active_pulser_ref;
static pulse_t *active_pulser;
static task_handle_t tasknumber;

static int gpio_pulse_push_state(lua_State *L, pulse_t *pulser) {
  uint32_t now;
  uint32_t expected_end_time;
  uint32_t active_pos;
  uint32_t steps;
  do {
    now = 0x7FFFFFFF & system_get_time();
    expected_end_time = pulser->expected_end_time;
    active_pos = pulser->active_pos;
    steps = pulser->steps;
  } while (expected_end_time != pulser->expected_end_time ||
           active_pos != pulser->active_pos ||
           steps != pulser->steps);

  if (active_pos >= pulser->entry_count) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, active_pos + 1);    // Lua is 1 offset
  }
  lua_pushinteger(L, steps);

  int32_t diff = (expected_end_time - now) & 0x7fffffff;
  lua_pushinteger(L, (diff << 1) >> 1);
  lua_pushinteger(L, now);
  return 4;
}

static int gpio_pulse_getstate(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

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
        return luaL_error( L, "bad stop position: %d (valid range 1 - %d)", stop_pos, pulser->entry_count );
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

static void fill_entry_from_table(lua_State *L, pulse_entry_t *entry) {
  if (lua_type(L, -1) != LUA_TTABLE) {
    luaL_error(L, "All entries must be tables");
  }

  lua_pushnil(L);             // key position
  while (lua_next(L, -2)) {
    // stack now contains: -1 => value; -2 => key; -3 => table
    if (lua_type(L, -2) == LUA_TNUMBER) {
      int pin = luaL_checkint(L, -2);
      int value = luaL_checkint(L, -1);

      if (pin < 0 || pin >= GPIO_PIN_NUM) {
        luaL_error(L, "pin number %d must be in range 0 .. %d", pin, GPIO_PIN_NUM - 1);
      }

      if (value) {
        entry->gpio_set |= BIT(pin_num[pin]);
      } else {
        entry->gpio_clr |= BIT(pin_num[pin]);
      }
    } else {
      const char *str = luaL_checkstring(L, -2);

      if (strcmp(str, "delay") == 0) {
        entry->delay = luaL_checkint(L, -1);
        if (entry->delay < 0 || entry->delay > DELAY_LIMIT) {
          luaL_error(L, "delay of %d must be in the range 0 .. " xstr(DELAY_LIMIT) " microseconds", entry->delay);
        }
      } else if (strcmp(str, "min") == 0) {
        entry->delay_min = luaL_checkint(L, -1);
        if (entry->delay_min < 0 || entry->delay_min > DELAY_LIMIT) {
          luaL_error(L, "delay minimum of %d must be in the range 0 .. " xstr(DELAY_LIMIT) " microseconds", entry->delay_min);
        }
      } else if (strcmp(str, "max") == 0) {
        entry->delay_max = luaL_checkint(L, -1);
        if (entry->delay_max < 0 || entry->delay_max > DELAY_LIMIT) {
          luaL_error(L, "delay maximum of %d must be in the range 0 .. " xstr(DELAY_LIMIT) " microseconds", entry->delay_max);
        }
      } else if (strcmp(str, "count") == 0) {
        entry->count = luaL_checkint(L, -1);
      } else if (strcmp(str, "loop") == 0) {
        entry->loop = luaL_checkint(L, -1);
      } else {
        luaL_error(L, "Unrecognized key found: %s", str);
      }
    }
    lua_pop(L, 1);
  }

  if (entry->delay_min != -1 || entry->delay_max != -1) {
    if (entry->delay_min == -1) {
      entry->delay_min = 0;
    }
    if (entry->delay_min > entry->delay ||
        entry->delay_max < entry->delay) {
      luaL_error(L, "Delay of %d must be between min and max", entry->delay);
    }
  }

  lua_pop(L, 1);
}

static int gpio_pulse_build(lua_State *L) {
  // Take a table argument
  luaL_checktype(L, 1, LUA_TTABLE);

  // First figure out how big we need the block to be
  size_t size = luaL_getn(L, 1);

  if (size > 100) {
    return luaL_error(L, "table is too large: %d entries is more than 100", size);
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

    entry->delay_min = -1;
    entry->delay_max = -1;

    lua_rawgeti(L, 1, i + 1);

    fill_entry_from_table(L, entry);
  }

  return 1;
}

static int gpio_pulse_update(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");
  int entry_pos = luaL_checkinteger(L, 2);  

  if (entry_pos < 1 || entry_pos > pulser->entry_count) {
    return luaL_error(L, "entry number must be in range 1 .. %d", pulser->entry_count);
  }

  pulse_entry_t *entry = pulser->entry + entry_pos - 1;

  pulse_entry_t new_entry = *entry;

  lua_pushvalue(L, 3);

  fill_entry_from_table(L, &new_entry);

  // Now do the update
  ETS_FRC1_INTR_DISABLE();
  *entry = new_entry;
  ETS_FRC1_INTR_ENABLE();

  return 0;
}

static int gpio_pulse_adjust(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser != pulser) {
    return 0;
  }

  int offset = luaL_checkinteger(L, 2);
  // This will alter the next adjustable
  pulser->next_adjust = offset;

  int rc = gpio_pulse_push_state(L, active_pulser);

  return rc;
}

static int gpio_pulse_cancel(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser != pulser) {
    return 0;
  }

  // Shut off the timer
  platform_hw_timer_close(TIMER_OWNER);

  int rc = gpio_pulse_push_state(L, active_pulser);

  active_pulser = NULL;

  int pulser_ref = active_pulser_ref;
  active_pulser_ref = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, pulser_ref);

  return rc;
}

static void ICACHE_RAM_ATTR gpio_pulse_timeout(os_param_t p) {
  (void) p;

  uint32_t now = system_get_time();

  int delay;

  if (active_pulser) {
    delay = active_pulser->pending_delay;
    if (delay > 0) {
      if (delay > 1200000) {
        delay = 1000000;
      }
      active_pulser->pending_delay -= delay;
      platform_hw_timer_arm_us(TIMER_OWNER, delay);
      return;
    }
  }

  do {
    active_pulser->active_pos = active_pulser->entry_pos;

    if (!active_pulser || active_pulser->entry_pos >= active_pulser->entry_count) {
      if (active_pulser) {
        active_pulser->steps++;
      }
      platform_hw_timer_close(TIMER_OWNER);
      task_post_low(tasknumber, (task_param_t)0);
      return;
    }
    active_pulser->steps++;

    pulse_entry_t *entry = active_pulser->entry + active_pulser->entry_pos;

    // Yes, this means that there is more skew on D0 than on other pins....
    if (entry->gpio_set & 0x10000) {
      gpio16_output_set(1);
    }
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, entry->gpio_set);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, entry->gpio_clr);
    if (entry->gpio_clr & 0x10000) {
      gpio16_output_set(0);
    }

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

    int delay_offset = 0;

    if (entry->delay_min != -1) {
      int offset = active_pulser->next_adjust;
      active_pulser->next_adjust = 0;
      delay_offset = ((0x7fffffff & (now - active_pulser->desired_end_time)) << 1) >> 1;
      delay -= delay_offset;
      delay += offset;
      //dbg_printf("%d(et %d diff %d): Delay was %d us, offset = %d, delay_offset = %d, new delay = %d, range=%d..%d\n", 
      //           now, active_pulser->desired_end_time, now - active_pulser->desired_end_time, 
      //           entry->delay, offset, delay_offset, delay, entry->delay_min, entry->delay_max);
      if (delay < entry->delay_min) {
        // we can't delay as little as 'delay', so we need to adjust
        // the next period as well.
        active_pulser->next_adjust = (entry->delay - entry->delay_min) + offset;
        delay = entry->delay_min;
      } else if (delay > entry->delay_max) {
        // we can't delay as much as 'delay', so we need to adjust
        // the next period as well.
        active_pulser->next_adjust = (entry->delay - entry->delay_max) + offset;
        delay = entry->delay_max;
      }
    }

    active_pulser->desired_end_time += delay + delay_offset;
    active_pulser->expected_end_time = system_get_time() + delay;
  } while (delay < 3);
  
  if (delay > 1200000) {
    active_pulser->pending_delay = delay - 1000000;
    delay = 1000000;
  }
  platform_hw_timer_arm_us(TIMER_OWNER, delay);
}

static int gpio_pulse_start(lua_State *L) {
  pulse_t *pulser = luaL_checkudata(L, 1, "gpio.pulse");

  if (active_pulser) {
    return luaL_error(L, "pulse operation already in progress");
  }

  int argno = 2;
  int initial_adjust;

  if (lua_type(L, argno) == LUA_TNUMBER) {
    initial_adjust = luaL_checkinteger(L, argno);
    argno++;
  }

  if (lua_type(L, argno) == LUA_TFUNCTION || lua_type(L, argno) == LUA_TLIGHTFUNCTION) {
    lua_pushvalue(L, argno);
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
  pulser->active_pos = 0;
  pulser->steps = 0;
  pulser->stop_pos = -1;
  pulser->next_adjust = initial_adjust;

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

    int rc = gpio_pulse_push_state(L, active_pulser);

    active_pulser = NULL;
    int pulser_ref = active_pulser_ref;
    active_pulser_ref = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, pulser_ref);

    lua_call(L, rc, 0);
  }
}

static const LUA_REG_TYPE pulse_map[] = {
  { LSTRKEY( "getstate" ),            LFUNCVAL( gpio_pulse_getstate ) },
  { LSTRKEY( "stop" ),                LFUNCVAL( gpio_pulse_stop ) },
  { LSTRKEY( "cancel" ),              LFUNCVAL( gpio_pulse_cancel ) },
  { LSTRKEY( "start" ),               LFUNCVAL( gpio_pulse_start ) },
  { LSTRKEY( "adjust" ),              LFUNCVAL( gpio_pulse_adjust ) },
  { LSTRKEY( "update" ),              LFUNCVAL( gpio_pulse_update ) },
  { LSTRKEY( "__gc" ),                LFUNCVAL( gpio_pulse_delete ) },
  { LSTRKEY( "__index" ),             LROVAL( pulse_map ) },
  { LNILKEY, LNILVAL }
};

const LUA_REG_TYPE gpio_pulse_map[] =
{
  { LSTRKEY( "build" ),               LFUNCVAL( gpio_pulse_build ) },
  { LSTRKEY( "__index" ),             LROVAL( gpio_pulse_map ) },
  { LNILKEY, LNILVAL }
};

int gpio_pulse_init(lua_State *L)
{
  luaL_rometatable(L, "gpio.pulse", (void *)pulse_map);
  tasknumber = task_get_id(gpio_pulse_task);
  return 0;
}

//NODEMCU_MODULE(GPIOPULSE, "gpiopulse", gpio_pulse_map, gpio_pulse_init);

