/*
 * Copyright 2016 Dius Computing Pty Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 * - Neither the name of the copyright holders nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @author Johny Mattsson <jmattsson@dius.com.au>
 */

#include "driver/console.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "soc/soc.h"
#include "soc/uart_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sys/reent.h"
#include <unistd.h>

#include "rom/libc_stubs.h"
#include "rom/uart.h"

#define UART_INPUT_QUEUE_SZ 0x100

// These used to be available in soc/uart_register.h:
#define UART_GET_RXFIFO_RD_BYTE(i)  GET_PERI_REG_BITS2(UART_FIFO_REG(i) , UART_RXFIFO_RD_BYTE_V, UART_RXFIFO_RD_BYTE_S)
#define UART_GET_RXFIFO_CNT(i)  GET_PERI_REG_BITS2(UART_STATUS_REG(i) , UART_RXFIFO_CNT_V, UART_RXFIFO_CNT_S)
#define UART_SET_AUTOBAUD_EN(i,val)  SET_PERI_REG_BITS(UART_AUTOBAUD_REG(i) ,UART_AUTOBAUD_EN_V,(val),UART_AUTOBAUD_EN_S)


typedef int (*_read_r_fn) (struct _reent *r, int fd, void *buf, int size);

static _read_r_fn _read_r_app;
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
static _read_r_fn _read_r_pro;
#endif

static xQueueHandle uart0Q;
static task_handle_t input_task = 0;

// --- Syscall support for reading from STDIN_FILENO ---------------

static int console_read_r (struct _reent *r, int fd, void *buf, int size, _read_r_fn next)
{
  if (fd == STDIN_FILENO)
  {
    static _lock_t stdin_lock;
    _lock_acquire_recursive (&stdin_lock);
    char *c = (char *)buf;
    int i = 0;
    for (; i < size; ++i)
    {
      if (!console_getc (c++))
        break;
    }
    _lock_release_recursive (&stdin_lock);
    return i;
  }
  else if (next)
    return next (r, fd, buf, size);
  else
    return -1;
}

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
static int console_read_r_pro (struct _reent *r, int fd, void *buf, int size)
{
  return console_read_r (r, fd, buf, size, _read_r_pro);
}
#endif
static int console_read_r_app (struct _reent *r, int fd, void *buf, int size)
{
  return console_read_r (r, fd, buf, size, _read_r_app);
}

// --- End syscall support -------------------------------------------

static void uart0_rx_intr_handler (void *arg)
{
  (void)arg;
  bool received = false;
  uint32_t status = READ_PERI_REG(UART_INT_ST_REG(CONSOLE_UART));
  while (status)
  {
    if (status & UART_FRM_ERR_INT_ENA)
    {
      // TODO: report somehow?
      WRITE_PERI_REG(UART_INT_CLR_REG(CONSOLE_UART), UART_FRM_ERR_INT_ENA);
    }
    if ((status & UART_RXFIFO_TOUT_INT_ENA) ||
        (status & UART_RXFIFO_FULL_INT_ENA))
    {
      uint32_t fifo_len = UART_GET_RXFIFO_CNT(CONSOLE_UART);
      for (uint32_t i = 0; i < fifo_len; ++i)
      {
        received = true;
        char c = UART_GET_RXFIFO_RD_BYTE(CONSOLE_UART);
        if (uart0Q)
         xQueueSendToBackFromISR (uart0Q, &c, NULL);
      }
      WRITE_PERI_REG(UART_INT_CLR_REG(CONSOLE_UART),
        UART_RXFIFO_TOUT_INT_ENA | UART_RXFIFO_FULL_INT_ENA);
    }
    status = READ_PERI_REG(UART_INT_ST_REG(CONSOLE_UART));
  }
  if (received && input_task)
    task_post_isr_low (input_task, false);
}


void console_setup (const ConsoleSetup_t *cfg)
{
  esp_rom_uart_tx_wait_idle (CONSOLE_UART);

  uart_config_t uart_conf = {
    .baud_rate = cfg->bit_rate,
    .data_bits =
      cfg->data_bits == CONSOLE_NUM_BITS_5 ? UART_DATA_5_BITS :
      cfg->data_bits == CONSOLE_NUM_BITS_6 ? UART_DATA_6_BITS :
      cfg->data_bits == CONSOLE_NUM_BITS_7 ? UART_DATA_7_BITS :
      UART_DATA_8_BITS,
    .stop_bits =
      cfg->stop_bits == CONSOLE_STOP_BITS_1 ? UART_STOP_BITS_1 :
      cfg->stop_bits == CONSOLE_STOP_BITS_2 ? UART_STOP_BITS_2 :
      UART_STOP_BITS_1_5,
    .parity =
      cfg->parity == CONSOLE_PARITY_NONE ? UART_PARITY_DISABLE :
      cfg->parity == CONSOLE_PARITY_EVEN ? UART_PARITY_EVEN :
      UART_PARITY_ODD,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  uart_param_config(CONSOLE_UART, &uart_conf);

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
  // TODO: Make this actually work
  UART_SET_AUTOBAUD_EN(CONSOLE_UART, cfg->auto_baud);
#endif
}



void console_init (const ConsoleSetup_t *cfg, task_handle_t tsk)
{
  input_task = tsk;
  uart0Q = xQueueCreate (UART_INPUT_QUEUE_SZ, sizeof (char));

  console_setup (cfg);

  uart_isr_register(CONSOLE_UART, uart0_rx_intr_handler, NULL, 0, NULL);
  uart_set_rx_timeout(CONSOLE_UART, 2);
  uart_set_rx_full_threshold(CONSOLE_UART, 10);
  uart_enable_intr_mask(CONSOLE_UART,
    UART_RXFIFO_TOUT_INT_ENA_M |
    UART_RXFIFO_FULL_INT_ENA_M |
    UART_FRM_ERR_INT_ENA_M);

  // Register our console_read_r_xxx functions to support stdin input
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  _read_r_app = syscall_table_ptr->_read_r;
  syscall_table_ptr->_read_r = console_read_r_app;
#else
  _read_r_app = syscall_table_ptr_app->_read_r;
  _read_r_pro = syscall_table_ptr_pro->_read_r;
  syscall_table_ptr_app->_read_r = console_read_r_app;
  syscall_table_ptr_pro->_read_r = console_read_r_pro;
#endif
}


bool console_getc (char *c)
{
  return (uart0Q && (xQueueReceive (uart0Q, c, 0) == pdTRUE));
}
