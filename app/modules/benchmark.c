/*
 * Source code inspired or based on https://sub.nanona.fi/esp8266/timing-and-ticks.html
 * 
 * This way one can compare measurements of different setups directly against idle, SDK-only based results.
 * 
 */

#include <stdint.h>
#include <stdlib.h>
#include "platform.h"
#include "hw_timer.h"
#include "lauxlib.h"
#include "mem.h"
#include "module.h"
#include "osapi.h"
#include "pin_map.h"
#include "user_interface.h"

/* need to prevent compiler optimizations */
#define __NOINLINE __attribute__((noinline))

/* local helpers... */
#define __asize(x) (sizeof(x) / sizeof(x[0]))

#define luaL_argcheck2(L, cond, numarg, extramsg) \
  if (!(cond)) return luaL_argerror(L, (numarg), (extramsg))

#define assertNoErr(L, cond) \
  if (cond != 0) return

// #####################
// static test buffers to prevent compiler optimizations

static volatile uint32_t __dummy_val;
static char __dummy_buf1[1024];
static char __dummy_buf2[1024];
static struct { /* used by timer function */
  bool isManualMode;
  bool isTimerExclusive;
  uint16_t cnt;
  uint16_t exitWhenCnt;
  uint32_t ticks;
  uint32_t btime;
  uint32_t duration;
  uint32_t ownTime;
  int lua_cb_fnc;
  uint32_t *individualDelays;
} __timerData;
static uint16_t repetitions;
static uint16_t maskOnW1TSC;
static struct { /* used by benchmarking lua functions */
  int cb;
  lua_State *L;
} __luaFunc;

// #####################
// helpers

static inline int32_t asm_ccount(void) {
  int32_t r;
  asm volatile("rsr %0, ccount"
               : "=r"(r));
  return r;
}

static inline void asm_nop() {
  asm volatile("nop");
}

static uint32_t __repeat_cb(int cnt, void (*callback)()) {
  while (cnt-- > 0) {
    callback();
  }
}

static uint32_t __measure(void (*callback)()) {
  const int32_t btime = asm_ccount();
  __repeat_cb(repetitions, callback);
  const int32_t etime = asm_ccount();
  return ((uint32_t)(etime - btime)) / repetitions;
}

// ########################
// timing-tests

static void __NOINLINE __timing_empty() {
  /* nothing */
}

static void __NOINLINE __timing_single_nop() {
  asm_nop();
}

static void __NOINLINE __timing_re_recurse() {
  __timing_single_nop();
}

static void __NOINLINE __timing_1us_sleep() {
  os_delay_us(1);
}

static void __NOINLINE __timing_10us_sleep() {
  os_delay_us(10);
}

static void __NOINLINE __timing_100us_sleep() {
  os_delay_us(100);
}

static void __NOINLINE __timing_gpio_read_pin() {
  /* read gpio4 -state */
  __dummy_val = GPIO_INPUT_GET(4);
}

static void __NOINLINE __timing_get_cpu_counts() {
  /* read gpio4 -state */
  __dummy_val = asm_ccount();
}

static void __NOINLINE __timing_gpio_pull_updown() {
  GPIO_OUTPUT_SET(5, 1);
  GPIO_OUTPUT_SET(5, 0);
}

static void __NOINLINE __timing_gpio_status_read() {
  __dummy_val = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
}

static void __NOINLINE __timing_gpio_status_write() {
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 0);
}

static void __NOINLINE __timing_adc_read() {
  system_adc_read();
}

static void __NOINLINE __timing_system_get_time() {
  __dummy_val = system_get_time();
}

static void __NOINLINE __timing_system_get_rtc_time() {
  __dummy_val = system_get_rtc_time();
}

static void __NOINLINE __timing_system_get_cpufreq() {
  __dummy_val = system_get_cpu_freq();
}

static void __NOINLINE __timing_memcpy_1k_bytes() {
  os_memcpy(__dummy_buf1, __dummy_buf2, sizeof(__dummy_buf1));
}

static void __NOINLINE __timing_memset_1k_bytes() {
  os_memset(__dummy_buf1, 0, sizeof(__dummy_buf1));
}

static void __NOINLINE __timing_bzero_1k_bytes() {
  os_bzero(__dummy_buf1, sizeof(__dummy_buf1));
}

static void __NOINLINE __timing_gpio_set_one() {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, maskOnW1TSC);
}

// timer interrupt handler related

