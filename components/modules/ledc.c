// Module for working with the ledc driver

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "driver/ledc.h"

#include "common.h"

typedef struct {
  int timer;
  int channel;
  int mode;
} ledc_channel;

static int lledc_new_channel( lua_State *L )
{
  const int top = lua_gettop(L);
  luaL_checktable (L, 1);

  /* Setup timer */
  ledc_timer_config_t ledc_timer;

  ledc_timer.duty_resolution = opt_checkint_range (L, "bits", LEDC_TIMER_13_BIT, 0, LEDC_TIMER_BIT_MAX-1);

  ledc_timer.freq_hz = opt_checkint_range(L, "frequency", -1, 1, 40000000UL);

  ledc_timer.speed_mode = opt_checkint_range(L, "mode", -1, 0, LEDC_SPEED_MODE_MAX-1);

  ledc_timer.timer_num = opt_checkint_range(L, "timer", -1, 0, LEDC_TIMER_MAX-1);

  ledc_timer.clk_cfg = LEDC_AUTO_CLK;

  /* Setup channel */
  ledc_channel_config_t channel_config = {
    .speed_mode = ledc_timer.speed_mode,
    .timer_sel = ledc_timer.timer_num,
    .intr_type = LEDC_INTR_DISABLE
  };

  channel_config.channel = opt_checkint_range(L, "channel", -1, 0, LEDC_CHANNEL_MAX-1);

  channel_config.duty = opt_checkint_range(L, "duty", -1, 0, 1<<(LEDC_TIMER_BIT_MAX-1));

  channel_config.gpio_num = opt_checkint_range(L, "gpio", -1, 0, GPIO_NUM_MAX-1);

  channel_config.flags.output_invert = opt_checkbool(L, "invert", 0);

  lua_settop(L, top);

  esp_err_t timerErr = ledc_timer_config(&ledc_timer);
  if(timerErr != ESP_OK)
    return luaL_error (L, "timer configuration failed code %d", timerErr);

  esp_err_t channelErr = ledc_channel_config(&channel_config);
  if(channelErr != ESP_OK)
    return luaL_error (L, "channel configuration failed code %d", channelErr);

  ledc_channel * channel = (ledc_channel*)lua_newuserdata(L, sizeof(ledc_channel));
  luaL_getmetatable(L, "ledc.channel");
  lua_setmetatable(L, -2);

  channel->mode = ledc_timer.speed_mode;
  channel->channel = channel_config.channel;
  channel->timer = ledc_timer.timer_num;

  return 1;
}

static int lledc_stop( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int idleLevel = luaL_checkint (L, 2);
  luaL_argcheck(L, idleLevel >= 0 && idleLevel <= 1, 1, "Invalid idle level");

  esp_err_t err = ledc_stop(channel->mode, channel->channel, idleLevel);
  if(err != ESP_OK)
    return luaL_error (L, "stop failed, code %d", err);

  return 1;
}

static int lledc_set_freq( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int frequency = luaL_checkint (L, 2);

  esp_err_t err = ledc_set_freq(channel->mode, channel->timer, frequency);
  if(err != ESP_OK)
    return luaL_error (L, "set freq failed, code %d", err);

  return 1;
}

static int lledc_get_freq( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int frequency = ledc_get_freq(channel->mode, channel->timer);
  lua_pushinteger (L, frequency);

  return 1;
}

static int lledc_set_duty( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");
  int duty = luaL_checkint (L, 2);

  esp_err_t dutyErr = ledc_set_duty(channel->mode, channel->channel, duty);
  if(dutyErr != ESP_OK)
    return luaL_error (L, "set duty failed, code %d", dutyErr);

  esp_err_t updateErr = ledc_update_duty(channel->mode, channel->channel);
  if(updateErr != ESP_OK)
    return luaL_error (L, "update duty failed, code %d", updateErr);

  return 1;
}

static int lledc_get_duty( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int duty = ledc_get_duty(channel->mode, channel->channel);
  lua_pushinteger (L, duty);

  return 1;
}

static int lledc_timer_rst( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  esp_err_t err = ledc_timer_rst(channel->mode, channel->timer);
  if(err != ESP_OK)
    return luaL_error (L, "reset failed, code %d", err);

  return 1;
}

