// Module for interfacing with PWM

#include <stdlib.h>
#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_types.h"
#include "pin_map.h"
#include "mem.h"
#include "hw_timer.h"

#define PWM2_TMR_OWNER ((os_param_t)'2')
#define PWM2_TMR_MAGIC_80MHZ 16
#define PWM2_TMR_MAGIC_160MHZ 32

//############################
// global type definitions

typedef struct
{
  uint32_t offInterruptCounter;
  uint32_t pulseInterruptCcounter;
  uint32_t currentInterruptCounter;
  uint16_t gpioMask;
} pwm2_pin_interrupt_t;

typedef struct
{
  pwm2_pin_interrupt_t pin[GPIO_PIN_NUM];
  uint16_t enabledGpioMask;
} pwm2_interrupt_handler_data_t;

typedef struct
{
  uint32_t pulseResolutions;
  uint32_t divisableFrequency;
  uint32_t frequencyDivisor;
  uint32_t duty;
  uint32_t resolutionCPUTicks;
  uint32_t resolutionInterruptCounterMultiplier;
} pwm2_pin_setup_t;

typedef struct
{
  pwm2_pin_setup_t pin[GPIO_PIN_NUM];
  uint32_t interruptTimerCPUTicks;
  uint32_t interruptTimerTicks;
  bool isStarted;
} pwm2_setup_data_t;

typedef struct
{
  pwm2_interrupt_handler_data_t interruptData;
  pwm2_setup_data_t setupData;
} pwm2_module_data_t;

//############################
// module vars, lazy initialized, allocated only if pwm2 is being used

static pwm2_module_data_t *moduleData = NULL;

//############################

// lua utils

static uint32_t getCPUTicksPerSec()
{
  return system_get_cpu_freq() * 1000000;
}

static uint8_t getCpuTimerTicksDivisor()
{
  return system_get_cpu_freq() == 80 ? PWM2_TMR_MAGIC_80MHZ : PWM2_TMR_MAGIC_160MHZ;
}

static uint32_t findGCD(uint32_t n1, uint32_t n2)
{
  while (n1 != n2)
  {
    if (n1 > n2)
      n1 -= n2;
    else
      n2 -= n1;
  }
  return n1;
}

static uint32_t findGreatesCommonDividerForTimerTicks(uint32_t newTimerTicks, uint32_t oldTimerTicks)
{
  return oldTimerTicks == 0 ? newTimerTicks : findGCD(newTimerTicks, oldTimerTicks);
}

static uint16_t findAllEnabledGpioMask(pwm2_module_data_t *moduleData)
{
  uint16_t enableGpioMask = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      enableGpioMask |= moduleData->interruptData.pin[i].gpioMask;
    }
  }
  return enableGpioMask;
}

static uint32_t findCommonCPUTicksDivisor(pwm2_module_data_t *moduleData)
{
  uint32_t gcdCPUTicks = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      gcdCPUTicks = findGreatesCommonDividerForTimerTicks(moduleData->setupData.pin[i].resolutionCPUTicks, gcdCPUTicks);
    }
  }
  return gcdCPUTicks;
}

static uint32_t cpuToTimerTicks(uint32_t cpuTicks)
{
  return cpuTicks / getCpuTimerTicksDivisor();
}

static void updatePinResolutionToInterruptsMultiplier(pwm2_pin_setup_t *sPin, uint32_t timerCPUTicks)
{
  sPin->resolutionInterruptCounterMultiplier = sPin->resolutionCPUTicks / timerCPUTicks;
}

static void updatePinPulseToInterruptsCounter(pwm2_pin_interrupt_t *iPin, pwm2_pin_setup_t *sPin)
{
  iPin->pulseInterruptCcounter = (sPin->pulseResolutions + 1) * sPin->resolutionInterruptCounterMultiplier;
}

static uint8_t getDutyAdjustment(const uint32_t duty, const uint32_t pulse)
{
  if (duty == 0)
  {
    return 0;
  }
  else if (duty == pulse)
  {
    return 2;
  }
  else
  {
    return 1;
  }
}

static void updatePinOffCounter(pwm2_pin_interrupt_t *iPin, pwm2_pin_setup_t *sPin)
{
  iPin->offInterruptCounter = (sPin->duty + getDutyAdjustment(sPin->duty, sPin->pulseResolutions)) * sPin->resolutionInterruptCounterMultiplier;
}

