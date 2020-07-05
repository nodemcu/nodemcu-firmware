/*
 * Software PWM using soft-interrupt timer1.
 * Supports higher frequencies compared to Espressif provided one.
 *
 * Nikolay Fiykov
 */

// Module for interfacing with PWM2 driver

#include <stdint.h>
#include "lauxlib.h"
#include "module.h"
#include "driver/pwm2.h"

#define luaL_argcheck2(L, cond, numarg, extramsg) \
  if (!(cond)) return luaL_argerror(L, (numarg), (extramsg))

//############################
// lua bindings

static int lpwm2_open(lua_State *L) {
  pwm2_init();
  return 0;
}

static int lpwm2_get_timer_data(lua_State *L) {
  lua_pushboolean(L, pwm2_get_module_data()->setupData.isStarted);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.interruptTimerCPUTicks);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.interruptTimerTicks);
  return 3;
}

static int lpwm2_get_pin_data(lua_State *L) {
  const uint8 pin = luaL_checkinteger(L, 1);
  luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, 1, "invalid pin number");
  lua_pushinteger(L, pwm2_is_pin_setup(pin));
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].duty);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].pulseResolutions);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].divisableFrequency);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].frequencyDivisor);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].resolutionCPUTicks);
  lua_pushinteger(L, pwm2_get_module_data()->setupData.pin[pin].resolutionInterruptCounterMultiplier);
  return 7;
}

static int lpwm2_setup_pin_common(lua_State *L, const bool isFreqHz) {
  if (pwm2_is_started()) {
    return luaL_error(L, "pwm2 : already started, stop it before setting up pins.");
  }
  const int pin = lua_tointeger(L, 1);
  const int freq = lua_tointeger(L, 2);
  const int resolution = lua_tointeger(L, 3);
  const int initDuty = lua_tointeger(L, 4);
  const int freqFractions = luaL_optinteger(L, 5, 1);

  luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, 1, "invalid pin number");
  luaL_argcheck2(L, freq > 0, 2, "invalid frequency");
  luaL_argcheck2(L, resolution > 0, 3, "invalid frequency resolution");
  luaL_argcheck2(L, initDuty >= 0 && initDuty <= resolution, 4, "invalid duty");
  luaL_argcheck2(L, freqFractions > 0, 5, "invalid frequency fractions");

  if (isFreqHz) {
    pwm2_setup_pin(pin, freqFractions, freq, resolution, initDuty);
  } else {
    pwm2_setup_pin(pin, freq, freqFractions, resolution, initDuty);
  }
  return 0;
}

static int lpwm2_setup_pin_hz(lua_State *L) {
  return lpwm2_setup_pin_common(L, true);
}

static int lpwm2_setup_pin_sec(lua_State *L) {
  return lpwm2_setup_pin_common(L, false);
}

static int lpwm2_set_duty(lua_State *L) {
  int pos = 0;
  while (true) {
    int pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1) {
      break;
    }
    luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, pos, "invalid pin number");

    int duty = luaL_optinteger(L, ++pos, -1);
    luaL_argcheck2(L, duty >= 0 && duty <= pwm2_get_module_data()->setupData.pin[pin].pulseResolutions, pos, "invalid duty");

    if (!pwm2_is_pin_setup(pin)) {
      return luaL_error(L, "pwm2 : pin=%d is not setup yet", pin);
    }
    pwm2_set_duty(pin, duty);
  }
  return 0;
}

static int lpwm2_release_pin(lua_State *L) {
  if (pwm2_is_started()) {
    return luaL_error(L, "pwm2 : pwm is started, stop it first.");
  }
  int pos = 0;
  while (true) {
    int pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1) {
      break;
    }
    luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, pos, "invalid pin number");
    pwm2_release_pin(2);
  }
  return 0;
}

static int lpwm2_stop(lua_State *L) {
  pwm2_stop();
  return 0;
}

static int lpwm2_start(lua_State *L) {
  if (!pwm2_start()) {
    luaL_error(L, "pwm2: currently platform timer1 is being used by another module.\n");
    lua_pushboolean(L, false);
  } else {
    lua_pushboolean(L, true);
  }
  return 1;
}

// Module function map
LROT_BEGIN(pwm2, NULL, 0)
LROT_FUNCENTRY(setup_pin_hz, lpwm2_setup_pin_hz)
LROT_FUNCENTRY(setup_pin_sec, lpwm2_setup_pin_sec)
LROT_FUNCENTRY(release_pin, lpwm2_release_pin)
LROT_FUNCENTRY(start, lpwm2_start)
LROT_FUNCENTRY(stop, lpwm2_stop)
LROT_FUNCENTRY(set_duty, lpwm2_set_duty)
LROT_FUNCENTRY(get_timer_data, lpwm2_get_timer_data)
LROT_FUNCENTRY(get_pin_data, lpwm2_get_pin_data)
// LROT_FUNCENTRY(print_setup, lpwm2_print_setup)
// LROT_FUNCENTRY( time_it, lpwm2_timeit)
// LROT_FUNCENTRY( test_tmr_manual, lpwm2_timing_frc1_manual_load_overhead)
// LROT_FUNCENTRY( test_tmr_auto, lpwm2_timing_frc1_auto_load_overhead)
LROT_END(pwm2, NULL, 0)

NODEMCU_MODULE(PWM2, "pwm2", pwm2, lpwm2_open);