static int lledc_timer_pause( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  esp_err_t err = ledc_timer_pause(channel->mode, channel->timer);
  if(err != ESP_OK)
    return luaL_error (L, "pause failed, code %d", err);

  return 1;
}

static int lledc_timer_resume( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  esp_err_t err = ledc_timer_resume(channel->mode, channel->timer);
  if(err != ESP_OK)
    return luaL_error (L, "resume failed, code %d", err);

  return 1;
}

static int lledc_set_fade_with_time( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int stack = 1;

  int targetDuty = luaL_checkint (L, ++stack);
  int maxFadeTime = luaL_checkint (L, ++stack);

  int wait = luaL_optint (L, ++stack, LEDC_FADE_NO_WAIT);
  luaL_argcheck(L, wait == LEDC_FADE_NO_WAIT || wait == LEDC_FADE_WAIT_DONE, stack, "Invalid wait");

  ledc_fade_func_install(0);

  esp_err_t fadeErr = ledc_set_fade_with_time(channel->mode, channel->channel, targetDuty, maxFadeTime);
  if(fadeErr != ESP_OK)
    return luaL_error (L, "set fade failed, code %d", fadeErr);

  esp_err_t startErr = ledc_fade_start(channel->mode, channel->channel, wait);
  if(startErr != ESP_OK)
    return luaL_error (L, "start fade failed, code %d", startErr);

  return 1;
}

static int lledc_set_fade_with_step( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int stack = 1;

  int targetDuty = luaL_checkint (L, ++stack);
  int scale = luaL_checkint (L, ++stack);
  int cycleNum = luaL_checkint (L, ++stack);

  int wait = luaL_optint (L, ++stack, LEDC_FADE_NO_WAIT);
  luaL_argcheck(L, wait == LEDC_FADE_NO_WAIT || wait == LEDC_FADE_WAIT_DONE, stack, "Invalid wait");

  ledc_fade_func_install(0);

  esp_err_t fadeErr = ledc_set_fade_with_step(channel->mode, channel->channel, targetDuty, scale, cycleNum);
  if(fadeErr != ESP_OK)
    return luaL_error (L, "set fade failed, code %d", fadeErr);

  esp_err_t startErr = ledc_fade_start(channel->mode, channel->channel, wait);
  if(startErr != ESP_OK)
    return luaL_error (L, "start fade failed, code %d", startErr);

  return 1;
}

static int lledc_set_fade( lua_State *L ) {
  ledc_channel * channel = (ledc_channel*)luaL_checkudata(L, 1, "ledc.channel");

  int stack = 1;

  int duty = luaL_checkint (L, ++stack);
  int direction = luaL_checkint (L, ++stack);
  luaL_argcheck(L, direction == LEDC_DUTY_DIR_DECREASE || direction == LEDC_DUTY_DIR_INCREASE, stack, "Invalid direction");

  int scale = luaL_checkint (L, ++stack);
  int cycleNum = luaL_checkint (L, ++stack);
  int stepNum = luaL_checkint (L, ++stack);;

  int wait = luaL_optint (L, ++stack, LEDC_FADE_NO_WAIT);
  luaL_argcheck(L, wait == LEDC_FADE_NO_WAIT || wait == LEDC_FADE_WAIT_DONE, stack, "Invalid wait");

  ledc_fade_func_install(0);

  esp_err_t fadeErr = ledc_set_fade(channel->mode, channel->channel, duty, direction, stepNum, cycleNum, scale);
  if(fadeErr != ESP_OK)
    return luaL_error (L, "set fade failed, code %d", fadeErr);

  esp_err_t startErr = ledc_fade_start(channel->mode, channel->channel, wait);
  if(startErr != ESP_OK)
    return luaL_error (L, "start fade failed, code %d", startErr);

  return 1;
}

// Module function map
LROT_BEGIN(ledc_channel, NULL, LROT_MASK_INDEX)
  LROT_TABENTRY ( __index,         ledc_channel )
  LROT_FUNCENTRY( getduty,         lledc_get_duty )
  LROT_FUNCENTRY( setduty,         lledc_set_duty )
  LROT_FUNCENTRY( getfreq,         lledc_get_freq )
  LROT_FUNCENTRY( setfreq,         lledc_set_freq )

  LROT_FUNCENTRY( stop,            lledc_stop )
  LROT_FUNCENTRY( reset,           lledc_timer_rst )
  LROT_FUNCENTRY( pause,           lledc_timer_pause )
  LROT_FUNCENTRY( resume,          lledc_timer_resume )

  LROT_FUNCENTRY( fadewithtime,    lledc_set_fade_with_time )
  LROT_FUNCENTRY( fadewithstep,    lledc_set_fade_with_step )
  LROT_FUNCENTRY( fade,            lledc_set_fade )