static void reCalculateCommonToAllPinsData(pwm2_module_data_t *moduleData)
{
  moduleData->interruptData.enabledGpioMask = findAllEnabledGpioMask(moduleData);
  moduleData->setupData.interruptTimerCPUTicks = findCommonCPUTicksDivisor(moduleData);
  moduleData->setupData.interruptTimerTicks = cpuToTimerTicks(moduleData->setupData.interruptTimerCPUTicks);
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      updatePinResolutionToInterruptsMultiplier(&moduleData->setupData.pin[i], moduleData->setupData.interruptTimerCPUTicks);
      updatePinPulseToInterruptsCounter(&moduleData->interruptData.pin[i], &moduleData->setupData.pin[i]);
      updatePinOffCounter(&moduleData->interruptData.pin[i], &moduleData->setupData.pin[i]);
    }
  }
}

//############################
// interrupt handler related

static inline void computeIsPinOn(pwm2_pin_interrupt_t *pin, uint16_t *maskOn)
{
  if (pin->currentInterruptCounter == pin->pulseInterruptCcounter)
  {
    pin->currentInterruptCounter = 1;
  }
  else
  {
    pin->currentInterruptCounter++;
  }
  // ets_printf("curr=%u    on=%u\n", pin->currentInterruptCounter, (pin->currentInterruptCounter < pin->offInterruptCounter));
  if (pin->currentInterruptCounter < pin->offInterruptCounter)
  {
    *maskOn |= pin->gpioMask;
  }
}

static inline uint16_t findAllPinOns(pwm2_interrupt_handler_data_t *data)
{
  uint16_t maskOn = 0;
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (data->pin[i].gpioMask > 0)
    {
      computeIsPinOn(&data->pin[i], &maskOn);
    }
  }
  return maskOn;
}

static inline void setGpioPins(const uint16_t enabledGpioMask, const register uint16_t maskOn)
{
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, maskOn);
  const register uint16_t maskOff = ~maskOn & enabledGpioMask;
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, maskOff);
}

static void ICACHE_RAM_ATTR timerInterruptHandler(os_param_t arg)
{
  pwm2_interrupt_handler_data_t *data = (pwm2_interrupt_handler_data_t *)arg;
  setGpioPins(data->enabledGpioMask, findAllPinOns(data));
}

//############################
// lua bindings

static int lpwm2_open(lua_State *L)
{
  moduleData = os_malloc(sizeof(pwm2_module_data_t));
  moduleData->interruptData.enabledGpioMask = 0;
  moduleData->setupData.interruptTimerCPUTicks = 0;
  moduleData->setupData.interruptTimerTicks = 0;
  moduleData->setupData.isStarted = false;
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    moduleData->interruptData.pin[i].currentInterruptCounter = 0;
    moduleData->interruptData.pin[i].gpioMask = 0;
    moduleData->interruptData.pin[i].offInterruptCounter = 0;
    moduleData->interruptData.pin[i].pulseInterruptCcounter = 0;
    moduleData->setupData.pin[i].divisableFrequency = 0;
    moduleData->setupData.pin[i].duty = 0;
    moduleData->setupData.pin[i].frequencyDivisor = 0;
    moduleData->setupData.pin[i].pulseResolutions = 0;
    moduleData->setupData.pin[i].resolutionCPUTicks = 0;
    moduleData->setupData.pin[i].resolutionInterruptCounterMultiplier = 0;
  }
  return 0;
}

static int lpwm2_print_setup(lua_State *L)
{
  ets_printf("pwm2 : isStarted %u\n", moduleData->setupData.isStarted);
  ets_printf("pwm2 : interruptTimerCPUTicks %u\n", moduleData->setupData.interruptTimerCPUTicks);
  ets_printf("pwm2 : interruptTimerTicks %u\n", moduleData->setupData.interruptTimerTicks);
  ets_printf("pin, duty, pulse, resolutionAsCPU, resolutionIntrCntMultiplier, divisableFreq, freqDivisor\n");
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      ets_printf("%u, %u, %u, %u, %u, %u, %u\n",
                 i,
                 moduleData->setupData.pin[i].duty,
                 moduleData->setupData.pin[i].pulseResolutions,
                 moduleData->setupData.pin[i].resolutionCPUTicks,
                 moduleData->setupData.pin[i].resolutionInterruptCounterMultiplier,
                 moduleData->setupData.pin[i].divisableFrequency,
                 moduleData->setupData.pin[i].frequencyDivisor);
    }
  }
  ets_printf("pwm2 : enabledGpioMask %X\n", moduleData->interruptData.enabledGpioMask);
  ets_printf("pin, pulseAsInterruptCnt, pinOffWhenIntrCnt, currentintrCnt, gpioMask\n");
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      ets_printf("%u, %u, %u, %u, %X\n",
                 i,
                 moduleData->interruptData.pin[i].pulseInterruptCcounter,
                 moduleData->interruptData.pin[i].offInterruptCounter,
                 moduleData->interruptData.pin[i].currentInterruptCounter,
                 moduleData->interruptData.pin[i].gpioMask);
    }
  }
}

