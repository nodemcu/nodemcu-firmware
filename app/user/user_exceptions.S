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

#ifdef __ESP8266__

/* Exception handler for supporting non-32bit wide loads from mapped SPI flash
 * (well, technically any mapped memory that can do 32bit loads). Pure assembly
 * since the RTOS-SDK does not use the ROM-routines for hooking exception
 * vectors, and thus we don't get the nifty C-wrapper. Without it we need to
 * do all the careful register saving and restoring, and at that point its
 * easier to also do the remaining logic in asm than reimplement the C-wrapper.
 *
 * On entry, a0,a2..a15 + SAR registers are saved into an exception frame kept
 * in RAM. Effectively this is the stack for the handler, as we intentionally
 * leave the real (a1) stack untouched to keep things simple and predictable.
 * Just like the C-wrapper, we do not permit loading into a1 as that is not
 * a valid use-case (the stack pointer is 32bit, nothing less).
 *
 * We "only" handle L8UI, L16UI and L16SI instructions, anything else we chain
 * through to the default SDK handler so it can do its usual error reporting.
 *
 * The handler comprises two parts, the actual UserExceptionVectorOverride
 * and the cause3_handler function. The former is what we wedge in to hook
 * the user exceptions, and it only contains enough logic to redirect cause 3
 * (EXCCAUSE_LOAD_STORE_ERROR) exceptions to the cause3_handler function.
 * All other exception codes are chained straight through to the SDK handler
 * we displaced. The cause3_handler is where the actual work gets done for
 * the load functionality.
 */

/* frame save area, a0->a15, but a1=>sar */
.section ".data"
.align 4
frame: .fill 16, 4, 0


/* macro to apply the stored frame values to regs, we need it twice */
.macro apply_frame
  l32i a15, a0, 60
  l32i a14, a0, 56
  l32i a13, a0, 52
  l32i a12, a0, 48
  l32i a11, a0, 44
  l32i a10, a0, 40
  l32i a9, a0, 36
  l32i a8, a0, 32
  l32i a7, a0, 28
  l32i a6, a0, 24
  l32i a5, a0, 20
  l32i a4, a0, 16
  l32i a3, a0, 12
  l32i a2, a0, 4
  wsr a2, SAR
  l32i a2, a0, 8
  l32i a0, a0, 0
.endm


/* Contants/literals for the cause3_handler function */
.section ".iram0.text"
.align 4
align_mask: .word ~3        /* mask to get 32bit alignment of addresses   */
load_mask:  .word 0x00f00f  /* mask for load instructions                 */
l8ui_match: .word 0x000002  /* post-mask match for 8bit load              */
l16ui_match:.word 0x001002  /* post-mask match for 16bit unsigned load    */
l16si_match:.word 0x009002  /* post-mask match for 16bit signed load      */
.literal_position


/* Register usage in cause3_handler:
 *   a0  exception frame pointer
 *   a1  stack (untouched)
 *   a2  temp values
 *   a3  alignment mask
 *   a4  instruction match
 *   a5  faulting instruction
 *   a6  masked instruction
 *   a7  excvaddr
 *   a11 temp values
 *   a12 temp values
 *   a13 epc1
 *   a15 extracted value
 */
