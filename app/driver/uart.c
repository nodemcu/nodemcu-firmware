/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: uart.c
 *
 * Description: Two UART mode configration and interrupt handler.
 *              Check your hardware connection while use this mode.
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "task/task.h"
#include "user_config.h"
#include "user_interface.h"
#include "osapi.h"

#define UART0   0
#define UART1   1

#ifndef FUNC_U0RXD
#define FUNC_U0RXD 0
#endif
#ifndef FUNC_U0CTS
#define FUNC_U0CTS                      4
#endif


// For event signalling
static task_handle_t sig = 0;
static uint8 *sig_flag;
static uint8 isr_flag = 0;

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

static os_timer_t autobaud_timer;

static void (*alt_uart0_tx)(char txchar);

LOCAL void ICACHE_RAM_ATTR
uart0_rx_intr_handler(void *para);


/******************************************************************************
 * FunctionName : uart_wait_tx_empty
 * Description  : Internal used function
 *                Wait for TX FIFO to become empty.
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_wait_tx_empty(uint8 uart_no)
{
    while ((READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S)) > 0)
        ;
}


/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
    uart_wait_tx_empty(uart_no);

    if (uart_no == UART1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    } else {
        /* rcv_buff size if 0x100 */
        ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
        PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
    }

    uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));

    WRITE_PERI_REG(UART_CONF0(uart_no), ((UartDev.exist_parity & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S) //SET BIT AND PARITY MODE
                   | ((UartDev.parity & UART_PARITY_M)  <<UART_PARITY_S )
                   | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                   | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));


    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

    //set rx fifo trigger
    WRITE_PERI_REG(UART_CONF1(uart_no), (UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S);

    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
    //enable rx_interrupt
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA);
}



/******************************************************************************
 * FunctionName : uart0_alt
 * Description  : Internal used function
 *                UART0 pins changed to 13,15 if 'on' is set, else set to normal pins
 * Parameters   : on - 1 = use alternate pins, 0 = use normal pins
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_alt(uint8 on)
{
    uart_wait_tx_empty(UART0);

    if (on)
    {
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        PIN_PULLUP_EN(PERIPHS_IO_MUX_MTCK_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS);
        // now make RTS/CTS behave as TX/RX
        IOSWAP |= (1 << IOSWAPU0);
    }
    else
    {
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
        PIN_PULLUP_EN(PERIPHS_IO_MUX_U0RXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
        // now make RX/TX behave as TX/RX
        IOSWAP &= ~(1 << IOSWAPU0);
    }
}


/******************************************************************************
 * FunctionName : uart_tx_one_char
 * Description  : Internal used function
 *                Use uart interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
STATUS ICACHE_FLASH_ATTR
uart_tx_one_char(uint8 uart, uint8 TxChar)
{
    if (uart == 0 && alt_uart0_tx) {
      (*alt_uart0_tx)(TxChar);
      return OK;
    }

    while (true)
    {
      uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
      if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
        break;
      }
    }

    WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
  if (c == '\n')
  {
    uart_tx_one_char(UART1, '\r');
    uart_tx_one_char(UART1, '\n');
  }
  else if (c == '\r')
  {
  }
  else
  {
    uart_tx_one_char(UART1, c);
  }
}

/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(uint8 *buf, uint16 len)
{
  uint16 i;

  for (i = 0; i < len; i++)
  {
    uart_tx_one_char(UART0, buf[i]);
  }
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR uart0_sendStr(const char *str)
{
    while(*str)
    {
        // uart_tx_one_char(UART0, *str++);
        uart0_putc(*str++);
    }
}

/******************************************************************************
 * FunctionName : uart0_putc
 * Description  : use uart0 to transfer char
 * Parameters   : uint8 c - send char
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR uart0_putc(const char c)
{
  if (c == '\n')
  {
    uart_tx_one_char(UART0, '\r');
    uart_tx_one_char(UART0, '\n');
  }
  else if (c == '\r')
  {
  }
  else
  {
    uart_tx_one_char(UART0, c);
  }
}

/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
LOCAL void
uart0_rx_intr_handler(void *para)
{
    /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
     * uart1 and uart0 respectively
     */
    RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
    uint8 RcvChar;
    bool got_input = false;

    if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST)) {
        return;
    }

    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);

    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
        RcvChar = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

        /* you can add your handle code below.*/

        *(pRxBuff->pWritePos) = RcvChar;

        // insert here for get one command line from uart
        if (RcvChar == '\r' || RcvChar == '\n' ) {
            pRxBuff->BuffState = WRITE_OVER;
        }

        if (pRxBuff->pWritePos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
            // overflow ...we may need more error handle here.
            pRxBuff->pWritePos = pRxBuff->pRcvMsgBuff ;
        } else {
            pRxBuff->pWritePos++;
        }

        if (pRxBuff->pWritePos == pRxBuff->pReadPos){   // overflow one byte, need push pReadPos one byte ahead
            if (pRxBuff->pReadPos == (pRxBuff->pRcvMsgBuff + RX_BUFF_SIZE)) {
                pRxBuff->pReadPos = pRxBuff->pRcvMsgBuff ;
            } else {
                pRxBuff->pReadPos++;
            }
        }

        got_input = true;
    }

    if (got_input && sig) {
      if (isr_flag == *sig_flag) {
        isr_flag ^= 0x01;
        task_post_low (sig, 0x8000 | isr_flag << 14 | false);
      }
    }
}

