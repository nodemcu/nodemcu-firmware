/*
 * Filename: softuart2.c
 * Author: sjp27
 * Date: 01/03/2021
 * Description: softuart2 driver.
 */

#include "platform.h"
#include "mem.h"
#include "hw_timer.h"
#include "driver/softuart2.h"

#define SOFTUART2_TMR_DIVISOR_80MHZ 16
#define SOFTUART2_TMR_DIVISOR_160MHZ 32

#define HALF_BIT_TIME 7
#define BIT_TIME 15
#define ONE_AND_HALF_BIT_TIME 23
#define CHAR_TIME (10*(BIT_TIME+1))

enum
{
    UART_STARTBIT,
    UART_BIT0,
    UART_BIT1,
    UART_BIT2,
    UART_BIT3,
    UART_BIT4,
    UART_BIT5,
    UART_BIT6,
    UART_BIT7,
    UART_STOPBIT
};

// softuart2 only allocated if softuart2 being used
static softuart2_t *softuart2 = NULL;

//############################
// util functions

static uint32_t getCPUTicksPerSec()
{
  return system_get_cpu_freq() * 1000000;
}

static uint32_t cpuToTimerTicks(uint32_t z_cpuTicks)
{
  return z_cpuTicks / (system_get_cpu_freq() == 80 ? SOFTUART2_TMR_DIVISOR_80MHZ : SOFTUART2_TMR_DIVISOR_160MHZ);
}

static uint16_t getPinGpioMask(uint8_t z_pin)
{
  return 1 << GPIO_ID_PIN(pin_num[z_pin]);
}

//############################
// interrupt handler functions

static void process_tx(void)
{
    uint16_t gpioMask = getPinGpioMask(softuart2->pin_tx);

    if(softuart2->tx_timer > 0)
    {
        softuart2->tx_timer--;
    }
    else
    {
        switch(softuart2->tx_state)
        {
            case UART_STARTBIT:
            {
                if(circular_buffer_pop(&softuart2->tx_buffer, &softuart2->tx_byte) == 0)
                {
                    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, gpioMask);
                    softuart2->tx_timer = BIT_TIME;
                    softuart2->tx_state++;
                }
                break;
            }

            case UART_BIT0:
            case UART_BIT1:
            case UART_BIT2:
            case UART_BIT3:
            case UART_BIT4:
            case UART_BIT5:
            case UART_BIT6:
            case UART_BIT7:
            {
                if(softuart2->tx_byte & 0x01)
                {
                    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, gpioMask);
                }
                else
                {
                    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, gpioMask);
                }

                softuart2->tx_byte >>= 1;
                softuart2->tx_timer = BIT_TIME;
                softuart2->tx_state++;
                break;
            }

            case UART_STOPBIT:
            {
                GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, gpioMask);
                softuart2->tx_timer = BIT_TIME;
                softuart2->tx_state = UART_STARTBIT;
                break;
            }

            default:
            {
                softuart2->tx_timer = 0;
                softuart2->tx_state = UART_STARTBIT;
                break;
            }
        }
    }
}

static void process_rx(void)
{
    if(softuart2->rx_timer > 0)
    {
        softuart2->rx_timer--;
    }
    else
    {
        uint8 rx = 0;
        uint32_t gpio = GPIO_REG_READ(GPIO_IN_ADDRESS);

        if((gpio & getPinGpioMask(softuart2->pin_rx)) != 0)
        {
            rx = 1;
        }
        else
        {
            rx = 0;
        }

        switch(softuart2->rx_state)
        {
            case UART_STARTBIT:
            {
                // Check for start bit
                if(rx == 0)
                {
                    softuart2->rx_timer = ONE_AND_HALF_BIT_TIME;
                    softuart2->rx_state++;
                }
                else if(softuart2->rx_idle_timer > 0)
                {
                    softuart2->rx_idle_timer--;
                        
                    if(softuart2->rx_idle_timer == 0)
                    {
                        (*softuart2->rx_callback)(softuart2);
                    }
                }
                break;
            }

            case UART_BIT0:
            case UART_BIT1:
            case UART_BIT2:
            case UART_BIT3:
            case UART_BIT4:
            case UART_BIT5:
            case UART_BIT6:
            case UART_BIT7:
            {
                softuart2->rx_byte >>= 1;

                // Set data bit
                if(rx == 1)
                {
                    softuart2->rx_byte |= 0x80;
                }
                else
                {
                    softuart2->rx_byte &= 0x7F;
                }

                softuart2->rx_timer = BIT_TIME;
                softuart2->rx_state++;
                break;
            }

            case UART_STOPBIT:
            {
                // Check for stop bit
                if(rx == 1)
                {
                    if(circular_buffer_push(&softuart2->rx_buffer, softuart2->rx_byte) == 0)
                    {
                        uint8_t need_rx_callback = 0;

                        // Check for callback conditions
        				if(softuart2->need_len > 0)
                        {
        				    if(circular_buffer_length(&softuart2->rx_buffer) >= softuart2->need_len)
                            {
        				        need_rx_callback = 1;
                            }
                        }
        				else if(softuart2->need_len < 0)
                        {
        				    softuart2->rx_idle_timer = 2*CHAR_TIME;
                        }
                        else if((char)softuart2->rx_byte == softuart2->end_char)
                        {
       				        need_rx_callback = 1;
                        }
                        
                        if(need_rx_callback == 1)
                        {
                            (*softuart2->rx_callback)(softuart2);
                        }
                    }
                }
                softuart2->rx_timer = HALF_BIT_TIME;
                softuart2->rx_state = UART_STARTBIT;
                break;
            }

            default:
            {
                softuart2->rx_timer = 0;
                softuart2->rx_state = UART_STARTBIT;
                break;
            }
        }
    }
}