static void __report_interrupt_times() {
  if (__timerData.lua_cb_fnc == 0) return;
  lua_State *L = lua_getstate();
  lua_rawgeti(L, LUA_REGISTRYINDEX, __timerData.lua_cb_fnc);
  lua_pushnumber(L, (double)__timerData.duration - __timerData.ownTime);
  lua_pushnumber(L, (double)__timerData.ownTime);
  lua_pushnumber(L, (double)__timerData.duration);
  lua_call(L, 3, 0);
  luaL_unref(L, LUA_REGISTRYINDEX, __timerData.lua_cb_fnc);
}

static inline void __set_start_time() {
  if (__timerData.exitWhenCnt == 1) {
    __timerData.btime = asm_ccount();
  }
}

static inline void __calc_duration() {
  __timerData.duration = ((uint32_t)(asm_ccount() - __timerData.btime)) / (__timerData.cnt - 1);
}

static inline void __close_timer() {
  if (__timerData.isTimerExclusive) {
    platform_hw_timer_close_exclusive();
  } else {
    platform_hw_timer_close('B');
  }
}

static inline void __interrupt_test_is_over() {
  __calc_duration();
  __close_timer();
  __report_interrupt_times();
}

static inline void __recordInterruptTimeIfAsked() {
  if (__timerData.individualDelays != NULL) {
    *(__timerData.individualDelays + __timerData.exitWhenCnt) = asm_ccount();
  }
}

static inline void __scheduleNextInterruptIfNeeded() {
  if (__timerData.isManualMode) {
    if (__timerData.isTimerExclusive) {
      RTC_REG_WRITE(FRC1_LOAD_ADDRESS, __timerData.ticks);
    } else {
      platform_hw_timer_arm_ticks('B', __timerData.ticks);
    }
  }
}

static void ICACHE_RAM_ATTR __timer_interrupt(os_param_t arg) {
  __timerData.exitWhenCnt++;
  if (__timerData.exitWhenCnt > __timerData.cnt) {
    __close_timer();
  } else {
    __recordInterruptTimeIfAsked();
    if (__timerData.exitWhenCnt == __timerData.cnt) {
      __interrupt_test_is_over();
    } else {
      __set_start_time();
      __scheduleNextInterruptIfNeeded();
    }
  }
}

// lua function benchmarking

static void __run_lua_func() {
  lua_rawgeti(__luaFunc.L, LUA_REGISTRYINDEX, __luaFunc.cb);
  lua_call(__luaFunc.L, 0, 0);
}

// ###########################
// lua methods

static int lbench_empty_call(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_empty));
  return 1;
}

static int lbench_single_nop(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_single_nop));
  return 1;
}

static int lbench_get_cpu_counts(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_get_cpu_counts));
  return 1;
}

static int lbench_re_recurse(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_re_recurse));
  return 1;
}

static int lbench_1us_sleep(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_1us_sleep));
  return 1;
}

static int lbench_10us_sleep(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_10us_sleep));
  return 1;
}

static int lbench_100us_sleep(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_100us_sleep));
  return 1;
}

static int lbench_gpio_read_pin(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_gpio_read_pin));
  return 1;
}

static int lbench_gpio_pull_updown(lua_State *L) {
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
  GPIO_OUTPUT_SET(5, 0);
  lua_pushnumber(L, (double)__measure(__timing_gpio_pull_updown));
  return 1;
}

static int lbench_gpio_status_read(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_gpio_status_read));
  return 1;
}

static int lbench_gpio_status_write(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_gpio_status_write));
  return 1;
}

static int lbench_adc_read(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_adc_read));
  return 1;
}

static int lbench_system_get_time(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_system_get_time));
  return 1;
}

static int lbench_system_get_rtc_time(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_system_get_rtc_time));
  return 1;
}

static int lbench_system_get_cpufreq(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_system_get_cpufreq));
  return 1;
}

static int lbench_memcpy_1k_bytes(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_memcpy_1k_bytes));
  return 1;
}

static int lbench_memset_1k_bytes(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_memset_1k_bytes));
  return 1;
}

static int lbench_bzero_1k_bytes(lua_State *L) {
  lua_pushnumber(L, (double)__measure(__timing_bzero_1k_bytes));
  return 1;
}

static uint16_t __getPinGpioMask(uint8_t pin) {
  return 1 << GPIO_ID_PIN(pin_num[pin]);
}

static int lbench_gpio_set_one(lua_State *L) {
  const int pin = lua_tointeger(L, 1);
  luaL_argcheck2(L, pin > 0 && pin < GPIO_PIN_NUM, 1, "invalid pin number");
  maskOnW1TSC = __getPinGpioMask(pin);
  PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, maskOnW1TSC);
  lua_pushnumber(L, (double)__measure(__timing_gpio_set_one));
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, maskOnW1TSC);
  return 1;
}

