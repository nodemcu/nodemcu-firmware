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

#include "user_interface.h"
#include "driver/pwm.h"

// #define PWM_DBG os_printf
#define PWM_DBG

LOCAL struct pwm_single_param pwm_single_toggle[2][PWM_CHANNEL + 1];
LOCAL struct pwm_single_param *pwm_single;

LOCAL struct pwm_param pwm;

// LOCAL uint8 pwm_out_io_num[PWM_CHANNEL] = {PWM_0_OUT_IO_NUM, PWM_1_OUT_IO_NUM, PWM_2_OUT_IO_NUM};
LOCAL int8 pwm_out_io_num[PWM_CHANNEL] = {-1, -1, -1, -1, -1, -1};

LOCAL uint8 pwm_channel_toggle[2];
LOCAL uint8 *pwm_channel;

LOCAL uint8 pwm_toggle = 1;
LOCAL uint8 pwm_timer_down = 1;

LOCAL uint8 pwm_current_channel = 0;

LOCAL uint16 pwm_gpio = 0;

LOCAL uint8 pwm_channel_num = 0;

//XXX: 0xffffffff/(80000000/16)=35A
#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)

//FRC1
#define FRC1_ENABLE_TIMER  BIT7

typedef enum {
    DIVDED_BY_1 = 0,
    DIVDED_BY_16 = 4,
    DIVDED_BY_256 = 8,
} TIMER_PREDIVED_MODE;

typedef enum {
    TM_LEVEL_INT = 1,
    TM_EDGE_INT   = 0,
} TIMER_INT_MODE;

