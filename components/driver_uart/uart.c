#if 0
/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *               2016 DiUS Computing Pty Ltd <jmattsson@dius.com.au>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266/ESP32 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "soc/soc.h"
#include "soc/io_mux_reg.h"
#include "esp_intr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "freertos/xtensa_api.h"

#include "driver/uart.h"
#include "task/task.h"
#include <stdio.h>

#define UART_INTR_MASK          0x1ff
#define UART_LINE_INV_MASK      (0x3f << 19)


static xQueueHandle uartQ[2];
static task_handle_t input_task = 0;

// FIXME: on the ESP32 the interrupts are not shared between UART0 & UART1

void uart_tx_one_char(uint32_t uart, uint8_t TxChar)
{
    while (true) {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS_REG(uart)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
            break;
        }
    }
    WRITE_PERI_REG(UART_FIFO_REG(uart) , TxChar);
}


static void uart1_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART1, '\r');
        uart_tx_one_char(UART1, '\n');
    } else if (c == '\r') {
    } else {
        uart_tx_one_char(UART1, c);
    }
}


static void uart0_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART0, '\r');
        uart_tx_one_char(UART0, '\n');
    } else if (c == '\r') {
    } else {
        uart_tx_one_char(UART0, c);
    }
}


//=================================================================

void UART_SetWordLength(UART_Port uart_no, UART_WordLength len)
{
    SET_PERI_REG_BITS(UART_CONF0_REG(uart_no), UART_BIT_NUM, len, UART_BIT_NUM_S);
}


void
UART_SetStopBits(UART_Port uart_no, UART_StopBits bit_num)
{
    SET_PERI_REG_BITS(UART_CONF0_REG(uart_no), UART_STOP_BIT_NUM, bit_num, UART_STOP_BIT_NUM_S);
}


void UART_SetLineInverse(UART_Port uart_no, UART_LineLevelInverse inverse_mask)
{
    CLEAR_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_LINE_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0_REG(uart_no), inverse_mask);
}


void UART_SetParity(UART_Port uart_no, UART_ParityMode Parity_mode)
{
    CLEAR_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_PARITY | UART_PARITY_EN);

    if (Parity_mode == USART_Parity_None) {
    } else {
        SET_PERI_REG_MASK(UART_CONF0_REG(uart_no), Parity_mode | UART_PARITY_EN);
    }
}


void UART_SetBaudrate(UART_Port uart_no, uint32 baud_rate)
{
    uart_div_modify(uart_no, UART_CLK_FREQ / baud_rate);
}


//only when USART_HardwareFlowControl_RTS is set , will the rx_thresh value be set.
void UART_SetFlowCtrl(UART_Port uart_no, UART_HwFlowCtrl flow_ctrl, uint8 rx_thresh)
{
  if (uart_no == 0)
  {
    if (flow_ctrl & USART_HardwareFlowControl_RTS) {
#if defined(__ESP8266__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
#elif defined(__ESP32__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO22_U, FUNC_GPIO22_U0RTS);
#endif
        SET_PERI_REG_BITS(UART_CONF1_REG(uart_no), UART_RX_FLOW_THRHD, rx_thresh, UART_RX_FLOW_THRHD_S);
        SET_PERI_REG_MASK(UART_CONF1_REG(uart_no), UART_RX_FLOW_EN);
    } else {
        CLEAR_PERI_REG_MASK(UART_CONF1_REG(uart_no), UART_RX_FLOW_EN);
    }

    if (flow_ctrl & USART_HardwareFlowControl_CTS) {
#if defined(__ESP8266__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
#elif defined(__ESP32__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO19_U, FUNC_GPIO19_U0CTS);
#endif
        SET_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_TX_FLOW_EN);
    } else {
        CLEAR_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_TX_FLOW_EN);
    }
  }
}


void UART_WaitTxFifoEmpty(UART_Port uart_no) //do not use if tx flow control enabled
{
    while (READ_PERI_REG(UART_STATUS_REG(uart_no)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S));
}


void UART_ResetFifo(UART_Port uart_no)
{
    SET_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0_REG(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
}


void UART_ClearIntrStatus(UART_Port uart_no, uint32 clr_mask)
{
    WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), clr_mask);
}


void UART_SetIntrEna(UART_Port uart_no, uint32 ena_mask)
{
    SET_PERI_REG_MASK(UART_INT_ENA_REG(uart_no), ena_mask);
}


void UART_intr_handler_register(void *fn, void *arg)
{
#if defined(__ESP8266__)
    _xt_isr_attach(ETS_UART_INUM, fn, arg);
#elif defined(__ESP32__)
    xt_set_interrupt_handler(ETS_UART0_INUM, fn, arg);
#endif
}


void UART_SetPrintPort(UART_Port uart_no)
{
    if (uart_no == 1) {
        ets_install_putc1(uart1_write_char);
    } else {
        ets_install_putc1(uart0_write_char);
    }
}


void UART_ParamConfig(UART_Port uart_no, const UART_ConfigTypeDef *pUARTConfig)
{
    if (uart_no == UART1) {
#if defined(__ESP8266__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
#elif defined(__ESP32__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA3_U, FUNC_SD_DATA3_U1TXD);
#endif
    } else {
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
#if defined(__ESP8266__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
#elif defined(__ESP32__)
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_U0RXD);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_U0TXD);
#endif
    }

    UART_SetFlowCtrl(uart_no, pUARTConfig->flow_ctrl, pUARTConfig->UART_RxFlowThresh);
    UART_SetBaudrate(uart_no, pUARTConfig->baud_rate);

    WRITE_PERI_REG(UART_CONF0_REG(uart_no),
                   ((pUARTConfig->parity == USART_Parity_None) ? 0x0 : (UART_PARITY_EN | pUARTConfig->parity))
                   | (pUARTConfig->stop_bits << UART_STOP_BIT_NUM_S)
                   | (pUARTConfig->data_bits << UART_BIT_NUM_S)
                   | ((pUARTConfig->flow_ctrl & USART_HardwareFlowControl_CTS) ? UART_TX_FLOW_EN : 0x0)
                   | pUARTConfig->UART_InverseMask
#if defined(__ESP32__)
                   | UART_TICK_REF_ALWAYS_ON
#endif
                   );

    UART_ResetFifo(uart_no);
}


