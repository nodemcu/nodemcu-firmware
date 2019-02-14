#ifndef __PWM2_H__
#define __PWM2_H__

#include "c_types.h"

/* Set the following three defines to your needs */

#ifndef SDK_PWM2_PERIOD_COMPAT_MODE
#define SDK_PWM2_PERIOD_COMPAT_MODE 0
#endif
#ifndef PWM2_MAX_CHANNELS
#define PWM2_MAX_CHANNELS 13
#endif
#define PWM2_DEBUG 0
#define PWM2_USE_NMI 0

/* no user servicable parts beyond this point */

/**
 * initialize internal structures, assigns initial duty values and starts the pwm.
 * 
 * @param: period: initial period (base unit 1us OR 200ns) i.e. 5000 for 1kHZ using 200ns
 * @param: duty: array of initial duty values, may be NULL, may be freed after pwm2_init (same size as pin_info_list)
 * @param: pwm2_channel_num: number of channels to use (size of pin_info_list)
 * @param: pin_info_list: array of pin_info : 3-tuples of MUX_REGISTER, MUX_VALUE and GPIO number
 */
void pwm2_init(uint32_t period, uint32_t *duty, uint32_t pwm2_channel_num, uint32_t (*pin_info_list)[3]);

/**
 * starts pwm if not started already.
 * takes new duty settings in use.
 */
void pwm2_start(void);

/**
 * assign a new duty value to given channel (see pwm2_init for channels)
 * @param duty to assign
 * @param channel to assign to (this is the index of pin_info_list passed to pwm2_init)
 */
void pwm2_set_duty(uint32_t duty, uint8_t channel);

/**
 * gets assigned duty for given channel
 * @param channel (this is the index of pin_info_list passed to pwm2_init)
 */
uint32_t pwm2_get_duty(uint8_t channel);

void pwm2_set_period(uint32_t period);

#endif