LOCAL void ICACHE_FLASH_ATTR
pwm_insert_sort(struct pwm_single_param pwm[], uint8 n)
{
    uint8 i;

    for (i = 1; i < n; i++) {
        if (pwm[i].h_time < pwm[i - 1].h_time) {
            int8 j = i - 1;
            struct pwm_single_param tmp;

            os_memcpy(&tmp, &pwm[i], sizeof(struct pwm_single_param));
            os_memcpy(&pwm[i], &pwm[i - 1], sizeof(struct pwm_single_param));

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

LOCAL volatile uint8 critical = 0;

#define LOCK_PWM(c)  do {                       \
    while( (c)==1 );                            \
    (c) = 1;                                    \
} while (0)

#define UNLOCK_PWM(c) do {                      \
    (c) = 0;                                    \
} while (0)

void ICACHE_FLASH_ATTR
pwm_start(void)
{
    uint8 i, j;
    PWM_DBG("--Function pwm_start() is called\n");
    PWM_DBG("pwm_gpio:%x,pwm_channel_num:%d\n",pwm_gpio,pwm_channel_num);
    PWM_DBG("pwm_out_io_num[0]:%d,[1]:%d,[2]:%d\n",pwm_out_io_num[0],pwm_out_io_num[1],pwm_out_io_num[2]);
    PWM_DBG("pwm.period:%d,pwm.duty[0]:%d,[1]:%d,[2]:%d\n",pwm.period,pwm.duty[0],pwm.duty[1],pwm.duty[2]);

    LOCK_PWM(critical);   // enter critical

    struct pwm_single_param *local_single = pwm_single_toggle[pwm_toggle ^ 0x01];
    uint8 *local_channel = &pwm_channel_toggle[pwm_toggle ^ 0x01];

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
    // step 3: combine same duty channels
    for (i = pwm_channel_num; i > 0; i--) {
        if (local_single[i].h_time == local_single[i - 1].h_time) {
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

    // if timer is down, need to set gpio and start timer
    if (pwm_timer_down == 1) {
        pwm_channel = local_channel;
        pwm_single = local_single;
        // start
        gpio_output_set(local_single[0].gpio_set, local_single[0].gpio_clear, pwm_gpio, 0);

        // yeah, if all channels' duty is 0 or 255, don't need to start timer, otherwise start...
        if (*local_channel != 1) {
            pwm_timer_down = 0;
            RTC_REG_WRITE(FRC1_LOAD_ADDRESS, local_single[0].h_time);
        }
    }

    if (pwm_toggle == 1) {
        pwm_toggle = 0;
    } else {
        pwm_toggle = 1;
    }

    UNLOCK_PWM(critical);   // leave critical
    PWM_DBG("3channel:%d,single[0]:%d,[1]:%d,[2]:%d,[3]:%d\n",*local_channel,local_single[0].h_time,local_single[1].h_time,local_single[2].h_time,local_single[3].h_time);
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

    LOCK_PWM(critical);   // enter critical
    if (duty < 1) {
        pwm.duty[channel] = 0;
    } else if (duty >= PWM_DEPTH) {
    	pwm.duty[channel] = PWM_DEPTH;
    } else {
    	pwm.duty[channel] = duty;
    }
    UNLOCK_PWM(critical);   // leave critical
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
    LOCK_PWM(critical);   // enter critical
    if (freq > PWM_FREQ_MAX) {
        pwm.freq = PWM_FREQ_MAX;
    } else if (freq < 1) {
        pwm.freq = 1;
    } else {
        pwm.freq = freq;
    }

    pwm.period = PWM_1S / pwm.freq;
    UNLOCK_PWM(critical);   // leave critical
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
pwm_tim1_intr_handler(void)
{
    uint8 local_toggle = pwm_toggle;                        // pwm_toggle may change outside
    RTC_CLR_REG_MASK(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);

    if (pwm_current_channel >= (*pwm_channel - 1)) {        // *pwm_channel may change outside
        pwm_single = pwm_single_toggle[local_toggle];
        pwm_channel = &pwm_channel_toggle[local_toggle];

        gpio_output_set(pwm_single[*pwm_channel - 1].gpio_set,
                        pwm_single[*pwm_channel - 1].gpio_clear,
                        pwm_gpio,
                        0);

        pwm_current_channel = 0;

        if (*pwm_channel != 1) {
            RTC_REG_WRITE(FRC1_LOAD_ADDRESS, pwm_single[pwm_current_channel].h_time);
        } else {
            pwm_timer_down = 1;
        }
    } else {
        gpio_output_set(pwm_single[pwm_current_channel].gpio_set,
                        pwm_single[pwm_current_channel].gpio_clear,
                        pwm_gpio, 0);

        pwm_current_channel++;
        RTC_REG_WRITE(FRC1_LOAD_ADDRESS, pwm_single[pwm_current_channel].h_time);
    }
}

/******************************************************************************
 * FunctionName : pwm_init
 * Description  : pwm gpio, params and timer initialization
 * Parameters   : uint16 freq : pwm freq param
 *                uint16 *duty : each channel's duty
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
pwm_init(uint16 freq, uint16 *duty)
{
    uint8 i;

    RTC_REG_WRITE(FRC1_CTRL_ADDRESS,  //FRC2_AUTO_RELOAD|
                  DIVDED_BY_16
                  | FRC1_ENABLE_TIMER
                  | TM_EDGE_INT);
    RTC_REG_WRITE(FRC1_LOAD_ADDRESS, 0);

    // PIN_FUNC_SELECT(PWM_0_OUT_IO_MUX, PWM_0_OUT_IO_FUNC);
    // PIN_FUNC_SELECT(PWM_1_OUT_IO_MUX, PWM_1_OUT_IO_FUNC);
    // PIN_FUNC_SELECT(PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_0_OUT_IO_NUM), 0);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_1_OUT_IO_NUM), 0);
    // GPIO_OUTPUT_SET(GPIO_ID_PIN(PWM_2_OUT_IO_NUM), 0);
    
    for (i = 0; i < PWM_CHANNEL; i++) {
        // pwm_gpio |= (1 << pwm_out_io_num[i]);
        pwm_gpio = 0;
        pwm.duty[0] = 0;
    }

    pwm_set_freq(500, 0);
    // pwm_set_freq_duty(freq, duty);

    pwm_start();
    
    ETS_FRC_TIMER1_INTR_ATTACH(pwm_tim1_intr_handler, NULL);
    TM1_EDGE_INT_ENABLE();
    ETS_FRC1_INTR_ENABLE();
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
            LOCK_PWM(critical);   // enter critical
            pwm_out_io_num[i] = channel;
            pwm.duty[i] = 0;
            pwm_gpio |= (1 << pin_num[channel]);
            PIN_FUNC_SELECT(pin_mux[channel], pin_func[channel]);
            GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel])), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[channel]))) & (~ GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE))); //disable open drain;
            pwm_channel_num++;
            UNLOCK_PWM(critical);   // leave critical
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
            LOCK_PWM(critical);   // enter critical
            pwm_out_io_num[i] = -1;
            pwm_gpio &= ~(1 << pin_num[channel]);   //clear the bit
            for(j=i;j<pwm_channel_num-1;j++){
                pwm_out_io_num[j] = pwm_out_io_num[j+1];
                pwm.duty[j] = pwm.duty[j+1];
            }
            pwm_out_io_num[pwm_channel_num-1] = -1;
            pwm.duty[pwm_channel_num-1] = 0;
            pwm_channel_num--;
            UNLOCK_PWM(critical);   // leave critical
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