void UART_IntrConfig(UART_Port uart_no,  const UART_IntrConfTypeDef *pUARTIntrConf)
{
    uint32 reg_val = 0;
    UART_ClearIntrStatus(uart_no, UART_INTR_MASK);
    reg_val = READ_PERI_REG(UART_CONF1_REG(uart_no));

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_RXFIFO_TOUT_INT_ENA) ?
                ((((pUARTIntrConf->UART_RX_TimeOutIntrThresh)&UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S) | UART_RX_TOUT_EN) : 0);

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_RXFIFO_FULL_INT_ENA) ?
                (((pUARTIntrConf->UART_RX_FifoFullIntrThresh)&UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) : 0);

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_TXFIFO_EMPTY_INT_ENA) ?
                (((pUARTIntrConf->UART_TX_FifoEmptyIntrThresh)&UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S) : 0);

    WRITE_PERI_REG(UART_CONF1_REG(uart_no), reg_val);
    CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(uart_no), UART_INTR_MASK);
    SET_PERI_REG_MASK(UART_INT_ENA_REG(uart_no), pUARTIntrConf->UART_IntrEnMask);
}


static void uart0_rx_intr_handler(void *para)
{
    /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    * uart1 and uart0 respectively
    */
    uint8 uart_no = UART0; // TODO: support UART1 as well
    uint8 fifo_len = 0;
    uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST_REG(uart_no)) ;

    while (uart_intr_status != 0x0) {
        if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)) {
            //os_printf_isr("FRM_ERR\r\n");
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), UART_FRM_ERR_INT_CLR);
        } else if (UART_RXFIFO_FULL_INT_ST == (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) {
            //os_printf_isr("full\r\n");
            fifo_len = (READ_PERI_REG(UART_STATUS_REG(uart_no)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;

            for (int i = 0; i < fifo_len; ++i)
            {
                char c = READ_PERI_REG(UART_FIFO_REG(uart_no)) & 0xff;
                if (uartQ[uart_no])
                  xQueueSendToBackFromISR (uartQ[uart_no], &c, NULL);
            }

            WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), UART_RXFIFO_FULL_INT_CLR);
            //CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
        } else if (UART_RXFIFO_TOUT_INT_ST == (uart_intr_status & UART_RXFIFO_TOUT_INT_ST)) {
            //os_printf_isr("timeout\r\n");
            fifo_len = (READ_PERI_REG(UART_STATUS_REG(uart_no)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;

            for (int i = 0; i < fifo_len; ++i)
            {
                char c = READ_PERI_REG(UART_FIFO_REG(uart_no)) & 0xff;
                if (uartQ[uart_no])
                  xQueueSendToBackFromISR (uartQ[uart_no], &c, NULL);
            }

            WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), UART_RXFIFO_TOUT_INT_CLR);
        } else if (UART_TXFIFO_EMPTY_INT_ST == (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) {
            //os_printf_isr("empty\n\r");
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
            CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(uart_no), UART_TXFIFO_EMPTY_INT_ENA);
        } else if (UART_RXFIFO_OVF_INT_ST  == (READ_PERI_REG(UART_INT_ST_REG(uart_no)) & UART_RXFIFO_OVF_INT_ST)) {
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_no), UART_RXFIFO_OVF_INT_CLR);
            //os_printf_isr("RX OVF!!\r\n");
        } else {
            //skip
        }

        uart_intr_status = READ_PERI_REG(UART_INT_ST_REG(uart_no)) ;
    }

    if (fifo_len && input_task)
      task_post_low (input_task, false);
}


void uart_init_uart0_console (const UART_ConfigTypeDef *config, task_handle_t tsk)
{
    input_task = tsk;
    uartQ[0] = xQueueCreate (0x100, sizeof (char));

    UART_WaitTxFifoEmpty(UART0);

    UART_ParamConfig(UART0, config);

    UART_IntrConfTypeDef uart_intr;
    uart_intr.UART_IntrEnMask =
        UART_RXFIFO_TOUT_INT_ENA |
        UART_FRM_ERR_INT_ENA |
        UART_RXFIFO_FULL_INT_ENA |
        UART_TXFIFO_EMPTY_INT_ENA;
    uart_intr.UART_RX_FifoFullIntrThresh = 10;
    uart_intr.UART_RX_TimeOutIntrThresh = 2;
    uart_intr.UART_TX_FifoEmptyIntrThresh = 20;
    UART_IntrConfig(UART0, &uart_intr);

    UART_SetPrintPort(UART0);
    UART_intr_handler_register(uart0_rx_intr_handler, NULL);
    ESP_UART0_INTR_ENABLE();
}


bool uart0_getc (char *c)
{
  return (uartQ[UART0] && (xQueueReceive (uartQ[UART0], c, 0) == pdTRUE));
}


void uart0_alt (bool on)
{
#if defined(__ESP8266__)
  if (on)
  {
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDO_U);
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
#else
  printf("Alternate UART0 pins not supported on this chip\n");
#endif
}

#endif
