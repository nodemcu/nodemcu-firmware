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

#define LOAD_MASK   0x00f00fu
#define L8UI_MATCH  0x000002u
#define L16UI_MATCH 0x001002u
#define L16SI_MATCH 0x009002u

static exception_handler_fn load_store_handler;

void load_non_32_wide_handler (struct exception_frame *ef, uint32_t cause)
{
  /* If this is not EXCCAUSE_LOAD_STORE_ERROR you're doing it wrong! */
  (void)cause;

  uint32_t epc1 = ef->epc;
  uint32_t excvaddr;
  uint32_t insn;
  asm (
    "rsr   %0, EXCVADDR;"    /* read out the faulting address */
    "movi  a4, ~3;"          /* prepare a mask for the EPC */
    "and   a4, a4, %2;"      /* apply mask for 32bit aligned base */
    "l32i  a5, a4, 0;"       /* load part 1 */
    "l32i  a6, a4, 4;"       /* load part 2 */
    "ssa8l %2;"              /* set up shift register for src op */
    "src   %1, a6, a5;"      /* right shift to get faulting instruction */
    :"=r"(excvaddr), "=r"(insn)
    :"r"(epc1)
    :"a4", "a5", "a6"
  );

  uint32_t valmask = 0;
  uint32_t what = insn & LOAD_MASK;

  if (what == L8UI_MATCH)
    valmask = 0xffu;
  else if (what == L16UI_MATCH || what == L16SI_MATCH)
    valmask = 0xffffu;
  else
  {
die:
    /* Turns out we couldn't fix this, so try and chain to the handler
     * that was set. (This is typically a remote GDB break). If none 
     * then trigger a system break instead and hang if the break doesn't 
     * get handled. This is effectively what would happen if the default 
     * handler was installed. */
    if (load_store_handler) {
      load_store_handler(ef, cause);
      return;
    }
     asm ("break 1, 1");
    while (1) {}
  }

  /* Load, shift and mask down to correct size */
  uint32_t val = (*(uint32_t *)(excvaddr & ~0x3));
  val >>= (excvaddr & 0x3) * 8;
  val &= valmask;

  /* Sign-extend for L16SI, if applicable */
  if (what == L16SI_MATCH && (val & 0x8000))
    val |= 0xffff0000;

  int regno = (insn & 0x0000f0u) >> 4;
  if (regno == 1)
    goto die;              /* we can't support loading into a1, just die */
  else if (regno != 0)
    --regno;               /* account for skipped a1 in exception_frame */

  ef->a_reg[regno] = val;  /* carry out the load */
  ef->epc += 3;            /* resume at following instruction */
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
