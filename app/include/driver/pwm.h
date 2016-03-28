#ifndef __PWM_H__
#define __PWM_H__

#define PWM_CHANNEL 6

struct pwm_single_param {
	uint16 gpio_set;
	uint16 gpio_clear;
    uint32 h_time;
};

struct pwm_param {
    uint32 period;
    uint16 freq;
    uint16  duty[PWM_CHANNEL];
};

#define PWM_DEPTH 1023
#define PWM_FREQ_MAX 1000

#define PWM_1S 1000000

// #define PWM_0_OUT_IO_MUX PERIPHS_IO_MUX_MTMS_U
// #define PWM_0_OUT_IO_NUM 14
// #define PWM_0_OUT_IO_FUNC  FUNC_GPIO14

// #define PWM_1_OUT_IO_MUX PERIPHS_IO_MUX_MTDI_U
// #define PWM_1_OUT_IO_NUM 12
// #define PWM_1_OUT_IO_FUNC  FUNC_GPIO12

// #define PWM_2_OUT_IO_MUX PERIPHS_IO_MUX_MTCK_U
// #define PWM_2_OUT_IO_NUM 13
// #define PWM_2_OUT_IO_FUNC  FUNC_GPIO13

void pwm_init(uint16 freq, uint16 *duty);
bool pwm_start(void);

void pwm_set_duty(uint16 duty, uint8 channel);
uint16 pwm_get_duty(uint8 channel);
void pwm_set_freq(uint16 freq, uint8 channel);
uint16 pwm_get_freq(uint8 channel);
bool pwm_add(uint8 channel);
bool pwm_delete(uint8 channel);
bool pwm_exist(uint8 channel);
#endif

