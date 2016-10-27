/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: pwm.c
 *
 * Description: pwm driver
 *
 * Modification history:
 *     2014/5/1, v1.0 create this file.
*******************************************************************************/
#include "platform.h"

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "gpio.h"
#include "hw_timer.h"

#include "user_interface.h"
#include "driver/pwm.h"

// #define PWM_DBG os_printf
#define PWM_DBG

// Enabling the next line will cause the interrupt handler to toggle
// this output pin during processing so that the timing is obvious
//
// #define PWM_DBG_PIN	13   // GPIO7

#ifdef PWM_DBG_PIN
#define PWM_DBG_PIN_HIGH()    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << PWM_DBG_PIN)
#define PWM_DBG_PIN_LOW()    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << PWM_DBG_PIN)
#else
#define PWM_DBG_PIN_HIGH()
#define PWM_DBG_PIN_LOW()
#endif

LOCAL struct pwm_single_param pwm_single_toggle[2][PWM_CHANNEL + 1];
LOCAL struct pwm_single_param *pwm_single;

LOCAL struct pwm_param pwm;

// LOCAL uint8 pwm_out_io_num[PWM_CHANNEL] = {PWM_0_OUT_IO_NUM, PWM_1_OUT_IO_NUM, PWM_2_OUT_IO_NUM};
LOCAL int8 pwm_out_io_num[PWM_CHANNEL] = {-1, -1, -1, -1, -1, -1};

LOCAL uint8 pwm_channel_toggle[2];
LOCAL uint8 *pwm_channel;

// Toggle flips between 1 and 0 when we make updates so that the interrupt code
// cn switch cleanly between the two states. The cinterrupt handler uses either
// the pwm_single_toggle[0] or pwm_single_toggle[1]
// pwm_toggle indicates which state should be used on the *next* timer interrupt 
// freq boundary.
LOCAL uint8 pwm_toggle = 1;
LOCAL volatile uint8 pwm_current_toggle = 1;
LOCAL uint8 pwm_timer_down = 1;

LOCAL uint8 pwm_current_channel = 0;

LOCAL uint16 pwm_gpio = 0;

LOCAL uint8 pwm_channel_num = 0;

LOCAL void ICACHE_RAM_ATTR pwm_tim1_intr_handler(os_param_t p);
#define TIMER_OWNER ((os_param_t) 'P')

LOCAL void ICACHE_FLASH_ATTR
pwm_insert_sort(struct pwm_single_param pwm[], uint8 n)
{
    uint8 i;

    for (i = 1; i < n; i++) {
        if (pwm[i].h_time < pwm[i - 1].h_time) {
            int8 j = i - 1;
            struct pwm_single_param tmp;

            os_memcpy(&tmp, &pwm[i], sizeof(struct pwm_single_param));

            while (tmp.h_time < pwm[j].h_time) {
                os_memcpy(&pwm[j + 1], &pwm[j], sizeof(struct pwm_single_param));
                j--;
                if (j < 0) {
                	break;
                }
            }

            os_memcpy(&pwm[j + 1], &tmp, sizeof(struct pwm_single_param));
        }
    }
}

