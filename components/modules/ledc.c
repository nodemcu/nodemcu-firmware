// Module for working with the ledc driver

#include "module.h"
#include "lauxlib.h"
#include "platform.h"

#include "driver/ledc.h"

static int lledc_setup( lua_State *L )
{
  int t=1;
  luaL_checkanytable (L, t);

  /* Setup timer */
  ledc_timer_config_t ledc_timer = {
      .bit_num = LEDC_TIMER_13_BIT
  };

  lua_getfield(L, t, "frequency");
  ledc_timer.freq_hz = luaL_checkinteger(L, -1);

  lua_getfield(L, t, "mode");
  ledc_timer.speed_mode = luaL_checkinteger(L, -1);

  lua_getfield(L, t, "timer");
  ledc_timer.timer_num = luaL_checkinteger(L, -1);

  esp_err_t timerErr = ledc_timer_config(&ledc_timer);
  if(timerErr != ESP_OK)
    return luaL_error (L, "timer configuration failed code %d", timerErr);

  /* Setup channel */
  ledc_channel_config_t channel_config = {
    .speed_mode = ledc_timer.speed_mode,
    .timer_sel = ledc_timer.timer_num
  };

  lua_getfield(L, t, "channel");
  channel_config.channel = luaL_checkinteger(L, -1);

  lua_getfield(L, t, "duty");
  channel_config.duty = luaL_checkinteger(L, -1);

  lua_getfield(L, t, "pin");
  channel_config.gpio_num = luaL_checkinteger(L, -1);

  lua_getfield(L, t, "interupt");
  channel_config.intr_type = luaL_optint (L, -1, 0) > 0 ? LEDC_INTR_FADE_END : LEDC_INTR_DISABLE;

  esp_err_t channelErr = ledc_channel_config(&channel_config);
  if(channelErr != ESP_OK)
    return luaL_error (L, "channel configuration failed code %d", channelErr);

  return 1;
}

static int lledc_stop( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int channel = luaL_checkint (L, 2);
  int idleLevel = luaL_checkint (L, 3);

  esp_err_t err = ledc_stop(speedMode, channel, idleLevel);
  if(err != ESP_OK)
    return luaL_error (L, "stop failed, code %d", err);

  return 1;
}

static int lledc_set_freq( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int timer = luaL_checkint (L, 2);
  int frequency = luaL_checkint (L, 3);

  esp_err_t err = ledc_set_freq(speedMode, timer, frequency);
  if(err != ESP_OK)
    return luaL_error (L, "set freq failed, code %d", err);

  return 1;
}

static int lledc_get_freq( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int timer = luaL_checkint (L, 2);

  int frequency = ledc_get_freq(speedMode, timer);
  lua_pushinteger (L, frequency);

  return 1;
}

static int lledc_set_duty( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int channel = luaL_checkint (L, 2);
  int duty = luaL_checkint (L, 3);

  esp_err_t dutyErr = ledc_set_duty(speedMode, channel, duty);
  if(dutyErr != ESP_OK)
    return luaL_error (L, "set duty failed, code %d", dutyErr);

  esp_err_t updateErr = ledc_update_duty(speedMode, channel);
  if(updateErr != ESP_OK)
    return luaL_error (L, "update duty failed, code %d", updateErr);

  return 1;
}

// int ledc_get_duty(ledc_mode_t speed_mode, ledc_channel_t channel)
static int lledc_get_duty( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int channel = luaL_checkint (L, 2);

  int duty = ledc_get_duty(speedMode, channel);
  lua_pushinteger (L, duty);

  return 1;
}

// esp_err_t ledc_timer_rst(ledc_mode_t speed_mode, uint32_t timer_sel)
static int lledc_timer_rst( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int timer = luaL_checkint (L, 2);

  esp_err_t err = ledc_timer_rst(speedMode, timer);
  if(err != ESP_OK)
    return luaL_error (L, "reset failed, code %d", err);

  return 1;
}

// esp_err_t ledc_timer_pause(ledc_mode_t speed_mode, uint32_t timer_sel)
static int lledc_timer_pause( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int timer = luaL_checkint (L, 2);

  esp_err_t err = ledc_timer_rst(speedMode, timer);
  if(err != ESP_OK)
    return luaL_error (L, "pause failed, code %d", err);

  return 1;
}

