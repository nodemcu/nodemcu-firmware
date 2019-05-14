// Module for interfacing with PWM

#include <stdlib.h>
#include "c_types.h"
#include "lauxlib.h"
#include "mem.h"
#include "module.h"
#include "pin_map.h"
#include "hw_timer.h"
#include "platform.h"

#define PWM2_TMR_MAGIC_80MHZ 16
#define PWM2_TMR_MAGIC_160MHZ 32

#define luaL_argcheck2(L, cond, numarg, extramsg) \
  if (!(cond)) return luaL_argerror(L, (numarg), (extramsg))

//############################
// global type definitions

typedef struct {
  uint32_t offInterruptCounter;
  uint32_t pulseInterruptCcounter;
  uint32_t currentInterruptCounter;
  uint16_t gpioMask;
} pwm2_pin_interrupt_t;

typedef struct {
  pwm2_pin_interrupt_t pin[GPIO_PIN_NUM];
  uint16_t enabledGpioMask;
} pwm2_interrupt_handler_data_t;

typedef struct {
  uint32_t pulseResolutions;
  uint32_t divisableFrequency;
  uint32_t frequencyDivisor;
  uint32_t duty;
  uint32_t resolutionCPUTicks;
  uint32_t resolutionInterruptCounterMultiplier;
} pwm2_pin_setup_t;

typedef struct {
  pwm2_pin_setup_t pin[GPIO_PIN_NUM];
  uint32_t interruptTimerCPUTicks;
  uint32_t interruptTimerTicks;
  bool isStarted;
} pwm2_setup_data_t;

typedef struct {
  pwm2_interrupt_handler_data_t interruptData;
  pwm2_setup_data_t setupData;
} pwm2_module_data_t;

//############################
// module vars, lazy initialized, allocated only if pwm2 is being used

static pwm2_module_data_t *moduleData = NULL;

//############################

static bool isPinSetup(const pwm2_module_data_t *data, const uint8_t pin) {
  return data->setupData.pin[pin].pulseResolutions > 0;
}

static uint32_t getCPUTicksPerSec() {
  return system_get_cpu_freq() * 1000000;
}

static uint8_t getCpuTimerTicksDivisor() {
  return system_get_cpu_freq() == 80 ? PWM2_TMR_MAGIC_80MHZ : PWM2_TMR_MAGIC_160MHZ;
}

static uint32_t findGCD(uint32_t n1, uint32_t n2) {
  uint32_t n3;
  while (n2 != 0) {
    n3 = n1;
    n1 = n2;
    n2 = n3 % n2;
  }
  return n1;
}

static uint32_t findGreatesCommonDividerForTimerTicks(uint32_t newTimerTicks, uint32_t oldTimerTicks) {
  return oldTimerTicks == 0 ? newTimerTicks : findGCD(newTimerTicks, oldTimerTicks);
}

static uint16_t findAllEnabledGpioMask(pwm2_module_data_t *moduleData) {
  uint16_t enableGpioMask = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (moduleData->setupData.pin[i].pulseResolutions > 0) {
      enableGpioMask |= moduleData->interruptData.pin[i].gpioMask;
    }
  }
  return enableGpioMask;
}

static uint32_t findCommonCPUTicksDivisor(pwm2_module_data_t *moduleData) {
  uint32_t gcdCPUTicks = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (moduleData->setupData.pin[i].pulseResolutions > 0) {
      gcdCPUTicks = findGreatesCommonDividerForTimerTicks(moduleData->setupData.pin[i].resolutionCPUTicks, gcdCPUTicks);
    }
  }
  return gcdCPUTicks;
}

static uint32_t cpuToTimerTicks(uint32_t cpuTicks) {
  return cpuTicks / getCpuTimerTicksDivisor();
}

static void updatePinResolutionToInterruptsMultiplier(pwm2_pin_setup_t *sPin, uint32_t timerCPUTicks) {
  sPin->resolutionInterruptCounterMultiplier = sPin->resolutionCPUTicks / timerCPUTicks;
}