LROT_END(ledc_channel, NULL, LROT_MASK_INDEX)

LROT_BEGIN(ledc, NULL, 0)
  LROT_FUNCENTRY( newChannel,      lledc_new_channel )

#if SOC_LEDC_SUPPORT_HS_MODE
  LROT_NUMENTRY ( HIGH_SPEED,      LEDC_HIGH_SPEED_MODE )
#endif
  LROT_NUMENTRY ( LOW_SPEED,       LEDC_LOW_SPEED_MODE )

  LROT_NUMENTRY ( TIMER_0,         LEDC_TIMER_0 )
  LROT_NUMENTRY ( TIMER_1,         LEDC_TIMER_1 )
  LROT_NUMENTRY ( TIMER_2,         LEDC_TIMER_2 )
  LROT_NUMENTRY ( TIMER_10_BIT,    LEDC_TIMER_10_BIT )
  LROT_NUMENTRY ( TIMER_11_BIT,    LEDC_TIMER_11_BIT )
  LROT_NUMENTRY ( TIMER_12_BIT,    LEDC_TIMER_12_BIT )
  LROT_NUMENTRY ( TIMER_13_BIT,    LEDC_TIMER_13_BIT )
  LROT_NUMENTRY ( TIMER_14_BIT,    LEDC_TIMER_14_BIT )
#if SOC_LEDC_TIMER_BIT_WIDE_NUM > 14
  LROT_NUMENTRY ( TIMER_15_BIT,    LEDC_TIMER_15_BIT )
  LROT_NUMENTRY ( TIMER_16_BIT,    LEDC_TIMER_16_BIT )
  LROT_NUMENTRY ( TIMER_17_BIT,    LEDC_TIMER_17_BIT )
  LROT_NUMENTRY ( TIMER_18_BIT,    LEDC_TIMER_18_BIT )
  LROT_NUMENTRY ( TIMER_19_BIT,    LEDC_TIMER_19_BIT )
  LROT_NUMENTRY ( TIMER_20_BIT,    LEDC_TIMER_20_BIT )
#endif

  LROT_NUMENTRY ( CHANNEL_0,       LEDC_CHANNEL_0 )
  LROT_NUMENTRY ( CHANNEL_1,       LEDC_CHANNEL_1 )
  LROT_NUMENTRY ( CHANNEL_2,       LEDC_CHANNEL_2 )
  LROT_NUMENTRY ( CHANNEL_3,       LEDC_CHANNEL_3 )
  LROT_NUMENTRY ( CHANNEL_4,       LEDC_CHANNEL_4 )
  LROT_NUMENTRY ( CHANNEL_5,       LEDC_CHANNEL_5 )
#if SOC_LED_CHANNEL_NUM > 6
  LROT_NUMENTRY ( CHANNEL_6,       LEDC_CHANNEL_6 )
  LROT_NUMENTRY ( CHANNEL_7,       LEDC_CHANNEL_7 )
#endif

  LROT_NUMENTRY ( IDLE_LOW,        0 )
  LROT_NUMENTRY ( IDLE_HIGH,       1 )

  LROT_NUMENTRY ( FADE_NO_WAIT,    LEDC_FADE_NO_WAIT )
  LROT_NUMENTRY ( FADE_WAIT_DONE,  LEDC_FADE_WAIT_DONE )
  LROT_NUMENTRY ( FADE_DECREASE,   LEDC_DUTY_DIR_DECREASE )
  LROT_NUMENTRY ( FADE_INCREASE,   LEDC_DUTY_DIR_INCREASE )
LROT_END(ledc, NULL, 0)

int luaopen_ledc(lua_State *L) {
  luaL_rometatable(L, "ledc.channel", LROT_TABLEREF(ledc_channel));  // create metatable for ledc.channel
  return 0;
}

NODEMCU_MODULE(LEDC, "ledc", ledc, luaopen_ledc);