// Returns FALSE if we cannot start
bool ICACHE_FLASH_ATTR
pwm_start(void)
{
    uint8 i, j;
    PWM_DBG("--Function pwm_start() is called\n");
    PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    PWM_DBG("pwm.period:%d,pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.period,pwm.duty[0],pwm.duty[1],pwm.duty[2]);

    // First we need to make sure that the interrupt handler is running
    // out of the same set of params as we expect
    while (!pwm_timer_down && pwm_toggle != pwm_current_toggle) {
      os_delay_us(100);
    }
    if (pwm_timer_down) {
      pwm_toggle = pwm_current_toggle;
    }

    uint8_t new_toggle = pwm_toggle ^ 0x01;

    struct pwm_single_param *local_single = pwm_single_toggle[new_toggle];
    uint8 *local_channel = &pwm_channel_toggle[new_toggle];

    // step 1: init PWM_CHANNEL+1 channels param
    for (i = 0; i < pwm_channel_num; i++) {
        uint32 us = pwm.period * pwm.duty[i] / PWM_DEPTH;
        local_single[i].h_time = US_TO_RTC_TIMER_TICKS(us);
        PWM_DBG("i:%d us:%d ht:%d\n",i,us,local_single[i].h_time);
        local_single[i].gpio_set = 0;
        local_single[i].gpio_clear = 1 << pin_num[pwm_out_io_num[i]];
    }

    local_single[pwm_channel_num].h_time = US_TO_RTC_TIMER_TICKS(pwm.period);
    local_single[pwm_channel_num].gpio_set = pwm_gpio;
    local_single[pwm_channel_num].gpio_clear = 0;
    PWM_DBG("i:%d period:%d ht:%d\n",pwm_channel_num,pwm.period,local_single[pwm_channel_num].h_time);
    // step 2: sort, small to big
    pwm_insert_sort(local_single, pwm_channel_num + 1);

    *local_channel = pwm_channel_num + 1;
    PWM_DBG("1channel:%d,single[0]:%d,[1]:%d,[2]:%d,[3]:%d\n",*local_channel,local_single[0].h_time,local_single[1].h_time,local_single[2].h_time,local_single[3].h_time);
    // step 3: combine same duty channels (or nearly the same duty). If there is
    // under 2 us between pwm outputs, then treat them as the same.
    for (i = pwm_channel_num; i > 0; i--) {
        if (local_single[i].h_time <= local_single[i - 1].h_time + US_TO_RTC_TIMER_TICKS(2)) {
            local_single[i - 1].gpio_set |= local_single[i].gpio_set;
            local_single[i - 1].gpio_clear |= local_single[i].gpio_clear;

            for (j = i + 1; j < *local_channel; j++) {
                os_memcpy(&local_single[j - 1], &local_single[j], sizeof(struct pwm_single_param));
            }

            (*local_channel)--;
        }
    }
    PWM_DBG("2channel:%d,single[0]:%d,[1]:%d,[2]:%d,[3]:%d\n",*local_channel,local_single[0].h_time,local_single[1].h_time,local_single[2].h_time,local_single[3].h_time);
    // step 4: cacl delt time
    for (i = *local_channel - 1; i > 0; i--) {
        local_single[i].h_time -= local_single[i - 1].h_time;
    }

    // step 5: last channel needs to clean
    local_single[*local_channel-1].gpio_clear = 0;

    // step 6: if first channel duty is 0, remove it
    if (local_single[0].h_time == 0) {
        local_single[*local_channel - 1].gpio_set &= ~local_single[0].gpio_clear;
        local_single[*local_channel - 1].gpio_clear |= local_single[0].gpio_clear;

        for (i = 1; i < *local_channel; i++) {
            os_memcpy(&local_single[i - 1], &local_single[i], sizeof(struct pwm_single_param));
        }

        (*local_channel)--;
    }

    // Make the new ones active
    pwm_toggle = new_toggle;

    // if timer is down, need to set gpio and start timer
    if (pwm_timer_down == 1) {
        pwm_channel = local_channel;
        pwm_single = local_single;
	pwm_current_toggle = pwm_toggle;
        // start
        gpio_output_set(local_single[0].gpio_set, local_single[0].gpio_clear, pwm_gpio, 0);

        // yeah, if all channels' duty is 0 or 255, don't need to start timer, otherwise start...
        if (*local_channel != 1) {
	  PWM_DBG("Need to setup timer\n");
	  if (!platform_hw_timer_init(TIMER_OWNER, FRC1_SOURCE, FALSE)) {
	    return FALSE;
	  }
	  pwm_timer_down = 0;
	  platform_hw_timer_set_func(TIMER_OWNER, pwm_tim1_intr_handler, 0);
	  platform_hw_timer_arm_ticks(TIMER_OWNER, local_single[0].h_time);
	} else {
	  PWM_DBG("Timer left idle\n");
	  platform_hw_timer_close(TIMER_OWNER);
	}
    } else {
      // ensure that all outputs are outputs
      gpio_output_set(0, 0, pwm_gpio, 0);
    }

#ifdef PWM_DBG_PIN
    // Enable as output
    gpio_output_set(0, 0, 1 << PWM_DBG_PIN, 0);
#endif

    PWM_DBG("3channel:%d,single[0]:%d,[1]:%d,[2]:%d,[3]:%d\n",*local_channel,local_single[0].h_time,local_single[1].h_time,local_single[2].h_time,local_single[3].h_time);

    return TRUE;
}

/******************************************************************************
 * FunctionName : pwm_set_duty
 * Description  : set each channel's duty params
 * Parameters   : uint8 duty    : 0 ~ PWM_DEPTH
 *                uint8 channel : channel index
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
pwm_set_duty(uint16 duty, uint8 channel)
{
    uint8 i;
    for(i=0;i<pwm_channel_num;i++){
        if(pwm_out_io_num[i] == channel){
            channel = i;
            break;
        }
    }
    if(i==pwm_channel_num)      // non found
        return;

    if (duty < 1) {
        pwm.duty[channel] = 0;
    } else if (duty >= PWM_DEPTH) {
    	pwm.duty[channel] = PWM_DEPTH;
    } else {
    	pwm.duty[channel] = duty;
    }
}

/******************************************************************************
 * FunctionName : pwm_set_freq
 * Description  : set pwm frequency
 * Parameters   : uint16 freq : 100hz typically
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
pwm_set_freq(uint16 freq, uint8 channel)
{
    if (freq > PWM_FREQ_MAX) {
        pwm.freq = PWM_FREQ_MAX;
    } else if (freq < 1) {
        pwm.freq = 1;
    } else {
        pwm.freq = freq;
    }

    pwm.period = PWM_1S / pwm.freq;
}

/******************************************************************************
 * FunctionName : pwm_set_freq_duty
 * Description  : set pwm frequency and each channel's duty
 * Parameters   : uint16 freq : 100hz typically
 *                uint16 *duty : each channel's duty
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
pwm_set_freq_duty(uint16 freq, uint16 *duty)
{
    uint8 i;

    pwm_set_freq(freq, 0);

    for (i = 0; i < PWM_CHANNEL; i++) {
        // pwm_set_duty(duty[i], i);
        if(pwm_out_io_num[i] != -1)
            pwm_set_duty(duty[i], pwm_out_io_num[i]);
    }
}

/******************************************************************************
 * FunctionName : pwm_get_duty
 * Description  : get duty of each channel
 * Parameters   : uint8 channel : channel index
 * Returns      : NONE
*******************************************************************************/
uint16 ICACHE_FLASH_ATTR
pwm_get_duty(uint8 channel)
{
    uint8 i;
    for(i=0;i<pwm_channel_num;i++){
        if(pwm_out_io_num[i] == channel){
            channel = i;
            break;
        }
    }
    if(i==pwm_channel_num)      // non found
        return 0;

    return pwm.duty[channel];
}

/******************************************************************************
 * FunctionName : pwm_get_freq
 * Description  : get pwm frequency
 * Parameters   : NONE
 * Returns      : uint16 : pwm frequency
*******************************************************************************/
uint16 ICACHE_FLASH_ATTR
pwm_get_freq(uint8 channel)
{
    return pwm.freq;
}

/******************************************************************************
 * FunctionName : pwm_period_timer
 * Description  : pwm period timer function, output high level,
 *                start each channel's high level timer
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_RAM_ATTR
pwm_tim1_intr_handler(os_param_t p)
{
  (void)p;

  PWM_DBG_PIN_HIGH();

  int offset = 0;

  while (1) {
    if (pwm_current_channel >= (*pwm_channel - 1)) {      
      pwm_single = pwm_single_toggle[pwm_toggle];
      pwm_channel = &pwm_channel_toggle[pwm_toggle];
      pwm_current_toggle = pwm_toggle;

      gpio_output_set(pwm_single[*pwm_channel - 1].gpio_set,
		      pwm_single[*pwm_channel - 1].gpio_clear,
		      0,
		      0);

      pwm_current_channel = 0;

      if (*pwm_channel == 1) {
	pwm_timer_down = 1;
	break;
      }
    } else {
      gpio_output_set(pwm_single[pwm_current_channel].gpio_set,
		      pwm_single[pwm_current_channel].gpio_clear,
		      0, 0);

      pwm_current_channel++;
    }

    int next_time = pwm_single[pwm_current_channel].h_time;
    // Delay now holds the time (in ticks) since when the last timer expiry was
    PWM_DBG_PIN_LOW();
    int delay = platform_hw_timer_get_delay_ticks(TIMER_OWNER) + 4 - offset;

    offset += next_time;

    next_time = next_time - delay;

    if (next_time > US_TO_RTC_TIMER_TICKS(4)) {
      PWM_DBG_PIN_HIGH();
      platform_hw_timer_arm_ticks(TIMER_OWNER, next_time);
      break;
    }
    PWM_DBG_PIN_HIGH();
  }

  PWM_DBG_PIN_LOW();
}

/******************************************************************************
 * FunctionName : pwm_init
 * Description  : pwm gpio, params and timer initialization
 * Parameters   : uint16 freq : pwm freq param
 *                uint16 *duty : each channel's duty
 * Returns      : void
*******************************************************************************/
void ICACHE_FLASH_ATTR
pwm_init(uint16 freq, uint16 *duty)
{
    uint8 i;

    // PIN_FUNC_SELECT(PWM_0_OUT_IO_MUX, PWM_0_OUT_IO_FUNC);
    // PIN_FUNC_SELECT(PWM_1_OUT_IO_MUX, PWM_1_OUT_IO_FUNC);
    // PIN_FUNC_SELECT(PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_0_OUT_IO_NUM), 0);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_1_OUT_IO_NUM), 0);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_2_OUT_IO_NUM), 0);
    
    for (i = 0; i < PWM_CHANNEL; i++) {
        // pwm_gpio |= (1 << pwm_out_io_num[i]);
        pwm_gpio = 0;
        pwm.duty[i] = 0;
    }

    pwm_set_freq(500, 0);
    // pwm_set_freq_duty(freq, duty);

    pwm_start();

    PWM_DBG("pwm_init returning\n");
}

bool ICACHE_FLASH_ATTR
pwm_add(uint8 channel){
    PWM_DBG("--Function pwm_add() is called. channel:%d\n", channel);
    PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i;
    for(i=0;i<PWM_CHANNEL;i++){
        if(pwm_out_io_num[i]==channel)  // already exist
            return true;
        if(pwm_out_io_num[i] == -1){ // empty exist
            pwm_out_io_num[i] = channel;
            pwm.duty[i] = 0;
            pwm_gpio |= (1 << pin_num[channel]);
            PIN_FUNC_SELECT(pin_mux[channel], pin_func[channel]);
            GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel])), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel]))) & (~ GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE))); //disable open drain;
            pwm_channel_num++;
            return true;
        }
    }
    return false;
}