static int __get_lua_cb_arg(lua_State *L, const int argPos) {
  lua_pushvalue(L, argPos);
  return luaL_ref(L, LUA_REGISTRYINDEX);
}

static void __setup_timer_interrupt(lua_State *L) {
  if (__timerData.isTimerExclusive) {
    const bool flg = platform_hw_timer_init_exclusive(FRC1_SOURCE, !__timerData.isManualMode, __timer_interrupt, (os_param_t)NULL, (void (*)(void))NULL);
    if (!flg) {
      luaL_error(L, "Currently platform timer1 is being used by another module.\n");
      return;
    }
    platform_hw_timer_arm_ticks_exclusive(__timerData.ticks);
  } else {
    if (!platform_hw_timer_init('B', FRC1_SOURCE, !__timerData.isManualMode)) {
      luaL_error(L, "Currently platform timer1 is being used by another module.\n");
      return;
    }
    if (!platform_hw_timer_set_func('B', __timer_interrupt, (os_param_t)NULL)) {
      luaL_error(L, "Failed to assign timer interrupt handler for benchmark module.\n");
      return;
    }
    if (!platform_hw_timer_arm_ticks('B', __timerData.ticks)) {
      luaL_error(L, "Failed to assign arm the timer for benchmark module.\n");
    }
  }
}

static void __benchmark_interrupt_handler_own_time() {
  __timerData.ownTime = 0;
  const int oldVal = __timerData.lua_cb_fnc;
  __timerData.lua_cb_fnc = 0;
  const bool oldVal2 = __timerData.isManualMode;
  __timerData.isManualMode = false;
  for (int i = 0; i <= __timerData.cnt; i++) {
    __timer_interrupt((os_param_t)NULL);
  }
  __timerData.ownTime = __timerData.duration;
  __timerData.lua_cb_fnc = oldVal;
  __timerData.isManualMode = oldVal2;
  __timerData.exitWhenCnt = 0;
}

static int __benchmark_timer_common(lua_State *L) {
  __benchmark_interrupt_handler_own_time();
  __setup_timer_interrupt(L);
  return 0;
}

static void __setup_timer_args(lua_State *L, const bool isManualMode, const bool isTimerExclusive) {
  __timerData.ticks = lua_tointeger(L, 1);
  __timerData.lua_cb_fnc = __get_lua_cb_arg(L, 3);
  __timerData.cnt = repetitions + 1;
  __timerData.isTimerExclusive = isTimerExclusive;
  __timerData.isManualMode = isManualMode;
  __timerData.exitWhenCnt = 0;
  __timerData.duration = 0;
  if (NULL != __timerData.individualDelays) {
    free(__timerData.individualDelays);
    __timerData.individualDelays = NULL;
  }
  const int shouldGatherIndividualTimes = lua_toboolean(L, 2);
  if (shouldGatherIndividualTimes) {
    __timerData.individualDelays = malloc((repetitions + 2) * sizeof(uint32_t));
  }
}

#define AssertLuaArgs                                                                      \
  luaL_argcheck2(L, ((uint32_t)lua_tointeger(L, 1)) >= 0, 1, "invalid timerTicks number"); \
  luaL_argcheck2(L, lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION, 3, "invalid callback function");

static int lbench_timing_frc1_manual_overhead_exclusive(lua_State *L) {
  AssertLuaArgs;
  __setup_timer_args(L, true, true);
  return __benchmark_timer_common(L);
}

static int lbench_timing_frc1_autoload_overhead_exclusive(lua_State *L) {
  AssertLuaArgs;
  __setup_timer_args(L, false, true);
  return __benchmark_timer_common(L);
}

static int lbench_timing_frc1_manual_overhead_shared(lua_State *L) {
  AssertLuaArgs;
  __setup_timer_args(L, true, false);
  return __benchmark_timer_common(L);
}

static int lbench_timing_frc1_autoload_overhead_shared(lua_State *L) {
  AssertLuaArgs;
  __setup_timer_args(L, false, false);
  return __benchmark_timer_common(L);
}

static int lbench_set_repetitions(lua_State *L) {
  const int r = lua_tointeger(L, 1);
  luaL_argcheck2(L, r > 0, 1, "invalid repetitions number");
  repetitions = r;
  return 0;
}

static int lbench_get_repetitions(lua_State *L) {
  lua_pushinteger(L, repetitions);
  return 1;
}

static int lbench_get_recorded_interrupt_counter(lua_State *L) {
  const int r = lua_tointeger(L, 1);
  luaL_argcheck2(L, r > 0 && r <= repetitions, 1, "out of range repetitions number");
  if (__timerData.individualDelays != NULL) {
    lua_pushnumber(L, (double)*(__timerData.individualDelays + r));
  } else {
    lua_pushnumber(L, 0);
  }
  return 1;
}

