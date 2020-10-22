// Module for reading keycards via Wiegand protocol

// ## Contributors
// [Cody Cutrer](https://github.com/ccutrer) adapted to being a NodeMCU module

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "task/task.h"
#include "user_interface.h"
#include "pm/swtimer.h"

#ifdef LUA_USE_MODULES_WIEGAND
#if !defined(GPIO_INTERRUPT_ENABLE) || !defined(GPIO_INTERRUPT_HOOK_ENABLE)
#error Must have GPIO_INTERRUPT and GPIO_INTERRUPT_HOOK if using WIEGAND module
#endif
#endif

typedef struct {
  uint32_t current_card;
  int bit_count;
  uint32_t last_card;
  uint32_t last_bit_count;
  int cb_ref;
  int self_ref;
  ETSTimer timer;
  int timer_running;
  int task_posted;
  int pinD0;
  int pinD1;
  uint32_t last_bit_time;
} wiegand_struct_t;
typedef wiegand_struct_t* wiegand_t;

static int tasknumber;
static volatile wiegand_t pins_to_wiegand_state[NUM_GPIO];

static wiegand_t wiegand_get( lua_State *L, int stack)
{
  wiegand_t w = (wiegand_t)luaL_checkudata(L, stack, "wiegand.wiegand");
  if (w == NULL)
    return (wiegand_t)luaL_error(L, "wiegand object expected");
  return w;
}

static uint32_t ICACHE_RAM_ATTR wiegand_intr(uint32_t ret_gpio_status)
{
  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  uint32_t gpio_bits = gpio_status;
  for(int i = 0; gpio_bits > 0; ++i, gpio_bits >>= 1) {
    if (i == NUM_GPIO)
      break;
    if ((gpio_bits & 1) == 0)
      continue;
    // find the struct registered for this pin
    volatile wiegand_t w = pins_to_wiegand_state[i];
    if (!w) {
      continue;
    }

    ++w->bit_count;
    w->current_card <<= 1;
    if (i == pin_num[w->pinD1])
      w->current_card |= 1;

    w->last_bit_time = system_get_time();

    if (!w->task_posted) {
      task_post_medium(tasknumber, (os_param_t)w);
      w->task_posted = 1;
    }

    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & (1 << i));
    ret_gpio_status &= ~(1 << i);
  }

  return ret_gpio_status;
}

static int parity(int val)
{
  int parity = 0;
  while (val > 0) {
    parity ^= val & 1;
    val >>= 1;
  }
  return parity;
}

static bool wiegand_store_card(volatile wiegand_t w)
{
  uint32_t card = w->current_card;
  int bit_count = w->bit_count;
  w->current_card = 0;
  w->bit_count = 0;

  switch(bit_count) {
    case 4:
      w->last_card = card;
      w->last_bit_count = bit_count;
      return true;
    case 26:
      // even parity over the first 13 bits, odd parity over the last 13 bits
      if (parity((card & 0x3ffe000) >> 13) != 0 || parity(card & 0x1fff) != 1)
        return false;

      w->last_card = (card >> 1) & 0xffffff;
      w->last_bit_count = bit_count;
      return true;
  }
  return false;
}

static void lwiegand_timer_done(void *param)
{
  lua_State *L = lua_getstate();

  wiegand_t w = (wiegand_t) param;

  os_timer_disarm(&w->timer);

  if (wiegand_store_card(w)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, w->cb_ref);

    lua_pushinteger(L, w->last_card);
    lua_pushinteger(L, w->last_bit_count);

    lua_call(L, 2, 0);
  }
}

static void lwiegand_cb(os_param_t param, uint8_t prio)
{
  wiegand_t w = (wiegand_t) param;
  (void) prio;

  *(volatile int *)&w->task_posted = 0;
  if (w->timer_running)
    os_timer_disarm(&w->timer);

  int timeout = 25 - (system_get_time() - w->last_bit_time) / 1000;
  if (timeout < 0) {
    lwiegand_timer_done(w);
  } else {
    os_timer_arm(&w->timer, timeout, 0);
  }
}