static void updatePinPulseToInterruptsCounter(pwm2_pin_interrupt_t *iPin, pwm2_pin_setup_t *sPin) {
  iPin->pulseInterruptCcounter = (sPin->pulseResolutions + 1) * sPin->resolutionInterruptCounterMultiplier;
}

static uint8_t getDutyAdjustment(const uint32_t duty, const uint32_t pulse) {
  if (duty == 0) {
    return 0;
  } else if (duty == pulse) {
    return 2;
  } else {
    return 1;
  }
}

static void updatePinOffCounter(pwm2_pin_interrupt_t *iPin, pwm2_pin_setup_t *sPin) {
  iPin->offInterruptCounter = (sPin->duty + getDutyAdjustment(sPin->duty, sPin->pulseResolutions)) * sPin->resolutionInterruptCounterMultiplier;
}

static void reCalculateCommonToAllPinsData(pwm2_module_data_t *moduleData) {
  moduleData->interruptData.enabledGpioMask = findAllEnabledGpioMask(moduleData);
  moduleData->setupData.interruptTimerCPUTicks = findCommonCPUTicksDivisor(moduleData);
  moduleData->setupData.interruptTimerTicks = cpuToTimerTicks(moduleData->setupData.interruptTimerCPUTicks);
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (isPinSetup(moduleData, i)) {
      updatePinResolutionToInterruptsMultiplier(&moduleData->setupData.pin[i], moduleData->setupData.interruptTimerCPUTicks);
      updatePinPulseToInterruptsCounter(&moduleData->interruptData.pin[i], &moduleData->setupData.pin[i]);
      updatePinOffCounter(&moduleData->interruptData.pin[i], &moduleData->setupData.pin[i]);
    }
  }
}

//############################
// interrupt handler related

static inline void computeIsPinOn(pwm2_pin_interrupt_t *pin, uint16_t *maskOn) {
  if (pin->currentInterruptCounter == pin->pulseInterruptCcounter) {
    pin->currentInterruptCounter = 1;
  } else {
    pin->currentInterruptCounter++;
  }
  // ets_printf("curr=%u    on=%u\n", pin->currentInterruptCounter, (pin->currentInterruptCounter < pin->offInterruptCounter));
  if (pin->currentInterruptCounter < pin->offInterruptCounter) {
    *maskOn |= pin->gpioMask;
  }
}

static inline bool isPinSetup2(const pwm2_interrupt_handler_data_t *data, const uint8_t pin) {
  return data->pin[pin].gpioMask > 0;
}

static inline uint16_t findAllPinOns(pwm2_interrupt_handler_data_t *data) {
  uint16_t maskOn = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (isPinSetup2(data, i)) {
      computeIsPinOn(&data->pin[i], &maskOn);
    }
  }
  return maskOn;
}

static inline void setGpioPins(const uint16_t enabledGpioMask, const register uint16_t maskOn) {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, maskOn);
  const register uint16_t maskOff = ~maskOn & enabledGpioMask;
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, maskOff);
}

static void ICACHE_RAM_ATTR timerInterruptHandler(os_param_t arg) {
  pwm2_interrupt_handler_data_t *data = (pwm2_interrupt_handler_data_t *)arg;
  setGpioPins(data->enabledGpioMask, findAllPinOns(data));
}

//############################
// lua bindings

static int lpwm2_open(lua_State *L) {
  moduleData = os_malloc(sizeof(pwm2_module_data_t));
  memset(moduleData, 0, sizeof(*moduleData));
  return 0;
}

static int lpwm2_get_timer_data(lua_State *L) {
  lua_pushboolean(L, moduleData->setupData.isStarted);
  lua_pushinteger(L, moduleData->setupData.interruptTimerCPUTicks);
  lua_pushinteger(L, moduleData->setupData.interruptTimerTicks);
  return 0;
}

