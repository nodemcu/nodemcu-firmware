//#define NODE_DEBUG

#include <stdint.h>
#include <string.h>

#include "module.h"
#include "lauxlib.h"
#include "task/task.h"

#include "gpio.h"
#include "hw_timer.h"
#include "lmem.h"
#include "platform.h"
#include "user_interface.h"

/* Must be a power of 2 (max 128 without changing types) */
#define RAWBUF_SIZE 128
#define RAWBUF_MASK (RAWBUF_SIZE - 1)

#define CB_RAW  0
#define CB_RC5  1

static task_handle_t tasknumber;
static ETSTimer timer;

static struct ir_device {
  int cb[2];
  uint32_t last_time;
  int16_t rawbuf[RAWBUF_SIZE];
  uint16_t rc5_data;
  uint8_t rc5_state;
  uint8_t rawbuf_read;
  uint8_t rawbuf_write;
  uint8_t pin, pin_num;
} *the_ir_device;

#define abs(x) ((x > 0) ? (x) : -(x))

static uint32_t ICACHE_RAM_ATTR lir_interrupt(uint32_t gpio_status)
{
  struct ir_device *dev = the_ir_device;
  if (!dev)
    return gpio_status;
  uint32_t bits = GPIO_REG_READ(GPIO_IN_ADDRESS);
  uint32_t now = system_get_time();
  uint32_t pin = dev->pin_num;

  /* Ack the interrupt */
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin));

  int32_t duration = now - dev->last_time;
  if (duration > 0x7fff || duration < 0)
    duration = 0x7fff;

  /* Sign of duration is used to encode HIGH/LOW */
  if (bits & BIT(pin))
    duration = -duration;

  dev->last_time = now;

  uint8_t pos = dev->rawbuf_write;
  uint8_t max = dev->rawbuf_read;

  if (max <= pos)
    max += RAWBUF_SIZE;
  max--;
  if (pos < max) {
    dev->rawbuf[pos] = duration;
    dev->rawbuf_write = (pos + 1) & RAWBUF_MASK;
  }

  task_post_low(tasknumber, 0);

  return gpio_status & ~BIT(pin);
}

// Lua: setup([pin])
static int lir_setup( lua_State *L )
{
  if (the_ir_device)
    return luaL_error(L, "No support for multiple IR devices");

  if (lua_gettop(L) == 0)
    return 0;

  int pin  = luaL_checkinteger( L, 1 );
  luaL_argcheck(L, platform_gpio_exists(pin), 1, "Invalid pin");

  struct ir_device *dev = lua_newuserdata(L, sizeof(*dev));
  memset(dev, 0, sizeof(*dev));
  dev->cb[CB_RAW] = LUA_NOREF;
  dev->cb[CB_RC5] = LUA_NOREF;
  luaL_getmetatable(L, "ir.device");
  lua_setmetatable(L, -2);

  the_ir_device = dev;

  dev->pin = pin;
  dev->pin_num = pin_num[pin];

  uint32_t bits = (1 << dev->pin_num);

  platform_gpio_mode(dev->pin, PLATFORM_GPIO_INT, PLATFORM_GPIO_FLOAT);
  gpio_pin_intr_state_set(GPIO_ID_PIN(dev->pin_num), GPIO_PIN_INTR_ANYEDGE);
  platform_gpio_register_intr_hook(bits, lir_interrupt);

  return 1;
}

// Lua: on(event[, cb])
static int lir_on( lua_State *L )
{
  struct ir_device *dev = luaL_checkudata(L, 1, "ir.device");
  luaL_argcheck(L, dev, 1, "ir.device expected");
  static const char * const opts[] = {"raw", "rc5"};
  int type = luaL_checkoption(L, 2, NULL, opts);

  if (dev->cb[type] != LUA_NOREF) {
    luaL_unref(L, LUA_REGISTRYINDEX, dev->cb[type]);
    dev->cb[type] = LUA_NOREF;
  }

  if (lua_gettop(L) < 3)
    return 0;

  luaL_argcheck(L, lua_type(L, 3) == LUA_TFUNCTION ||
      lua_type(L, 3) == LUA_TLIGHTFUNCTION, 3, "Invalid callback");

  lua_pushvalue(L, 3);
  dev->cb[type] = luaL_ref(L, LUA_REGISTRYINDEX);

  return 0;
}