static void reregister_gpio_hooks()
{
  uint32_t mask = 0;
  for (int i = 0; i < NUM_GPIO; ++i) {
    if (pins_to_wiegand_state[i])
      mask |= (1 << i);
  }
  platform_gpio_register_intr_hook(mask, wiegand_intr);
}

static int lwiegand_close( lua_State* L)
{
  wiegand_t w = wiegand_get(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, w->cb_ref);
  w->cb_ref = LUA_NOREF;
  if (w->timer_running) {
    os_timer_disarm(&w->timer);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, w->self_ref);
  w->self_ref = LUA_NOREF;

  pins_to_wiegand_state[pin_num[w->pinD0]] = NULL;
  pins_to_wiegand_state[pin_num[w->pinD1]] = NULL;

  reregister_gpio_hooks();
  platform_gpio_intr_init(w->pinD0, GPIO_PIN_INTR_DISABLE);
  platform_gpio_intr_init(w->pinD1, GPIO_PIN_INTR_DISABLE);

  return 0;
}

// Lua: wiegand.created0pin, d1pin)
static int lwiegand_create(lua_State* L)
{
  unsigned pinD0 = luaL_checkinteger(L, 1);
  unsigned pinD1 = luaL_checkinteger(L, 2);
  luaL_argcheck(L, platform_gpio_exists(pinD0) && pinD0>0, 1, "Invalid pin for D0");
  luaL_argcheck(L, platform_gpio_exists(pinD1) && pinD1>0 && pinD0 != pinD1, 2, "Invalid pin for D1");
  luaL_checktype(L, 3, LUA_TFUNCTION);

  if (pins_to_wiegand_state[pin_num[pinD0]] || pins_to_wiegand_state[pin_num[pinD1]])
    return luaL_error(L, "pin already in use");

  wiegand_t ud = (wiegand_t)lua_newuserdata(L, sizeof(wiegand_struct_t));
  if (!ud) return luaL_error(L, "not enough memory");
  luaL_getmetatable(L, "wiegand.wiegand");
	lua_setmetatable(L, -2);

  ud->current_card = 0;
  ud->bit_count = 0;
  ud->timer_running = 0;
  ud->task_posted = 0;
  ud->pinD0 = pinD0;
  ud->pinD1 = pinD1;

  platform_gpio_mode( pinD0, PLATFORM_GPIO_INT, PLATFORM_GPIO_FLOAT);
  platform_gpio_mode( pinD1, PLATFORM_GPIO_INT, PLATFORM_GPIO_FLOAT);

  lua_pushvalue(L, 3);
  ud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushvalue(L, -1);
  ud->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  os_timer_setfn(&ud->timer, lwiegand_timer_done, ud);
  SWTIMER_REG_CB(lwiegand_timer_done, SWTIMER_RESUME);

  pins_to_wiegand_state[pin_num[pinD0]] = ud;
  pins_to_wiegand_state[pin_num[pinD1]] = ud;

  reregister_gpio_hooks();
  platform_gpio_intr_init(pinD0, GPIO_PIN_INTR_NEGEDGE);
  platform_gpio_intr_init(pinD1, GPIO_PIN_INTR_NEGEDGE);

  return 1;
}

// Module function map
LROT_BEGIN(wiegand_dyn, NULL, LROT_MASK_GC_INDEX)
  LROT_FUNCENTRY( __gc, lwiegand_close )
  LROT_TABENTRY( __index, wiegand_dyn )
  LROT_FUNCENTRY( close, lwiegand_close )
LROT_END(wiegand_dyn, NULL, LROT_MASK_GC_INDEX)

LROT_BEGIN(wiegand, NULL, 0)
  LROT_FUNCENTRY( create, lwiegand_create )
LROT_END (wiegand, NULL, 0)

int luaopen_wiegand( lua_State *L ) {
  luaL_rometatable(L, "wiegand.wiegand", LROT_TABLEREF(wiegand_dyn));
  tasknumber = task_get_id(lwiegand_cb);
  memset((void *)pins_to_wiegand_state, 0, sizeof(pins_to_wiegand_state));

  return 0;
}

NODEMCU_MODULE(WIEGAND, "wiegand", wiegand, luaopen_wiegand);