static void ICACHE_RAM_ATTR timerInterruptHandler(os_param_t z_arg)
{
    uint8_t pin_clk = softuart2->pin_clk;
    uint16_t gpioMask = getPinGpioMask(pin_clk);

    if(pin_clk != 0xFF)
    {
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, gpioMask);
    }

    if(softuart2->pin_tx != 0xFF)
    {
        process_tx();
    }
    
    if(softuart2->pin_rx != 0xFF)
    {
        process_rx();
    }

    if(pin_clk != 0xFF)
    {
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, gpioMask);
    }
}

//############################
// driver public functions

/**
 * softuart2_drv_init
 * @return softuart2 softuart2
 */
softuart2_t *softuart2_drv_init()
{
    softuart2 = os_malloc(sizeof(softuart2_t));
    
    if(softuart2 != NULL)
    {
        memset(softuart2, 0, sizeof(*softuart2));
        softuart2->rx_buffer.head = 0;
        softuart2->rx_buffer.tail = 0;
        softuart2->rx_buffer.len = 0;
        softuart2->rx_buffer.maxlen = SOFTUART2_MAX_BUFF;
        softuart2->tx_buffer.head = 0;
        softuart2->tx_buffer.tail = 0;
        softuart2->tx_buffer.len = 0;
        softuart2->tx_buffer.maxlen = SOFTUART2_MAX_BUFF;
    }
    return(softuart2);
}

/**
 * softuart2_drv_start
 * @return true if started OK
 */
bool softuart2_drv_start()
{
  if(softuart2 != NULL)
  {
      uint8_t pin;

      if (!platform_hw_timer_init_exclusive(FRC1_SOURCE, TRUE, timerInterruptHandler, (os_param_t)&softuart2, (void (*)(void))NULL)) {
        return false;
      }

      // tx pin
      pin = softuart2->pin_tx;
      if(pin != 0xFF)
      {
          PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]); // set tx pin as gpio
          PIN_PULLUP_EN(pin_mux[pin]); // set tx pin pullup on
          GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, getPinGpioMask(pin)); // set tx pin as output
          GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, getPinGpioMask(pin));
      }

      // rx pin
      pin = softuart2->pin_rx;
      if(pin != 0xFF)
      {
          PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]); // set rx pin as gpio
          PIN_PULLUP_EN(pin_mux[pin]); // set rx pin pullup on
      }
      // clk pin
      pin = softuart2->pin_clk;
      if(pin != 0xFF)
      {
          PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]); // set clk pin as gpio
          PIN_PULLUP_EN(pin_mux[pin]); // set clk pin pullup on
          GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, getPinGpioMask(pin)); // set clk pin as output
      }
      
      softuart2->tx_timer = 0;
      softuart2->tx_state = UART_STARTBIT;

      softuart2->rx_timer = 0;
      softuart2->rx_idle_timer = 0;
      softuart2->rx_state = UART_STARTBIT;

      platform_hw_timer_arm_ticks_exclusive(cpuToTimerTicks(getCPUTicksPerSec()/(softuart2->baudrate*16)));
      return true;
  }
  else
  {
      return false;
  }
}

/**
 * softuart2_drv_get_data
 * @return softuart2 data
 */
softuart2_t *softuart2_drv_get_data()
{
    return softuart2;
}

/**
 * softuart2_drv_delete
 * Free up resources
 */
void softuart2_drv_delete()
{
    platform_hw_timer_close_exclusive();
    os_free(softuart2);
    softuart2 = NULL;
}

/**
 * Push data to circular buffer
 * @param z_buffer - pointer to buffer
 * @param z_data - data to push
 * @return 0 if OK otherwise -1
 */
int circular_buffer_push(circular_buffer_t *z_buffer, uint8_t z_data)
{
    int next;

    next = z_buffer->head + 1;  // next is where head will point to after this write.
    if (next >= z_buffer->maxlen)
    {
        next = 0;
    }

    if (next == z_buffer->tail)  // if the head + 1 == tail, buffer is full
    {
        return -1;
    }

    z_buffer->buffer[z_buffer->head] = z_data;  // write data and then move
    z_buffer->len++;
    z_buffer->head = next;  // head to next data offset.
    return 0;
}

/**
 * Pop data from circular buffer
 * @param z_buffer - pointer to buffer
 * @param z_data - pointer to data
 * @return 0 if OK otherwise -1
 */
int circular_buffer_pop(circular_buffer_t *z_buffer, uint8_t *z_data)
{
    int next;

    if (z_buffer->head == z_buffer->tail)  // if the head == tail, we don't have any data
    {
        return -1;
    }

    next = z_buffer->tail + 1;  // next is where tail will point to after this read.
    if(next >= z_buffer->maxlen)
    {
        next = 0;
    }

    *z_data = z_buffer->buffer[z_buffer->tail];  // read data and then move
    z_buffer->len--;
    z_buffer->tail = next; // tail to next offset.
    return 0;
}

/**
 * Length of circular buffer
 * @param z_buffer - pointer to buffer
 * @return length
 */
int circular_buffer_length(circular_buffer_t *z_buffer)
{
    return(z_buffer->len);
}
