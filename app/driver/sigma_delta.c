
#include "platform.h"
#include "driver/sigma_delta.h"


void sigma_delta_setup( void )
{
    GPIO_REG_WRITE(GPIO_SIGMA_DELTA,
                   GPIO_SIGMA_DELTA_SET(GPIO_SIGMA_DELTA_ENABLE) |
                   GPIO_SIGMA_DELTA_TARGET_SET(0x00) |
                   GPIO_SIGMA_DELTA_PRESCALE_SET(0x00));
}

void sigma_delta_stop( void )
{
    GPIO_REG_WRITE(GPIO_SIGMA_DELTA,
                   GPIO_SIGMA_DELTA_SET(GPIO_SIGMA_DELTA_DISABLE) |
                   GPIO_SIGMA_DELTA_TARGET_SET(0x00) |
                   GPIO_SIGMA_DELTA_PRESCALE_SET(0x00) );
}

void ICACHE_RAM_ATTR sigma_delta_set_prescale_target( sint16 prescale, sint16 target )
{
    uint32_t prescale_mask, target_mask;

    prescale_mask = prescale >= 0 ? GPIO_SIGMA_DELTA_PRESCALE_MASK : 0x00;
    target_mask   = target   >= 0 ? GPIO_SIGMA_DELTA_TARGET_MASK   : 0x00;

    // set prescale and target with one register access to avoid glitches
    GPIO_REG_WRITE(GPIO_SIGMA_DELTA,
                   (GPIO_REG_READ(GPIO_SIGMA_DELTA) & ~(prescale_mask | target_mask)) |
                   (GPIO_SIGMA_DELTA_PRESCALE_SET(prescale) & prescale_mask) |
                   (GPIO_SIGMA_DELTA_TARGET_SET(target) & target_mask));
}