static int lpwm2_get_pin_data(lua_State *L) {
  const uint8 pin = luaL_checkinteger(L, 1);
  luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, 1, "invalid pin number");
  lua_pushinteger(L, moduleData->setupData.pin[pin].duty);
  lua_pushinteger(L, moduleData->setupData.pin[pin].pulseResolutions);
  lua_pushinteger(L, moduleData->setupData.pin[pin].divisableFrequency);
  lua_pushinteger(L, moduleData->setupData.pin[pin].frequencyDivisor);
  lua_pushinteger(L, moduleData->setupData.pin[pin].resolutionCPUTicks);
  lua_pushinteger(L, moduleData->setupData.pin[pin].resolutionInterruptCounterMultiplier);
  return 0;
}

// static int lpwm2_print_setup(lua_State *L) {
//   ets_printf("pwm2 : isStarted %u\n", moduleData->setupData.isStarted);
//   ets_printf("pwm2 : interruptTimerCPUTicks %u\n", moduleData->setupData.interruptTimerCPUTicks);
//   ets_printf("pwm2 : interruptTimerTicks %u\n", moduleData->setupData.interruptTimerTicks);
//   ets_printf("pin, duty, pulse, resolutionAsCPU, resolutionIntrCntMultiplier, divisableFreq, freqDivisor\n");
//   for (int i = 1; i < GPIO_PIN_NUM; i++) {
//     if (isPinSetup(moduleData, i)) {
//       ets_printf("%u, %u, %u, %u, %u, %u, %u\n",
//                  i,
//                  moduleData->setupData.pin[i].duty,
//                  moduleData->setupData.pin[i].pulseResolutions,
//                  moduleData->setupData.pin[i].resolutionCPUTicks,
//                  moduleData->setupData.pin[i].resolutionInterruptCounterMultiplier,
//                  moduleData->setupData.pin[i].divisableFrequency,
//                  moduleData->setupData.pin[i].frequencyDivisor);
//     }
//   }
//   ets_printf("pwm2 : enabledGpioMask %X\n", moduleData->interruptData.enabledGpioMask);
//   ets_printf("pin, pulseAsInterruptCnt, pinOffWhenIntrCnt, currentintrCnt, gpioMask\n");
//   for (int i = 1; i < GPIO_PIN_NUM; i++) {
//     if (isPinSetup(moduleData, i)) {
//       ets_printf("%u, %u, %u, %u, %X\n",
//                  i,
//                  moduleData->interruptData.pin[i].pulseInterruptCcounter,
//                  moduleData->interruptData.pin[i].offInterruptCounter,
//                  moduleData->interruptData.pin[i].currentInterruptCounter,
//                  moduleData->interruptData.pin[i].gpioMask);
//     }
//   }
// }

static uint64_t enduserFreqToCPUTicks(const uint64_t divisableFreq, const uint64_t freqDivisor, const uint64_t resolution) {
  return (getCPUTicksPerSec() / (freqDivisor * resolution)) * divisableFreq;
}

static uint16_t getPinGpioMask(uint8_t pin) {
  return 1 << GPIO_ID_PIN(pin_num[pin]);
}

static void set_duty(pwm2_module_data_t *moduleData, const uint8_t pin, const uint32_t duty) {
  pwm2_pin_setup_t *sPin = &moduleData->setupData.pin[pin];
  pwm2_pin_interrupt_t *iPin = &moduleData->interruptData.pin[pin];
  sPin->duty = duty;
  updatePinOffCounter(iPin, sPin);
}

static int setup_pin(
    pwm2_module_data_t *moduleData,
    const uint8_t pin,
    const uint32_t divisableFreq,
    const uint32_t freqDivisor,
    const uint32_t resolution,
    const uint32_t initDuty) {
  moduleData->setupData.pin[pin].pulseResolutions = resolution;
  moduleData->setupData.pin[pin].divisableFrequency = divisableFreq;
  moduleData->setupData.pin[pin].frequencyDivisor = freqDivisor;
  moduleData->setupData.pin[pin].resolutionCPUTicks = enduserFreqToCPUTicks(divisableFreq, freqDivisor, resolution);
  moduleData->interruptData.pin[pin].gpioMask = getPinGpioMask(pin);
  reCalculateCommonToAllPinsData(moduleData);
  set_duty(moduleData, pin, initDuty);
}