static void 
uart_autobaud_timeout(void *timer_arg)
{
  uint32_t uart_no = (uint32_t) timer_arg;
  uint32_t divisor = uart_baudrate_detect(uart_no, 1);
  static int called_count = 0;

  // Shut off after two minutes to stop wasting CPU cycles if insufficient input received
  if (called_count++ > 10 * 60 * 2 || divisor) {
    os_timer_disarm(&autobaud_timer);
  }

  if (divisor) {
    uart_div_modify(uart_no, divisor);
  }
}
#include "pm/swtimer.h"

static void 
uart_init_autobaud(uint32_t uart_no)
{
  os_timer_setfn(&autobaud_timer, uart_autobaud_timeout, (void *) uart_no);
  SWTIMER_REG_CB(uart_autobaud_timeout, SWTIMER_DROP);
    //if autobaud hasn't done it's thing by the time light sleep triggered, it probably isn't going to happen.
  os_timer_arm(&autobaud_timer, 100, TRUE);
}

static void 
uart_stop_autobaud()
{
  os_timer_disarm(&autobaud_timer);
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 *                os_signal_t  sig_input - signal to post
 *                uint8       *flag_input - flag of consumer task
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br, os_signal_t sig_input, uint8 *flag_input)
{
    sig = sig_input;
    sig_flag = flag_input;

    // rom use 74880 baut_rate, here reinitialize
    UartDev.baut_rate = uart0_br;
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
#ifdef BIT_RATE_AUTOBAUD
    uart_init_autobaud(0);
#endif
}

void ICACHE_FLASH_ATTR
uart_setup(uint8 uart_no)
{
#ifdef BIT_RATE_AUTOBAUD
    uart_stop_autobaud();
#endif
    // poll Tx FIFO empty outside before disabling interrupts
    uart_wait_tx_empty(uart_no);
    ETS_UART_INTR_DISABLE();
    uart_config(uart_no);
    ETS_UART_INTR_ENABLE();
}

void ICACHE_FLASH_ATTR uart_set_alt_output_uart0(void (*fn)(char)) {
  alt_uart0_tx = fn;
}

UartConfig ICACHE_FLASH_ATTR uart_get_config(uint8 uart_no) {
  UartConfig config;

  config.baut_rate = UART_CLK_FREQ / READ_PERI_REG(UART_CLKDIV(uart_no));

  uint32_t conf = READ_PERI_REG(UART_CONF0(uart_no));

  config.exist_parity = (conf >> UART_PARITY_EN_S)    & UART_PARITY_EN_M;
  config.parity       = (conf >> UART_PARITY_S)       & UART_PARITY_M;
  config.stop_bits    = (conf >> UART_STOP_BIT_NUM_S) & UART_STOP_BIT_NUM;
  config.data_bits    = (conf >> UART_BIT_NUM_S)      & UART_BIT_NUM;

  return config;
}
