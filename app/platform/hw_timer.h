#ifndef _HW_TIMER_H
#define _HW_TIMER_H

#if APB_CLK_FREQ == 80 * 1000000
// 80 MHz divided by 16 is 5 MHz count rate.
#define US_TO_RTC_TIMER_TICKS(t)      ((t) * 5)
#else
#define US_TO_RTC_TIMER_TICKS(t)          \
    ((t) ?                                   \
     (((t) > 0x35A) ?                   \
      (((t)>>2) * ((APB_CLK_FREQ>>4)/250000) + ((t)&0x3) * ((APB_CLK_FREQ>>4)/1000000))  :    \
      (((t) *(APB_CLK_FREQ>>4)) / 1000000)) :    \
     0)
#endif

typedef enum {
    FRC1_SOURCE = 0,
    NMI_SOURCE = 1,
} FRC1_TIMER_SOURCE_TYPE;

bool ICACHE_RAM_ATTR platform_hw_timer_arm_ticks(os_param_t owner, uint32_t ticks);

bool ICACHE_RAM_ATTR platform_hw_timer_arm_us(os_param_t owner, uint32_t microseconds);

bool platform_hw_timer_set_func(os_param_t owner, void (* user_hw_timer_cb_set)(os_param_t), os_param_t arg);

bool platform_hw_timer_init(os_param_t owner, FRC1_TIMER_SOURCE_TYPE source_type, bool autoload);

bool ICACHE_RAM_ATTR platform_hw_timer_close(os_param_t owner);

uint32_t ICACHE_RAM_ATTR platform_hw_timer_get_delay_ticks(os_param_t owner);

#endif