static uint64_t enduserFreqToCPUTicks(const uint64_t divisableFreq, const uint64_t freqDivisor, const uint64_t resolution)
{
  return (getCPUTicksPerSec() / (freqDivisor * resolution)) * divisableFreq;
}

static uint16_t getPinGpioMask(uint8_t pin)
{
  return 1 << GPIO_ID_PIN(pin_num[pin]);
}

static void set_duty(pwm2_module_data_t *moduleData, const uint8_t pin, const uint32_t duty)
{
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
    const uint32_t initDuty)
{
  moduleData->setupData.pin[pin].pulseResolutions = resolution;
  moduleData->setupData.pin[pin].divisableFrequency = divisableFreq;
  moduleData->setupData.pin[pin].frequencyDivisor = freqDivisor;
  moduleData->setupData.pin[pin].resolutionCPUTicks = enduserFreqToCPUTicks(divisableFreq, freqDivisor, resolution);
  moduleData->interruptData.pin[pin].gpioMask = getPinGpioMask(pin);
  reCalculateCommonToAllPinsData(moduleData);
  set_duty(moduleData, pin, initDuty);
}

static int lpwm2_setup_pin_common(lua_State *L, const bool isFreqHz)
{
  if (moduleData->setupData.isStarted)
  {
    return luaL_error(L, "pwm2 : already started, stop it before setting up pins.");
  }
  int pos = 0;
  while (true)
  {
    LUA_INTEGER pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1)
    {
      break;
    }
    if (pin < 1 || pin > GPIO_PIN_NUM)
    {
      return luaL_error(L, "pwm2 : expected 0 < pin < %d but got instead %d", GPIO_PIN_NUM, pin);
    }
    LUA_INTEGER freq = luaL_optinteger(L, ++pos, 0);
    if (freq < 1)
    {
      return luaL_error(L, "pwm2 : for pin=%d expected frequency > 0 but got %d", pin, freq);
    }
    LUA_INTEGER resolution = luaL_optinteger(L, ++pos, 0);
    if (resolution < 0)
    {
      return luaL_error(L, "pwm2 : for pin=%d expected pulse width resolution > 0 but got %d", pin, resolution);
    }
    LUA_INTEGER initDuty = luaL_optinteger(L, ++pos, 0);
    if (initDuty < 0 || initDuty > resolution)
    {
      return luaL_error(L, "pwm2 : for pin=%d expected 0 <= initial duty <= %d but got %d", pin, resolution, initDuty);
    }
    LUA_INTEGER freqFractions = luaL_optinteger(L, ++pos, 0);
    if (freqFractions < 0)
    {
      return luaL_error(L, "pwm2 : for pin=%d expected positive frequency fractions but got %d", pin, freqFractions);
    }
    freqFractions = (freqFractions == 0) ? 1 : freqFractions;
    if (isFreqHz)
    {
      setup_pin(moduleData, pin, freqFractions, freq, resolution, initDuty);
    }
    else
    {
      setup_pin(moduleData, pin, freq, freqFractions, resolution, initDuty);
    }
  }
  return 0;
}

static int lpwm2_setup_pin_hz(lua_State *L)
{
  return lpwm2_setup_pin_common(L, true);
}

static int lpwm2_setup_pin_sec(lua_State *L)
{
  return lpwm2_setup_pin_common(L, false);
}

static int lpwm2_set_duty(lua_State *L)
{
  int pos = 0;
  while (true)
  {
    LUA_INTEGER pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1)
    {
      break;
    }
    if (pin < 1 || pin > GPIO_PIN_NUM)
    {
      return luaL_error(L, "pwm2 : expected 0 < pin < %d but got instead %d", GPIO_PIN_NUM, pin);
    }
    LUA_INTEGER duty = luaL_optinteger(L, ++pos, -1);
    if (duty < 0 || duty > moduleData->setupData.pin[pin].pulseResolutions)
    {
      return luaL_error(L, "pwm2 : for pin=%d expected 0 <= initial duty <= %d but got %d", pin, moduleData->setupData.pin[pin].pulseResolutions, duty);
    }
    set_duty(moduleData, pin, duty);
  }
  return 0;
}

