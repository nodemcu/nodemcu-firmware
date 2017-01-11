#include "platform.h"
#include "driver/console.h"
#include "driver/sigmadelta.h"
#include <stdio.h>

int platform_init (void)
{
  return PLATFORM_OK;
}

// ****************************************************************************
// UART

uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits )
{
  if (id == CONSOLE_UART)
  {
    ConsoleSetup_t cfg;
    cfg.bit_rate  = baud;
    switch (databits)
    {
      case 5: cfg.data_bits = CONSOLE_NUM_BITS_5; break;
      case 6: cfg.data_bits = CONSOLE_NUM_BITS_6; break;
      case 7: cfg.data_bits = CONSOLE_NUM_BITS_7; break;
      case 8: // fall-through
      default: cfg.data_bits = CONSOLE_NUM_BITS_8; break;
    }
    switch (parity)
    {
      case PLATFORM_UART_PARITY_EVEN: cfg.parity = CONSOLE_PARITY_EVEN; break;
      case PLATFORM_UART_PARITY_ODD:  cfg.parity = CONSOLE_PARITY_ODD; break;
      default: // fall-through
      case PLATFORM_UART_PARITY_NONE: cfg.parity = CONSOLE_PARITY_NONE; break;
    }
    switch (stopbits)
    {
      default: // fall-through
      case PLATFORM_UART_STOPBITS_1:
        cfg.stop_bits = CONSOLE_STOP_BITS_1; break;
      case PLATFORM_UART_STOPBITS_1_5:
        cfg.stop_bits = CONSOLE_STOP_BITS_1_5; break;
      case PLATFORM_UART_STOPBITS_2:
        cfg.stop_bits = CONSOLE_STOP_BITS_2; break;
    }
    cfg.auto_baud = false;
    console_setup (&cfg);
    return baud;
  }
  else
  {
    printf("UART1/UART2 not yet supported\n");
    return 0;
  }
}


void platform_uart_send( unsigned id, uint8_t data )
{
  if (id == CONSOLE_UART)
    putchar (data);
}

// *****************************************************************************
// Sigma-Delta platform interface

static gpio_num_t platform_sigma_delta_channel2gpio[SIGMADELTA_CHANNEL_MAX];

int platform_sigma_delta_exists( unsigned channel ) {
  return (channel < SIGMADELTA_CHANNEL_MAX);
}

uint8_t platform_sigma_delta_setup( uint8_t channel, uint8_t gpio_num )
{
#if 0
  // signal generator can't be stopped this way
  // stop signal generator
  if (ESP_OK != sigmadelta_set_prescale( channel, 0 ))
    return 0;
#endif

  // note channel to gpio assignment
  platform_sigma_delta_channel2gpio[channel] = gpio_num;

  return ESP_OK == sigmadelta_set_pin( channel, gpio_num ) ? 1 : 0;
}

uint8_t platform_sigma_delta_close( uint8_t channel )
{
#if 0
  // Note: signal generator can't be stopped this way
  // stop signal generator
  if (ESP_OK != sigmadelta_set_prescale( channel, 0 ))
    return 0;
#endif

  gpio_set_level( platform_sigma_delta_channel2gpio[channel], 1 );
  gpio_config_t cfg;
  // force pin back to GPIO
  cfg.intr_type = GPIO_INTR_DISABLE;
  cfg.mode = GPIO_MODE_OUTPUT;  // essential to switch IO matrix to GPIO
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  cfg.pin_bit_mask = 1 << platform_sigma_delta_channel2gpio[channel];
  if (ESP_OK != gpio_config( &cfg ))
    return 0;

  // and set it finally to input with pull-up enabled
  cfg.mode = GPIO_MODE_INPUT;

  return ESP_OK == gpio_config( &cfg ) ? 1 : 0;
}

#if 0
// PWM emulation not possible, code kept for future reference
uint8_t platform_sigma_delta_set_pwmduty( uint8_t channel, uint8_t duty )
{
  uint8_t target = 0, prescale = 0;

  target = duty > 128 ? 256 - duty : duty;
  prescale = target == 0 ? 0 : target-1;

  //freq = 80000 (khz) /256 /duty_target * (prescale+1)
  if (ESP_OK != sigmadelta_set_prescale( channel, prescale ))
    return 0;
  if (ESP_OK != sigmadelta_set_duty( channel, duty-128 ))
    return 0;

  return 1;
}
#endif

uint8_t platform_sigma_delta_set_prescale( uint8_t channel, uint8_t prescale )
{
  return ESP_OK == sigmadelta_set_prescale( channel, prescale ) ? 1 : 0;
}

uint8_t IRAM_ATTR platform_sigma_delta_set_duty( uint8_t channel, int8_t duty )
{
  return ESP_OK == sigmadelta_set_duty( channel, duty ) ? 1 : 0;
}
