/*
 * Software PWM using soft-interrupt timer1.
 * Supports higher frequencies compared to Espressif provided one.
 *
 * Nikolay Fiykov
 */

#ifndef __PWM2_H__
#define __PWM2_H__

#include <stdint.h>
#include "pin_map.h"

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

// driver's public API

void pwm2_init();
pwm2_module_data_t *pwm2_get_module_data();
bool pwm2_is_pin_setup(const uint8_t pin);
void pwm2_setup_pin(
    const uint8_t pin,
    const uint32_t divisableFreq,
    const uint32_t freqDivisor,
    const uint32_t resolution,
    const uint32_t initDuty
    );
void pwm2_release_pin(const uint8_t pin);
void pwm2_stop();
bool pwm2_start();
bool pwm2_is_started();
void pwm2_set_duty(const uint8_t pin, const uint32_t duty);

#endif
