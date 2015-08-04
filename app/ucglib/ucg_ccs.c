/*

  ucg_ccs.c
  
  color component slide sub library

  Universal uC Color Graphics Library
  
  Copyright (c) 2013, olikraus@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this list 
    of conditions and the following disclaimer.
    
  * Redistributions in binary form must reproduce the above copyright notice, this 
    list of conditions and the following disclaimer in the documentation and/or other 
    materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  

  e: end
  n: steps
  
  
  f(0) = s
  f(n) = e
  f(x) = m*x + s
  f(n-1) = m*(n-1) + s =!= e
      --> m = (e-s)/n
  f(x) = (e-s)/(n-1) * x + s
  f(0) = s
  f(1) = s + (e-s)/(n-1)

*/

#include "ucg.h"

/*
  Setup change from "start" to "end" with a specified amount of "steps".
  After calling this procedure, ccs->current will contain the "start" value.
*/
void ucg_ccs_init(ucg_ccs_t *ccs, uint8_t start, uint8_t end, ucg_int_t steps)
{
  ccs->start = start;
  ccs->num = end-start;
  ccs->den = steps-1;
  ccs->dir = 1;
  
  ccs->quot = ccs->num / ccs->den;
  if ( ccs->num < 0 )
  {
    ccs->num = -ccs->num;
    ccs->dir = -1;
  }
  ccs->rem = ccs->num % ccs->den;
  
  ccs->frac = ccs->den/2;
  ccs->current = start;
}

/*
  Make one step towards the "end" value. 
  ccs->curront will contain the updated value.
*/
void ucg_ccs_step(ucg_ccs_t *ccs)
{
  
  ccs->current += ccs->quot;
  ccs->frac += ccs->rem;
  if ( ccs->frac >= ccs->den )
  {
    ccs->current += ccs->dir;
    ccs->frac -= ccs->den;
  }  
}

/*
  f(x) = (e-s)/(n-1) * x + s
  current = (num / den)  * (pos / den)
  
  Seek to the specified "pos"ition.
  "pos" must be between 0 and "end"-1
*/
void ucg_ccs_seek(ucg_ccs_t *ccs, ucg_int_t pos)
{
  ucg_int_t p;
  ccs->current = ccs->quot;
  ccs->current *= pos;
  p = ccs->rem * pos  + ccs->den/2;
  if ( ccs->dir >= 0 )
    ccs->current += p / ccs->den;
  else
    ccs->current -= p / ccs->den;
  ccs->frac = p % ccs->den;
  ccs->current += ccs->start;
}