static int lpwm2_setup_pin_common(lua_State *L, const bool isFreqHz) {
  if (moduleData->setupData.isStarted) {
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
    setup_pin(moduleData, pin, freqFractions, freq, resolution, initDuty);
  } else {
    setup_pin(moduleData, pin, freq, freqFractions, resolution, initDuty);
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
    luaL_argcheck2(L, duty >= 0 && duty <= moduleData->setupData.pin[pin].pulseResolutions, pos, "invalid duty");

    if (!isPinSetup(moduleData, pin)) {
      return luaL_error(L, "pwm2 : pin=%d is not setup yet", pin);
    }
    set_duty(moduleData, pin, duty);
  }
  return 0;
}

static int lpwm2_release_pin(lua_State *L) {
  if (moduleData->setupData.isStarted) {
    return luaL_error(L, "pwm2 : pwm is started, stop it first.");
  }
  int pos = 0;
  while (true) {
    int pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1) {
      break;
    }
    luaL_argcheck2(L, pin > 0 && pin <= GPIO_PIN_NUM, pos, "invalid pin number");
    moduleData->setupData.pin[pin].pulseResolutions = 0;
    moduleData->interruptData.pin[pin].gpioMask = 0;
  }
  return 0;
}

static int lpwm2_stop(lua_State *L) {
  if (!moduleData->setupData.isStarted) {
    return 0;
  }
  platform_hw_timer_close_exclusive();
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, moduleData->interruptData.enabledGpioMask);  // clear pins of being gpio output
  moduleData->setupData.isStarted = false;
  return 0;
}

static void configureAllPinsAsGpioOutput(pwm2_module_data_t *moduleData) {
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (isPinSetup(moduleData, i)) {
      PIN_FUNC_SELECT(pin_mux[i], pin_func[i]);  // set pin as gpio
      PIN_PULLUP_EN(pin_mux[i]);                 // set pin pullup on
    }
  }
}

static void resetPinCounters(pwm2_module_data_t *moduleData) {
  for (int i = 1; i < GPIO_PIN_NUM; i++) {
    if (isPinSetup(moduleData, i)) {
      moduleData->interruptData.pin[i].currentInterruptCounter = 0;
    }
  }
}

static int lpwm2_start(lua_State *L) {
  if (moduleData->setupData.isStarted) {
    return 0;
  }
  if (!platform_hw_timer_init_exclusive(FRC1_SOURCE, TRUE, timerInterruptHandler, (os_param_t)&moduleData->interruptData, (void (*)(void))NULL)) {
    return luaL_error(L, "pwm2: currently platform timer1 is being used by another module.\n");
  }
  configureAllPinsAsGpioOutput(moduleData);
  resetPinCounters(moduleData);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, moduleData->interruptData.enabledGpioMask);  // set pins as gpio output
  moduleData->setupData.isStarted = true;
  platform_hw_timer_arm_ticks_exclusive(moduleData->setupData.interruptTimerTicks);
  return 0;
}

// // #######################################
// // #######################################
// // #######################################
// // TESTING and TIMING

// static inline uint32_t getCpuTicks() {
//   register uint32_t cycles;
//   asm volatile("rsr %0,ccount"
//                : "=a"(cycles));
//   return cycles;
// }

// static uint32_t __repeat_cb(int cnt, void (*callback)()) {
//   while (cnt-- > 0) {
//     callback();
//   }
// }

// static uint32_t __measure(void (*callback)()) {
//   const int cnt = 1000;
//   int32_t btime, etime;

//   btime = getCpuTicks();
//   __repeat_cb(cnt, callback);
//   etime = getCpuTicks();
//   return ((uint32_t)(etime - btime)) / cnt;
// }

// static void __timing_intr() {
//   timerInterruptHandler((os_param_t)&moduleData->interruptData);
// }

