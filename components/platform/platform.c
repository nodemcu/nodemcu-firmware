#include "platform.h"
#include "driver/uart.h"

int platform_init (void)
{
  return PLATFORM_OK;
}

// ****************************************************************************
// UART

uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits )
{
  UART_ConfigTypeDef cfg;
  switch( baud )
  {
    case BIT_RATE_300:
    case BIT_RATE_600:
    case BIT_RATE_1200:
    case BIT_RATE_2400:
    case BIT_RATE_4800:
    case BIT_RATE_9600:
    case BIT_RATE_19200:
    case BIT_RATE_38400:
    case BIT_RATE_57600:
    case BIT_RATE_74880:
    case BIT_RATE_115200:
    case BIT_RATE_230400:
    case BIT_RATE_460800:
    case BIT_RATE_921600:
    case BIT_RATE_1843200:
    case BIT_RATE_3686400:
      cfg.baud_rate = baud;
      break;
    default:
      cfg.baud_rate = BIT_RATE_9600;
      break;
  }

  switch( databits )
  {
    case 5:
      cfg.data_bits = UART_WordLength_5b;
      break;
    case 6:
      cfg.data_bits = UART_WordLength_6b;
      break;
    case 7:
      cfg.data_bits = UART_WordLength_7b;
      break;
    case 8:
    default:
      cfg.data_bits = UART_WordLength_8b;
      break;
  }

  switch (stopbits)
  {
    case PLATFORM_UART_STOPBITS_1_5:
      cfg.stop_bits = USART_StopBits_1_5;
      break;
    case PLATFORM_UART_STOPBITS_2:
      cfg.stop_bits = USART_StopBits_2;
      break;
    default:
      cfg.stop_bits = USART_StopBits_1;
      break;
  }

  switch (parity)
  {
    case PLATFORM_UART_PARITY_EVEN:
      cfg.parity = USART_Parity_Even;
      break;
    case PLATFORM_UART_PARITY_ODD:
      cfg.parity = USART_Parity_Odd;
      break;
    default:
      cfg.parity = USART_Parity_None;
      break;
  }

  UART_ParamConfig (id, &cfg);
  return baud;
}


// if set=1, then alternate serial output pins are used. (15=rx, 13=tx)
void platform_uart_alt( int set )
{
    uart0_alt( set );
    return;
}


void platform_uart_send( unsigned id, uint8_t data )
{
  uart_tx_one_char(id, data);
}
