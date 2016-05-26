/*
 * Copyright 2015 Dius Computing Pty Ltd. All rights reserved.
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

/* Minimal handler function for 8/16bit loads from the mapped SPI flash.
 * Called from the overridden UserExceptionVector on exception cause 3.
 */
asm(
".section \".iram0.text\"\n"
"  .align 4\n"
"store_mask:\n"
"  .word 0x004000\n"   /* bit 14 is set on store instructions (see note ^)    */
"align_mask:\n"
"  .word ~3\n"         /* mask to get 32bit alignment of addresses            */
".type cause3_handler,@function\n"
"cause3_handler:\n"
"  wsr a2, EXCSAVE2\n" /* free up a2 for use too                              */
"  rsr a2, EPC1\n"     /* get the program counter that caused the exception   */
"  ssa8l a2\n"         /* prepare to extract the (unaligned) instruction      */
"  l32r a0, align_mask\n"/* prepare mask for 32bit alignment                  */
"  and a2, a2, a0\n"   /* get aligned base address of instruction             */
"  l32i a0, a2, 0\n"   /* load first part                                     */
"  l32i a2, a2, 4\n"   /* load second part                                    */
"  src a0, a2, a0\n"   /* faulting instruction now in a0                      */
"  l32r a2, store_mask\n"
"  and a0, a0, a2\n"   /* test for store bit                                  */
"  bnez a0, 1f\n"      /* it's a store, we don't do stores, get out           */

""                     /* As it turns out, the ESP8266 happily does 8/16bit   */
                       /* loads from the mapped SPI flash, but then raises an */
                       /* exception anyway; we can simply increment the       */
                       /* program counter and be our merry way, safe in       */
                       /* the knowledge that the loads have already been done.*/
                       /* Note that this only applies to the SPI flash, not   */
                       /* internal ROM or IRAM - if we ever want to support   */
                       /* those we'll need to add the appropriate loading     */
                       /* logic here. For now I see no need for such support. */

"  rsr a2, EPC1\n"     /* read the program counter again                      */
"  addi a2, a2, 3\n"   /* advance program counter past faulting instruction   */
"  wsr a2, EPC1\n"     /* and store it back                                   */
"  rsr a2, EXCSAVE2\n" /* restore a2                                          */
"  rsr a0, EXCSAVE1\n" /* restore a0                                          */
"  rfe\n"              /* and done!                                           */
"1:rsr a2, EXCSAVE2\n" /* we're about to chain, so restore the a2 we clobbered*/
"  ret\n"              /* and hop back into the exception vector code         */
);
/* Note ^) Except for the S32E instruction, but that's not applicable here,
 *         so we can happily ignore it.
 */



/* Our sneaky override of the UserExceptionVector to allow us to handle 8/16bit
 * loads from SPI flash. MUST BE >= 32bytes compiled, as the next vector starts
 * there.
 */
asm(
".section \".UserExceptionVectorOverride.text\"\n"
".type _UserExceptionVectorOverride,@function\n"
".globl _UserExceptionVectorOverride\n"
"_UserExceptionVectorOverride:\n"
"  wsr a0, EXCSAVE1\n"     /* free up a0 for a while                          */
"  rsr a0, EXCCAUSE\n"     /* get the exception cause                         */
"  bnei a0, 3, 2f\n"       /* if not EXCCAUSE_LOAD_STORE_ERROR, chain to rtos */
"  j 1f\n"                 /* jump past noncode bytes for cause3_handler addr */
"  .align 4\n"             /* proper alignment for literals                   */
"  .literal_position\n"    /* the linker will put cause3_handler addr here    */
"1:call0 cause3_handler\n" /* handle loads and rfe, stores will return here   */
"2:rsr a0, EXCSAVE1\n"     /* restore a0 before we chain                      */
"  j _UserExceptionVector\n" /* and off we go to rtos                         */
);
