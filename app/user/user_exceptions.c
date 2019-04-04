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

#include "user_exceptions.h"

static exception_handler_fn load_store_handler;

void load_non_32_wide_handler (struct exception_frame *ef, uint32_t cause)
{
  uint32_t val, insn;
  (void)cause;  /* If this is not EXCCAUSE_LOAD_STORE_ERROR you're doing it wrong! */

  asm (
    /*
     * Move the aligned content of the exception addr to val
     */
    "rsr     a6, EXCVADDR;"    /* read out the faulting address */
    "movi    a5, ~3;"          /* prepare a mask for the EPC */
    "and     a5, a5, a6;"      /* apply mask for 32bit aligned base */
    "l32i    a5, a5, 0;"       /* load aligned value */
    "ssa8l   a6;"              /* set up shift register for value */
    "srl     %[val], a5;"      /* shift left to align value */
                               /* we are done with a6 = EXCVADDR */
   /*
    *  Move the aligned instruction to insn
    */
    "movi    a5, ~3;"          /* prepare a mask for the insn */
    "and     a6, a5, %[epc];"  /* apply mask for 32bit aligned base */
    "l32i    a5, a6, 0;"       /* load part 1 */
    "l32i    a6, a6, 4;"       /* load part 2 */
    "ssa8l   %[epc];"          /* set up shift register for src op */
    "src     %[op], a6, a5;"   /* right shift to get faulting instruction */
    :[val]"=r"(val), [op]"=r"(insn)
    :[epc]"r"(ef->epc)
    :"a5", "a6"
  );

/* These instructions have the format 0xADSBII where AB = opcode and D = dest reg */
  uint32_t regno = (insn>>4)&0x0f;                           /* pick out nibble D*/
  uint32_t opcode = (uint8_t) (((insn>>12)<<4)|(insn&0xf));  /* and nibbles AB */
#define L8UI  0x02u
#define L16UI 0x12u
#define L16SI 0x92u

  if (opcode == L8UI) {                       /* L8UI */
    val = (uint8_t) val;
  } else {
    val = (uint16_t) val;                     /* assume L16SI or L16UI */
    if (opcode == L16SI) {
      val = (unsigned)((int)((sint16_t)val)); /* force signed 16->32 bit */
    } else if (opcode != L16UI) {
   /*
    * Anything other than L8UI, L16SI or L16UI then chain to the next handler
    * if set (typically a remote GDB break). Otherwise execute the default action
    * which is to trigger a system break and hang if the break doesn't get handled
    */
      if (load_store_handler) {
        load_store_handler(NULL, 0 /* ef ,  cause */);
        return;
      } else {
        asm ("break 1, 1");
        while (1) {}
      }
    }
  }
  ef->a_reg[regno ? regno-1: regno] = val; /* carry out the load */
  ef->epc += 3;                            /* resume at following instruction */
}

/**
 * The SDK's user_main function installs a debugging handler regardless
 * of whether there's a proper handler installed for EXCCAUSE_LOAD_STORE_ERROR,
 * which of course breaks everything if we allow that to go through. As such,
 * we use the linker to wrap that call and stop the SDK from shooting itself in
 * its proverbial foot. We do save the EXCCAUSE_LOAD_STORE_ERROR handler so that
 * we can chain to it above.
 */
exception_handler_fn TEXT_SECTION_ATTR
__wrap__xtos_set_exception_handler (uint32_t cause, exception_handler_fn fn)
{
  if (cause != EXCCAUSE_LOAD_STORE_ERROR)
    __real__xtos_set_exception_handler (cause, fn);
  else
    load_store_handler = fn;
}
