/*
 * Software PWM using soft-interrupt timer1.
 * Supports higher frequencies compared to Espressif provided one.
 *
 * Nikolay Fiykov
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mem.h"
#include "pin_map.h"
#include "platform.h"
#include "hw_timer.h"
#include "driver/pwm2.h"
#include "user_interface.h"

#define PWM2_TMR_MAGIC_80MHZ 16
#define PWM2_TMR_MAGIC_160MHZ 32

// module vars, lazy initialized, allocated only if pwm2 is being used

static pwm2_module_data_t *moduleData = NULL;

//############################
// tools

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
// driver's public API

void pwm2_init() {
  moduleData = os_malloc(sizeof(pwm2_module_data_t));
  memset(moduleData, 0, sizeof(*moduleData));
}

pwm2_module_data_t *pwm2_get_module_data() {
    return moduleData;
}

bool pwm2_is_pin_setup(const uint8_t pin) {
  return isPinSetup(moduleData, pin);
}

void pwm2_setup_pin(
    const uint8_t pin,
    const uint32_t divisableFreq,
    const uint32_t freqDivisor,
    const uint32_t resolution,
    const uint32_t initDuty
    )
{
  moduleData->setupData.pin[pin].pulseResolutions = resolution;
  moduleData->setupData.pin[pin].divisableFrequency = divisableFreq;
  moduleData->setupData.pin[pin].frequencyDivisor = freqDivisor;
  moduleData->setupData.pin[pin].resolutionCPUTicks = enduserFreqToCPUTicks(divisableFreq, freqDivisor, resolution);
  moduleData->interruptData.pin[pin].gpioMask = getPinGpioMask(pin);
  reCalculateCommonToAllPinsData(moduleData);
  set_duty(moduleData, pin, initDuty);
}

void pwm2_release_pin(const uint8_t pin) {
  moduleData->setupData.pin[pin].pulseResolutions = 0;
  moduleData->interruptData.pin[pin].gpioMask = 0;
}

void pwm2_stop() {
  if (!moduleData->setupData.isStarted) {
    return;
  }
  platform_hw_timer_close_exclusive();
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, moduleData->interruptData.enabledGpioMask);  // clear pins of being gpio output
  moduleData->setupData.isStarted = false;
}

bool pwm2_start() {
  if (moduleData->setupData.isStarted) {
    return true;
  }
  if (!platform_hw_timer_init_exclusive(FRC1_SOURCE, TRUE, timerInterruptHandler, (os_param_t)&moduleData->interruptData, (void (*)(void))NULL)) {
    return false;
  }
  configureAllPinsAsGpioOutput(moduleData);
  resetPinCounters(moduleData);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, moduleData->interruptData.enabledGpioMask);  // set pins as gpio output
  moduleData->setupData.isStarted = true;
  platform_hw_timer_arm_ticks_exclusive(moduleData->setupData.interruptTimerTicks);
  return true;
}

bool pwm2_is_started() {
  return moduleData->setupData.isStarted;
}

void pwm2_set_duty(const uint8_t pin, const uint32_t duty) {
  set_duty(moduleData, pin, duty);
}