static void rc5_cb(struct ir_device *dev)
{
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, dev->cb[CB_RC5]);
  lua_newtable(L);
  lua_pushnumber(L, dev->rc5_data);
  lua_setfield(L, -2, "code");
  lua_pushboolean(L, (dev->rc5_data >> 11) & 1);
  lua_setfield(L, -2, "toggle");
  lua_pushnumber(L, (dev->rc5_data >> 6) & 0x1f);
  lua_setfield(L, -2, "device");
  lua_pushnumber(L, dev->rc5_data & 0x3f);
  lua_setfield(L, -2, "command");
  lua_call(L, 1, 0);
}

#define RC5_STATE_END    0x80
#define RC5_STATE_START  0x81
#define RC5_STATE_ERROR  0x82
#define RC5_SECOND 0x10

static void rc5_break(struct ir_device *dev)
{
  dev->rc5_state = RC5_STATE_START;
  dev->rc5_data = 0;
}

static void rc5_data(struct ir_device *dev, int bit)
{
  NODE_DBG("rc5_state=0x%x bit=%d rc5_data=0x%x\n",
      dev->rc5_state, bit, dev->rc5_data);

  if (dev->rc5_state == RC5_STATE_START) {
    if (bit)
      dev->rc5_state = 12;
    else
      dev->rc5_state = RC5_STATE_END;
    return;
  } else if (dev->rc5_state & RC5_STATE_END) {
    return;
  }

  int offset = (dev->rc5_state & 0xf);

  if (!(dev->rc5_state & RC5_SECOND)) {
    if (!bit)
      dev->rc5_data |= 1 << offset;
    if (!offset)
      rc5_cb(dev);
    dev->rc5_state |= RC5_SECOND;
  } else {
    int old_bit = (dev->rc5_data >> offset) & 1;
    if (old_bit ^ bit)
      dev->rc5_state = RC5_STATE_ERROR;
    else if (offset > 0)
      dev->rc5_state = offset - 1;
    else
      dev->rc5_state = RC5_STATE_END;
  }
}

static void lir_task(os_param_t param, uint8_t prio)
{
  struct ir_device *dev = the_ir_device;
  if (!dev)
    return;
  uint8_t pos = dev->rawbuf_read;
  uint8_t max = dev->rawbuf_write;
  if (max < pos)
    max += RAWBUF_SIZE;

  if (dev->cb[CB_RC5] != LUA_NOREF) {
    uint8_t cb5_pos = pos;
    for(; cb5_pos < max; cb5_pos++) {
      int16_t v = dev->rawbuf[cb5_pos & RAWBUF_MASK];
      NODE_DBG("cb5_pos=%u length:%d\n",
          cb5_pos, v);
      if (abs(v) > 2000) {
        rc5_break(dev);
      } else if (abs(v) > 1000) {
        rc5_data(dev, v < 0);
        rc5_data(dev, v < 0);
      } else {
        rc5_data(dev, v < 0);
      }
    }
  }
  if (dev->cb[CB_RAW] != LUA_NOREF) {
    lua_State *L = lua_getstate();
    lua_rawgeti(L, LUA_REGISTRYINDEX, dev->cb[CB_RAW]);

    lua_newtable(L);
    for(int i = 1; pos < max; pos++, i++) {
      int16_t v = dev->rawbuf[pos & RAWBUF_MASK];
      lua_pushnumber(L, v);
      lua_rawseti(L, -2, i);
    }

    lua_call(L, 1, 0);
  }
  dev->rawbuf_read = max & RAWBUF_MASK;
}

static int lir_gc(lua_State *L)
{
  struct ir_device *dev = luaL_checkudata(L, 1, "ir.device");
  luaL_argcheck(L, dev, 1, "ir.device expected");

  platform_gpio_unregister_intr_hook(lir_interrupt);
  gpio_pin_intr_state_set(GPIO_ID_PIN(dev->pin_num), GPIO_PIN_INTR_DISABLE);
  platform_gpio_mode(dev->pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);

  the_ir_device = NULL;
}

LROT_BEGIN( ir_device, NULL, LROT_MASK_GC_INDEX )
  LROT_FUNCENTRY( __gc, lir_gc )
  LROT_TABENTRY( __index, ir_device )
  LROT_FUNCENTRY( on, lir_on )
LROT_END( ir_device, NULL, LROT_MASK_GC_INDEX )

static int ir_open( lua_State *L )
{
  tasknumber = task_get_id( lir_task );

  luaL_rometatable(L, "ir.device", LROT_TABLEREF(ir_device));

  return 0;
}

LROT_BEGIN( ir, NULL, 0 )
  LROT_FUNCENTRY( setup, lir_setup )
LROT_END( ir, NULL, 0 )

NODEMCU_MODULE(IR, "ir", ir, ir_open);
