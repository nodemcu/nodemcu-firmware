#include "platform.h"
#include "driver/console.h"
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
