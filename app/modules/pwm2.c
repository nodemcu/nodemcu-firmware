// Module for interfacing with PWM

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "driver/pwm2.h"
#include "pin_map.h"

#define LPWM2_PIN_IS_NOT_USED -1
#define LPWM2_PIN_IS_USED -2

#define LPWM2_OK -1

static struct
{
  sint8_t pinChannelMap[GPIO_PIN_NUM];
  uint32_t initialDuty[GPIO_PIN_NUM];
  uint32_t period;
  uint32 io_info[GPIO_PIN_NUM][3];
} lpwm2_context;

static int lpwm2_arg_pin(lua_State *L, const char *msg, const uint8_t argPos, uint8_t *pin)
{
  LUA_INTEGER val = luaL_optinteger(L, argPos, 0);
  if (val == 0)
    return luaL_error(L, "pwm2.%s : pin number expected", msg);
  if (val < 1 || val >= GPIO_PIN_NUM)
    return luaL_error(L, "pwm2.%s : invalid pin=%d, expects 0<pin<%d", val, GPIO_PIN_NUM);
  *pin = val;
  return LPWM2_OK;
}

static int lpwm2_arg_duty(lua_State *L, const char *msg, const uint8_t argPos, uint32_t *duty)
{
  LUA_INTEGER val = luaL_optinteger(L, argPos, -1);
  if (val <= -1)
    return luaL_error(L, "pwm2.%s : duty expected", msg);
  if (val > lpwm2_context.period)
    return luaL_error(L, "pwm2.%s : duty=%d cannot exceed period=%d", lpwm2_context.period);
  *duty = val;
  return LPWM2_OK;
}

static int lpwm2_get_channel(lua_State *L, const char *msg, const uint8_t pin, uint8_t *channel)
{
  *channel = lpwm2_context.pinChannelMap[pin];
  if (*channel < 0)
    return luaL_error(L, "pwm2.%s : pin %d is not setup", msg, pin);
  return LPWM2_OK;
}

// Lua: setup_period( period )
static int lpwm2_setup_period(lua_State *L)
{
  unsigned period = luaL_checkinteger(L, 1);
  if (period < 0 || period > 5000000)
    return luaL_error(L, "pwm2.setup_period : expects 0<period<5*10^6 but found %d", period);
  lpwm2_context.period = period;
  return 0;
}

// Lua: setup_pin( pin, duty )
static int lpwm2_setup_pin(lua_State *L)
{
  const char *msg = "setup_pin";
  int err;
  uint8_t pin;
  err = lpwm2_arg_pin(L, msg, 1, &pin);
  if (err != LPWM2_OK)
    return err;
  uint32_t duty;
  err = lpwm2_arg_duty(L, msg, 2, &duty);
  if (err != LPWM2_OK)
    return err;
  lpwm2_context.pinChannelMap[pin] = LPWM2_PIN_IS_USED;
  lpwm2_context.initialDuty[pin] = duty;
  return 0;
}

// Lua: setup_done()
static int lpwm2_setup_done(lua_State *L)
{
  uint32_t duties[GPIO_PIN_NUM];
  uint32_t channelsCnt = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (lpwm2_context.pinChannelMap[i] == LPWM2_PIN_IS_USED)
    {
      lpwm2_context.pinChannelMap[i] = channelsCnt;
      duties[channelsCnt] = lpwm2_context.initialDuty[i];
      lpwm2_context.io_info[channelsCnt][0] = pin_mux[i];
      lpwm2_context.io_info[channelsCnt][1] = pin_func[i];
      lpwm2_context.io_info[channelsCnt][2] = pin_num[i];
      channelsCnt++;
    }
  }
  if (channelsCnt < 1)
    return luaL_error(L, "pwm2.setup_done : setup some pins before completing setup");
  pwm2_init(lpwm2_context.period, duties, channelsCnt, lpwm2_context.io_info);
  return 0;
}

// Lua: start()
static int lpwm2_start(lua_State *L)
{
  pwm2_start();
  return 0;
}

// Lua: set_duty( pin, duty )
static int lpwm2_set_duty(lua_State *L)
{
  const char *msg = "set_duty";
  int err;
  uint8_t pin;
  err = lpwm2_arg_pin(L, msg, 1, &pin);
  if (err != LPWM2_OK)
    return err;
  uint8_t channel;
  err = lpwm2_get_channel(L, msg, pin, &channel);
  if (err != LPWM2_OK)
    return err;
  uint32_t duty;
  err = lpwm2_arg_duty(L, msg, 2, &duty);
  if (err != LPWM2_OK)
    return err;
  pwm2_set_duty(duty, channel);
  return 0;
}

// Lua: duty = get_duty( pin )
static int lpwm2_get_duty(lua_State *L)
{
  const char *msg = "get_duty";
  int err;
  uint8_t pin;
  err = lpwm2_arg_pin(L, msg, 1, &pin);
  if (err != LPWM2_OK)
    return err;
  uint8_t channel;
  err = lpwm2_get_channel(L, msg, pin, &channel);
  if (err != LPWM2_OK)
    return err;
  return pwm2_get_duty(channel);
}

int lpwm2_open(lua_State *L)
{
  for (int i = 0; i < GPIO_PIN_NUM; i++)
  {
    lpwm2_context.pinChannelMap[i] = LPWM2_PIN_IS_NOT_USED;
    lpwm2_context.initialDuty[i] = 0;
  }
  return 0;
}

// Module function map
static const LUA_REG_TYPE pwm2_map[] = {
    {LSTRKEY("setup_period"), LFUNCVAL(lpwm2_setup_period)},
    {LSTRKEY("setup_pin"), LFUNCVAL(lpwm2_setup_pin)},
    {LSTRKEY("setup_done"), LFUNCVAL(lpwm2_setup_done)},
    {LSTRKEY("start"), LFUNCVAL(lpwm2_start)},
    {LSTRKEY("set_duty"), LFUNCVAL(lpwm2_set_duty)},
    {LSTRKEY("get_duty"), LFUNCVAL(lpwm2_get_duty)},
    {LNILKEY, LNILVAL}};

NODEMCU_MODULE(PWM2, "pwm2", pwm2_map, lpwm2_open);