// esp_err_t ledc_timer_resume(ledc_mode_t speed_mode, uint32_t timer_sel)
static int lledc_timer_resume( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int timer = luaL_checkint (L, 2);

  esp_err_t err = ledc_timer_resume(speedMode, timer);
  if(err != ESP_OK)
    return luaL_error (L, "resume failed, code %d", err);

  return 1;
}

static int lledc_set_fade_with_time( lua_State *L ) {
  int speedMode = luaL_checkint (L, 1);
  int channel = luaL_checkint (L, 2);
  int targetDuty = luaL_checkint (L, 3);
  int maxFadeTime = luaL_checkint (L, 4);
  int wait = luaL_optint (L, 5, LEDC_FADE_NO_WAIT);

  esp_err_t fadeErr = ledc_set_fade_with_time(speedMode, channel, targetDuty, maxFadeTime);
  if(fadeErr != ESP_OK)
    return luaL_error (L, "set fade failed, code %d", fadeErr);

  esp_err_t startErr = ledc_fade_start(speedMode, channel, wait);
  if(startErr != ESP_OK)
    return luaL_error (L, "start fade failed, code %d", startErr);

  return 1;
}

// Module function map
static const LUA_REG_TYPE ledc_map[] =
{
  { LSTRKEY( "setup" ),           LFUNCVAL( lledc_setup ) },

  { LSTRKEY( "getduty" ),         LFUNCVAL( lledc_get_duty ) },
  { LSTRKEY( "setduty" ),         LFUNCVAL( lledc_set_duty ) },
  { LSTRKEY( "getfreq" ),         LFUNCVAL( lledc_get_freq ) },
  { LSTRKEY( "setfreq" ),         LFUNCVAL( lledc_set_freq ) },

  { LSTRKEY( "stop" ),            LFUNCVAL( lledc_stop ) },
  { LSTRKEY( "reset" ),           LFUNCVAL( lledc_timer_rst ) },
  { LSTRKEY( "pause" ),           LFUNCVAL( lledc_timer_pause ) },
  { LSTRKEY( "resume" ),          LFUNCVAL( lledc_timer_resume ) },

  { LSTRKEY( "fadewithtime" ),    LFUNCVAL( lledc_set_fade_with_time ) },

  { LSTRKEY( "HIGH_SPEED"),       LNUMVAL( LEDC_HIGH_SPEED_MODE ) },
  { LSTRKEY( "LOW_SPEED"),        LNUMVAL( LEDC_LOW_SPEED_MODE ) },
  { LSTRKEY( "TIMER0"),           LNUMVAL( LEDC_TIMER_0 ) },
  { LSTRKEY( "TIMER1"),           LNUMVAL( LEDC_TIMER_1 ) },
  { LSTRKEY( "TIMER2"),           LNUMVAL( LEDC_TIMER_2 ) },
  { LSTRKEY( "CHANNEL0"),         LNUMVAL( LEDC_CHANNEL_0 ) },
  { LSTRKEY( "CHANNEL1"),         LNUMVAL( LEDC_CHANNEL_1 ) },
  { LSTRKEY( "CHANNEL2"),         LNUMVAL( LEDC_CHANNEL_2 ) },
  { LSTRKEY( "CHANNEL3"),         LNUMVAL( LEDC_CHANNEL_3 ) },
  { LSTRKEY( "CHANNEL4"),         LNUMVAL( LEDC_CHANNEL_4 ) },
  { LSTRKEY( "CHANNEL5"),         LNUMVAL( LEDC_CHANNEL_5 ) },
  { LSTRKEY( "CHANNEL6"),         LNUMVAL( LEDC_CHANNEL_6 ) },
  { LSTRKEY( "CHANNEL7"),         LNUMVAL( LEDC_CHANNEL_7 ) },

  { LSTRKEY( "NO_WAIT"),          LNUMVAL( LEDC_FADE_NO_WAIT ) },
  { LSTRKEY( "WAIT_DONE"),        LNUMVAL( LEDC_FADE_WAIT_DONE ) }
};

NODEMCU_MODULE(LEDC, "ledc", ledc_map, NULL);