bool ICACHE_FLASH_ATTR
pwm_delete(uint8 channel){
    PWM_DBG("--Function pwm_delete() is called. channel:%d\n", channel);
    PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i,j;
    for(i=0;i<pwm_channel_num;i++){
        if(pwm_out_io_num[i]==channel){  // exist
            pwm_out_io_num[i] = -1;
            pwm_gpio &= ~(1 << pin_num[channel]);   //clear the bit
            for(j=i;j<pwm_channel_num-1;j++){
                pwm_out_io_num[j] = pwm_out_io_num[j+1];
                pwm.duty[j] = pwm.duty[j+1];
            }
            pwm_out_io_num[pwm_channel_num-1] = -1;
            pwm.duty[pwm_channel_num-1] = 0;
            pwm_channel_num--;
            return true;
        }
    }
    // non found
    return true;
}

bool ICACHE_FLASH_ATTR
pwm_exist(uint8 channel){
    PWM_DBG("--Function pwm_exist() is called. channel:%d\n", channel);
    PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    PWM_DBG("pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.duty[0],pwm.duty[1],pwm.duty[2]);
    uint8 i;
    for(i=0;i<PWM_CHANNEL;i++){
        if(pwm_out_io_num[i]==channel)  // exist
            return true;
    }
    return false;
}
