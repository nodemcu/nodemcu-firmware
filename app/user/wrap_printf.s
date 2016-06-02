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

/* The RTOS SDK provided printf() function has explicit support for dealing
 * with formatting strings stored in flash (used via the os_printf macro).
 * For others, that's a very useful optimisation, but for us it's an unwanted
 * pessimisation as it forces a malloc for each printf. To avoid this we
 * have a minimal replacement printf() which omits the explicit flash-support
 * support, and just calls the internal SDK function print() directly.
 */
.section ".iram0.text"
.type __wrap_printf,@function
.globl __wrap_printf
.align 4
.literal_position
__wrap_printf:
  movi a9, os_printf_enabled  /* a9 is caller saved                           */
  l8ui a9, a9, 0              /* load os_printf_enabled flag                  */
  bnez a9, 1f                 /* if it's enabled, we print everything         */
  movi a9, _irom0_sdktext_end /* else we check if it's SDK called             */
  bgeu a0, a9, 1f             /* ...and only print from user code             */
  ret
1:addi a1, a1, -48      /* get some stack space                               */
  s32i a0, a1,  0       /* store a0 at the bottom                             */
  s32i a2, a1, 16       /* copy the formatting string and first few arguments */
  s32i a3, a1, 20       /* ...onto the stack in a way that we get a struct    */
  s32i a4, a1, 24       /* ...{ uint32_t reg_args[6]; uint32_t ignore[2]; }   */
  s32i a5, a1, 28       /* ...with the formatting string in the first array   */
  s32i a6, a1, 32       /* ...array slot of .reg_args.                        */
  s32i a7, a1, 36       /* ...                                                */
  mov a3, a2            /* print() wants the formatting string as arg 2 too   */
  movi a2, 0            /* and a zero for its first arg                       */
  addi a4, a1, 16       /* third arg is the pointer to the argument array     */
  mov a5, a4            /* as is the fourth arg                               */
  movi a6, 4            /* and the fifth arg is 4  (https://xkcd.com/221/ ?)  */
  call0 print           /* do it!                                             */
  l32i a0, a1, 0        /* restore return address into a0                     */
  addi a1, a1, 48       /* release the stack                                  */
  ret                   /* all done                                           */

/* Flag for controlling whether SDK printf()s are suppressed or not.
 * This is to provide an RTOS-safe reimplementation of system_set_os_print(). */
.section ".data"
.globl os_printf_enabled
os_printf_enabled: .byte 1
