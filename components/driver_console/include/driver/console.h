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

#include "esp32/rom/uart.h"
#include "task/task.h"
#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_UART CONFIG_ESP_CONSOLE_UART_NUM

typedef enum
{
  CONSOLE_NUM_BITS_5 = 0x0,
  CONSOLE_NUM_BITS_6 = 0x1,
  CONSOLE_NUM_BITS_7 = 0x2,
  CONSOLE_NUM_BITS_8 = 0x3
} ConsoleNumBits_t;

typedef enum
{
  CONSOLE_PARITY_NONE = 0x2,
  CONSOLE_PARITY_ODD  = 0x1,
  CONSOLE_PARITY_EVEN = 0x0
} ConsoleParity_t;

typedef enum
{
  CONSOLE_STOP_BITS_1   = 0x1,
  CONSOLE_STOP_BITS_1_5 = 0x2,
  CONSOLE_STOP_BITS_2   = 0x3
} ConsoleStopBits_t;

typedef struct
{
  uint32_t          bit_rate;
  ConsoleNumBits_t  data_bits;
  ConsoleParity_t   parity;
  ConsoleStopBits_t stop_bits;
  bool              auto_baud;
} ConsoleSetup_t;

void console_init (const ConsoleSetup_t *cfg, task_handle_t tsk);
void console_setup (const ConsoleSetup_t *cfg);
bool console_getc (char *c);
