/*

  ucg_clip.c
  
  Clipping procedures for the device funktions
  
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
  
*/

#include "ucg.h"

static ucg_int_t ucg_clip_is_x_visible(ucg_t *ucg) UCG_NOINLINE;
static ucg_int_t ucg_clip_is_y_visible(ucg_t *ucg) UCG_NOINLINE;

static ucg_int_t ucg_clip_is_x_visible(ucg_t *ucg)
{
  ucg_int_t t;
  t = ucg->arg.pixel.pos.x;
  t -= ucg->clip_box.ul.x;
  if ( t < 0 )
    return 0;
  if ( t >= ucg->clip_box.size.w )
    return 0;
  
  return 1;
}

static ucg_int_t ucg_clip_is_y_visible(ucg_t *ucg)
{
  ucg_int_t t;
  t = ucg->arg.pixel.pos.y;
  t -= ucg->clip_box.ul.y;
  if ( t < 0 )
    return 0;
  if ( t >= ucg->clip_box.size.h )
    return 0;
  
  return 1;
}

/*
  Description:
    clip range from a (included) to b (excluded) agains c (included) to d (excluded)
  Assumptions:
    a <= b
    c <= d
*/
static ucg_int_t ucg_clip_intersection(ucg_int_t *ap, ucg_int_t *bp, ucg_int_t c, ucg_int_t d)
{
  ucg_int_t a = *ap;
  ucg_int_t b = *bp;
  
  if ( a >= d )
    return 0;
  if ( b <= c )
    return 0;
  if ( a < c )
    *ap = c;
  if ( b > d )
    *bp = d;
  return 1;
}

ucg_int_t ucg_clip_is_pixel_visible(ucg_t *ucg)
{
  if ( ucg_clip_is_x_visible(ucg) == 0 )
    return 0;
  if ( ucg_clip_is_y_visible(ucg) == 0 )
    return 0;
  return 1;
}



/*
  assumes, that ucg->arg contains data for l90fx and does clipping 
  against ucg->clip_box
*/
ucg_int_t ucg_clip_l90fx(ucg_t *ucg)
{
  ucg_int_t a;
  ucg_int_t b;
  ucg->arg.offset = 0;
  switch(ucg->arg.dir)
  {
    case 0:
      if ( ucg_clip_is_y_visible(ucg) == 0 )
	return 0; 
      a = ucg->arg.pixel.pos.x;
      b = a;
      b += ucg->arg.len;
      
      if ( ucg_clip_intersection(&a, &b, ucg->clip_box.ul.x, ucg->clip_box.ul.x+ucg->clip_box.size.w) == 0 )
	return 0;
      
      ucg->arg.offset = a - ucg->arg.pixel.pos.x;
      ucg->arg.pixel.pos.x = a;
      b -= a;
      ucg->arg.len = b;
      
      break;
    case 1:
      if ( ucg_clip_is_x_visible(ucg) == 0 )
	return 0;
      
      a = ucg->arg.pixel.pos.y;
      b = a;
      b += ucg->arg.len;
      
      if ( ucg_clip_intersection(&a, &b, ucg->clip_box.ul.y, ucg->clip_box.ul.y+ucg->clip_box.size.h) == 0 )
	return 0;

      ucg->arg.offset = a - ucg->arg.pixel.pos.y;
      ucg->arg.pixel.pos.y = a;
      b -= a;
      ucg->arg.len = b;
      
      break;
    case 2:
      if ( ucg_clip_is_y_visible(ucg) == 0 )
	return 0;
      
      b = ucg->arg.pixel.pos.x;
      b++;
      
      a = b;
      a -= ucg->arg.len;
      
      
      if ( ucg_clip_intersection(&a, &b, ucg->clip_box.ul.x, ucg->clip_box.ul.x+ucg->clip_box.size.w) == 0 )
	return 0;
      
      ucg->arg.len = b-a;
      
      b--;
      ucg->arg.offset = ucg->arg.pixel.pos.x-b;
      ucg->arg.pixel.pos.x = b;
      
      break;
    case 3:
      if ( ucg_clip_is_x_visible(ucg) == 0 )
	return 0;

      b = ucg->arg.pixel.pos.y;
      b++;
      
      a = b;
      a -= ucg->arg.len;
      
      
      if ( ucg_clip_intersection(&a, &b, ucg->clip_box.ul.y, ucg->clip_box.ul.y+ucg->clip_box.size.h) == 0 )
	return 0;
      
      ucg->arg.len = b-a;
      
      b--;
      ucg->arg.offset = ucg->arg.pixel.pos.y-b;
      ucg->arg.pixel.pos.y = b;
      
      
      break;
  }

  return 1;
}

ucg_int_t ucg_clip_l90tc(ucg_t *ucg)
{
  if ( ucg_clip_l90fx(ucg) == 0 )
      return 0;
  ucg->arg.pixel_skip = ucg->arg.offset & 0x07;
  ucg->arg.bitmap += (ucg->arg.offset >>3);
  return 1;
}

/* old code
ucg_int_t ucg_clip_l90tc(ucg_t *ucg)
{
  ucg_int_t t;
  switch(ucg->arg.dir)
  {
    case 0:
      t = ucg->arg.pixel.pos.x;
      if ( ucg_clip_l90fx(ucg) == 0 )
	return 0;
      t = ucg->arg.pixel.pos.x - t;
      break;
    case 1:
      t = ucg->arg.pixel.pos.y;
      if ( ucg_clip_l90fx(ucg) == 0 )
	return 0;
      t = ucg->arg.pixel.pos.y - t;
      break;
    case 2:
      t = ucg->arg.pixel.pos.x;
      if ( ucg_clip_l90fx(ucg) == 0 )
	return 0;
      t -= ucg->arg.pixel.pos.x;
      break;
    case 3:
      t = ucg->arg.pixel.pos.y;
      if ( ucg_clip_l90fx(ucg) == 0 )
	return 0;
      t -= ucg->arg.pixel.pos.y;
      break;
  } 
  ucg->arg.pixel_skip = t & 0x07;
  ucg->arg.bitmap += (t >>3);
  return 1;
}
*/

ucg_int_t ucg_clip_l90se(ucg_t *ucg)
{
  uint8_t i;
  if ( ucg_clip_l90fx(ucg) == 0 )
      return 0;
  for ( i = 0; i < 3; i++ )
  {
    ucg_ccs_seek(ucg->arg.ccs_line+i, ucg->arg.offset);
  }  
  return 1;
}