// static int lpwm2_timeit(lua_State *L) {
//   for (int i = 1; i < GPIO_PIN_NUM; i++) {
//     setup_pin(moduleData, i, 1, 1000, 2, 1);
//     resetPinCounters(moduleData);
//     ets_printf("    with pins %d: %u ticks\n",
//                i,
//                __measure(__timing_intr));
//   }
//   return 0;
// }

// static struct{ /* used ny timer function */
//   int cnt;
//   int exitWhenZero;
//   char *msg;
//   uint32_t ticks;
//   uint32_t btime;
//   uint32_t duration;
//   uint32_t ownTime;
// } timerData;

// static void ICACHE_RAM_ATTR __timer_intr_manual(os_param_t arg) {
//   timerData.exitWhenZero++;
//   if (timerData.exitWhenZero >= timerData.cnt) {
//     if (timerData.exitWhenZero == timerData.cnt) {
//       const uint32_t etime = getCpuTicks();
//       timerData.duration = ((uint32_t)(etime - timerData.btime)) / (timerData.cnt - 1);
//       ets_printf("%s: %u ticks\n", timerData.msg, (timerData.duration - timerData.ownTime));
//     }
//     platform_hw_timer_close_exclusive();
//   } else {
//     if (timerData.exitWhenZero == 1) {
//       timerData.btime = getCpuTicks();
//     }
//     platform_hw_timer_arm_ticks_exclusive(timerData.ticks);
//   }
// }

// static int lpwm2_timing_frc1_manual_load_overhead(lua_State *L) {
//   timerData.ownTime = 0;
//   timerData.cnt = 1001;
//   timerData.ticks = 3;
//   timerData.msg = "  timer_manual_own_time";
//   timerData.exitWhenZero = 0;
//   for (int i = 0; i <= timerData.cnt; i++) {
//     __timer_intr_manual((os_param_t)NULL);
//   }
//   timerData.ownTime = timerData.duration;
//   if (!platform_hw_timer_init_exclusive(FRC1_SOURCE, FALSE, __timer_intr_manual, (os_param_t)NULL, (void(*)(void))NULL)) {
//     ets_printf("Currently platform timer1 is being used by another module.\n");
//   } else {
//     timerData.msg = "    tmr_manual_overhead";
//     timerData.exitWhenZero = 0;
//     platform_hw_timer_arm_ticks_exclusive(1);
//   }
//   return 0;
// }

// static void ICACHE_RAM_ATTR __timer_intr_auto(os_param_t arg) {
//   timerData.exitWhenZero++;
//   if (timerData.exitWhenZero > timerData.cnt) {
//   } else if (timerData.exitWhenZero == timerData.cnt) {
//     const uint32_t etime = getCpuTicks();
//     timerData.duration = ((uint32_t)(etime - timerData.btime)) / (timerData.cnt - 1);
//     ets_printf("%s: %u ticks\n", timerData.msg, (timerData.duration - timerData.ownTime));
//     platform_hw_timer_close_exclusive();
//   } else {
//     if (timerData.exitWhenZero == 1) {
//       timerData.btime = getCpuTicks();
//     }
//   }
// }

// static int lpwm2_timing_frc1_auto_load_overhead(lua_State *L) {
//   timerData.ownTime = 0;
//   timerData.cnt = 1001;
//   timerData.ticks = 3;
//   timerData.msg = "    timer_auto_own_time";
//   timerData.exitWhenZero = 0;
//   for (int i = 0; i <= timerData.cnt; i++) {
//     __timer_intr_auto((os_param_t)NULL);
//   }
//   timerData.ownTime = timerData.duration;
//   if (!platform_hw_timer_init_exclusive(FRC1_SOURCE, TRUE, __timer_intr_auto, (os_param_t)NULL, (void(*)(void))NULL)) {
//     ets_printf("Currently platform timer1 is being used by another module.\n");
//   } else {
//     timerData.msg = "      tmr_auto_overhead";
//     timerData.exitWhenZero = 0;
//     platform_hw_timer_arm_ticks_exclusive(1);
//   }
//   return 0;
// }

// Module function map
LROT_BEGIN(pwm2)
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