static int lpwm2_release_pin(lua_State *L)
{
  int pos = 0;
  while (true)
  {
    LUA_INTEGER pin = luaL_optinteger(L, ++pos, -1);
    if (pin == -1)
    {
      break;
    }
    if (pin < 1 || pin > GPIO_PIN_NUM)
    {
      return luaL_error(L, "pwm2 : expected 0 < pin < %d but got instead %d", GPIO_PIN_NUM, pin);
    }
    moduleData->setupData.pin[pin].pulseResolutions = 0;
    moduleData->interruptData.pin[pin].gpioMask = 0;
  }
  return 0;
}

static int lpwm2_stop(lua_State *L)
{
  if (!moduleData->setupData.isStarted)
  {
    return 0;
  }
  platform_hw_timer_close(PWM2_TMR_OWNER);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, moduleData->interruptData.enabledGpioMask); // clear pins of being gpio output
  moduleData->setupData.isStarted = false;
  return 0;
}

static void configureAllPinsAsGpioOutput(pwm2_module_data_t *moduleData)
{
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      PIN_FUNC_SELECT(pin_mux[i], pin_func[i]); // set pin as gpio
      PIN_PULLUP_EN(pin_mux[i]);                // set pin pullup on
    }
  }
}

static void resetPinCounters(pwm2_module_data_t *moduleData)
{
  for (int i = 1; i < GPIO_PIN_NUM; i++)
  {
    if (moduleData->setupData.pin[i].pulseResolutions > 0)
    {
      moduleData->interruptData.pin[i].currentInterruptCounter = 0;
    }
  }
}

static int lpwm2_start(lua_State *L)
{
  if (moduleData->setupData.isStarted)
  {
    return 0;
  }
  if (!platform_hw_timer_init(PWM2_TMR_OWNER, FRC1_SOURCE, TRUE))
  {
    return luaL_error(L, "pwm2: currently platform timer1 is being used by another module.\n");
  }
  configureAllPinsAsGpioOutput(moduleData);
  resetPinCounters(moduleData);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, moduleData->interruptData.enabledGpioMask); // set pins as gpio output
  moduleData->setupData.isStarted = true;
  platform_hw_timer_set_func(PWM2_TMR_OWNER, timerInterruptHandler, (os_param_t)&moduleData->interruptData);
  platform_hw_timer_arm_ticks(PWM2_TMR_OWNER, moduleData->setupData.interruptTimerTicks);
  return 0;
}

// #######################################
// #######################################
// #######################################
// TESTING and TIMING

// static inline uint32_t getCpuTicks()
// {
//   register uint32_t cycles;
//   asm volatile("rsr %0,ccount"
//                : "=a"(cycles));
//   return cycles;
// }

// static uint32_t
// __repeat_cb(int cnt, void (*callback)())
// {
//   while (cnt-- > 0)
//   {
//     callback();
//   }
// }

// static uint32_t
// __measure(void (*callback)())
// {
//   const int cnt = 1000;
//   int32_t btime, etime;

//   btime = getCpuTicks();
//   __repeat_cb(cnt, callback);
//   etime = getCpuTicks();
//   return ((uint32_t)(etime - btime)) / cnt;
// }

// static void __timing_intr()
// {
//   timerInterruptHandler((os_param_t)&moduleData->interruptData);
// }

// static int lpwm2_timeit(lua_State *L)
// {
//   for (int i = 1; i < GPIO_PIN_NUM; i++)
//   {
//     setup_pin(moduleData, i, 1, 1000, 2, 1);
//     resetPinCounters(moduleData);
//     ets_printf("    with pins %d: %u ticks\n",
//                i,
//                __measure(__timing_intr));
//   }
//   return 0;
// }

// Module function map
static const LUA_REG_TYPE pwm2_map[] = {
    {LSTRKEY("setup_pin_hz"), LFUNCVAL(lpwm2_setup_pin_hz)},
    {LSTRKEY("setup_pin_sec"), LFUNCVAL(lpwm2_setup_pin_sec)},
    {LSTRKEY("release_pin"), LFUNCVAL(lpwm2_release_pin)},
    {LSTRKEY("print_setup"), LFUNCVAL(lpwm2_print_setup)},
    {LSTRKEY("start"), LFUNCVAL(lpwm2_start)},
    {LSTRKEY("stop"), LFUNCVAL(lpwm2_stop)},
    {LSTRKEY("set_duty"), LFUNCVAL(lpwm2_set_duty)},
    // {LSTRKEY("time_it"), LFUNCVAL(lpwm2_timeit)},
    {LNILKEY, LNILVAL}};

NODEMCU_MODULE(PWM2, "pwm2", pwm2_map, lpwm2_open);