static int lbench_open(lua_State *L) {
  gpio_init();
  repetitions = 2000;
  __timerData.individualDelays = NULL;
}



static int lbench_print_timer_data(lua_State *L) {
  ets_printf("isManualMode = %d\n", __timerData.isManualMode);
  ets_printf("isTimerExclusive = %d\n", __timerData.isTimerExclusive);
  ets_printf("cnt = %d\n", __timerData.cnt);
  ets_printf("exitWhenCnt = %d\n", __timerData.exitWhenCnt);
  ets_printf("ticks = %d\n", __timerData.ticks);
  ets_printf("btime = %d\n", __timerData.btime);
  ets_printf("duration = %d\n", __timerData.duration);
  ets_printf("ownTime = %d\n", __timerData.ownTime);
  ets_printf("lua_cb_fnc = %d\n", __timerData.lua_cb_fnc);
  ets_printf("individualDelays = %d\n", __timerData.individualDelays);
}

static int lbench_ccount(lua_State *L) {
  lua_pushinteger(L, asm_ccount());
  return 1;
}

static int lbench_lua_func(lua_State *L) {
  // set up the new callback if present
  lua_pushvalue(L, 1);
  __luaFunc.cb = luaL_ref(L, LUA_REGISTRYINDEX);
  __luaFunc.L = L;
  const double val = (double)__measure(__run_lua_func);
  luaL_unref(__luaFunc.L, LUA_REGISTRYINDEX, __luaFunc.cb);
  lua_pushnumber(L, val);
  // lua_pushnumber(L, (double)__measure(__run_lua_func));
  return 1;
}

// Module function map
LROT_BEGIN(benchmark)
LROT_FUNCENTRY(ccount, lbench_ccount)
LROT_FUNCENTRY(bench_lua_func, lbench_lua_func)
LROT_FUNCENTRY(print_timer_data, lbench_print_timer_data)
LROT_FUNCENTRY(set_repetitions, lbench_set_repetitions)
LROT_FUNCENTRY(get_repetitions, lbench_get_repetitions)
LROT_FUNCENTRY(empty_call, lbench_empty_call)
LROT_FUNCENTRY(single_nop, lbench_single_nop)
LROT_FUNCENTRY(get_cpu_counts, lbench_get_cpu_counts)
LROT_FUNCENTRY(re_recurse, lbench_re_recurse)
LROT_FUNCENTRY(us1_sleep, lbench_1us_sleep)
LROT_FUNCENTRY(us10_sleep, lbench_10us_sleep)
LROT_FUNCENTRY(us100_sleep, lbench_100us_sleep)
LROT_FUNCENTRY(gpio_read_pin, lbench_gpio_read_pin)
LROT_FUNCENTRY(gpio_pull_updown, lbench_gpio_pull_updown)
LROT_FUNCENTRY(gpio_status_read, lbench_gpio_status_read)
LROT_FUNCENTRY(gpio_status_write, lbench_gpio_status_write)
LROT_FUNCENTRY(adc_read, lbench_adc_read)
LROT_FUNCENTRY(system_get_time, lbench_system_get_time)
LROT_FUNCENTRY(system_get_rtc_time, lbench_system_get_rtc_time)
LROT_FUNCENTRY(system_get_cpufreq, lbench_system_get_cpufreq)
LROT_FUNCENTRY(memcpy_1k_bytes, lbench_memcpy_1k_bytes)
LROT_FUNCENTRY(memset_1k_bytes, lbench_memset_1k_bytes)
LROT_FUNCENTRY(bzero_1k_bytes, lbench_bzero_1k_bytes)
LROT_FUNCENTRY(gpio_set_one, lbench_gpio_set_one)
LROT_FUNCENTRY(frc1_manual_exclusive, lbench_timing_frc1_manual_overhead_exclusive)
LROT_FUNCENTRY(frc1_autoload_exclusive, lbench_timing_frc1_autoload_overhead_exclusive)
LROT_FUNCENTRY(frc1_manual_shared, lbench_timing_frc1_manual_overhead_shared)
LROT_FUNCENTRY(frc1_autoload_shared, lbench_timing_frc1_autoload_overhead_shared)
LROT_FUNCENTRY(get_recorded_interrupt_counter, lbench_get_recorded_interrupt_counter)
LROT_END(benchmark, NULL, 0)

NODEMCU_MODULE(BENCHMARK, "benchmark", benchmark, lbench_open);