.type cause3_handler,@function
cause3_handler:
  movi a0, frame       /* keep our exception frame pointer in a0              */
  s32i a2, a0, 8       /* save all used and/or relevant registers to frame    */
  s32i a3, a0, 12
  s32i a4, a0, 16
  s32i a5, a0, 20
  s32i a6, a0, 24
  s32i a7, a0, 28
  s32i a8, a0, 32
  s32i a9, a0, 36
  s32i a10, a0, 40
  s32i a11, a0, 44
  s32i a12, a0, 48
  s32i a13, a0, 52
  s32i a14, a0, 56
  s32i a15, a0, 60
  rsr a2, EXCSAVE1     /* retrieve original a0                                */
  s32i a2, a0, 0       /* save original a0 to frame                           */
  rsr a2, SAR          /* get shift register contents                         */
  s32i a2, a0, 4       /* save sar to frame in a1 slot                        */

  rsr a13, EPC1        /* get the program counter that caused the exception   */
  ssa8l a13            /* prepare to extract the (unaligned) instruction      */
  l32r a3, align_mask  /* prepare mask for 32bit alignment                    */
  and a2, a13, a3      /* get aligned base address of instruction             */
  l32i a11, a2, 0      /* load first part                                     */
  l32i a12, a2, 4      /* load second part                                    */
  src a5, a12, a11     /* faulting instruction now in a5                      */
  l32r a2, load_mask   /* get the mask for the load fields                    */
  and a6, a2, a5       /* extract the to-match-on fields from the instruction */

  rsr a7, EXCVADDR     /* get the attempted address to load from              */
  and a2, a7, a3       /* mask down to 32bit alignment                        */
  l32i a2, a2, 0       /* full word in a2                                     */
  ssa8l a7             /* set up shift based on excvaddr                      */
  sra a15, a2          /* right-shifted word, not yet masked                  */

  l32r a4, l8ui_match  /* work out what the faulting instruction is           */
  beq a6, a4, 1f
  l32r a4, l16ui_match
  beq a6, a4, 2f
  l32r a4, l16si_match
  beq a6, a4, 2f
  j 9f                 /* it's not a supported load, we need to chain to SDK  */
1:movi a2, 0xff        /* 8bits to keep                                       */
  j 3f
2:movi a2, 0xffff      /* 16bits to keep                                      */
3:and a15, a15, a2     /* apply mask to get the bits we care about            */
  l32r a4, l16si_match /* time to consider need for sign extension            */
  bne a6, a4, 4f       /* skip sign extension                                 */
  bbci a15, 15, 4f     /* no sign to extend                                   */
  movi a2, 0xffff0000  /* manual sign extension since 'sext' op not present   */
  or a15, a15, a2      /* sign extend                                         */
4:movi a2, 0xf0        /* register mask for load instructions                 */
  and a2, a5, a2       /* extract register field                              */
  srli a2, a2, 2       /* register number*4, effectively offset into frame    */
  beqi a2, 4, 9f       /* nope, won't do a1, chain through to SDK instead     */
  add  a2, a0, a2      /* pointer to correct register slot in frame           */
  s32i a15, a2, 0      /* apply new value to stashed register                 */

  addi a2, a13, 3      /* advance program counter past faulting instruction   */
  wsr a2, EPC1         /* and store it back                                   */
  apply_frame          /* apply the results                                   */
  rfe                  /* and done!                                           */

9:apply_frame          /* restore the original registers                      */
  ret                  /* and hop back into the exception vector code         */



/* Our sneaky override of the UserExceptionVector to allow us to handle 8/16bit
 * loads from SPI flash. MUST be <= 32bytes compiled, as the next vector starts
 * there.
 */
.section ".UserExceptionVectorOverride.text"
.type _UserExceptionVectorOverride,@function
.globl _UserExceptionVectorOverride
_UserExceptionVectorOverride:
  wsr a0, EXCSAVE1         /* free up a0 for a while                          */
  rsr a0, EXCCAUSE         /* get the exception cause                         */
  bnei a0, 3, 2f           /* if not EXCCAUSE_LOAD_STORE_ERROR, chain to rtos */
  j 1f                     /* jump past noncode bytes for cause3_handler addr */
  .align 4                 /* proper alignment for literals                   */
  .literal_position        /* the linker will put cause3_handler addr here    */
1:call0 cause3_handler     /* handle loads with rfe, stores will return here  */
2:rsr a0, EXCSAVE1         /* restore a0 before we chain                      */
  j _UserExceptionVector   /* and off we go to rtos                           */

#endif
