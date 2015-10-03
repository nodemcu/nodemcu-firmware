/*

  ucg_line.c
  
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

void ucg_Draw90Line(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len, ucg_int_t dir, ucg_int_t col_idx)
{
  ucg->arg.pixel.rgb.color[0] = ucg->arg.rgb[col_idx].color[0];
  ucg->arg.pixel.rgb.color[1] = ucg->arg.rgb[col_idx].color[1];
  ucg->arg.pixel.rgb.color[2] = ucg->arg.rgb[col_idx].color[2];
  ucg->arg.pixel.pos.x = x;
  ucg->arg.pixel.pos.y = y;
  ucg->arg.len = len;
  ucg->arg.dir = dir;
  ucg_DrawL90FXWithArg(ucg);
}

void ucg_DrawHLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len)
{
  ucg_Draw90Line(ucg, x, y, len, 0, 0);
}

void ucg_DrawVLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len)
{
  ucg_Draw90Line(ucg, x, y, len, 1, 0);
}

void ucg_DrawHRLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len)
{
  ucg_Draw90Line(ucg, x, y, len, 2, 0);
}

void ucg_DrawGradientLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len, ucg_int_t dir)
{
  /* color goes from ucg->arg.rgb[0] to ucg->arg.rgb[1] */
  ucg->arg.pixel.pos.x = x;
  ucg->arg.pixel.pos.y = y;
  ucg->arg.len = len;
  ucg->arg.dir = dir;
  ucg_DrawL90SEWithArg(ucg);
}

void ucg_DrawLine(ucg_t *ucg, ucg_int_t x1, ucg_int_t y1, ucg_int_t x2, ucg_int_t y2)
{
  ucg_int_t tmp;
  ucg_int_t x,y;
  ucg_int_t dx, dy;
  ucg_int_t err;
  ucg_int_t ystep;

  uint8_t swapxy = 0;
  
  /* no BBX intersection check at the moment... */

  ucg->arg.pixel.rgb.color[0] = ucg->arg.rgb[0].color[0];
  ucg->arg.pixel.rgb.color[1] = ucg->arg.rgb[0].color[1];
  ucg->arg.pixel.rgb.color[2] = ucg->arg.rgb[0].color[2];
    
  if ( x1 > x2 ) dx = x1-x2; else dx = x2-x1;
  if ( y1 > y2 ) dy = y1-y2; else dy = y2-y1;

  if ( dy > dx ) 
  {
    swapxy = 1;
    tmp = dx; dx =dy; dy = tmp;
    tmp = x1; x1 =y1; y1 = tmp;
    tmp = x2; x2 =y2; y2 = tmp;
  }
  if ( x1 > x2 ) 
  {
    tmp = x1; x1 =x2; x2 = tmp;
    tmp = y1; y1 =y2; y2 = tmp;
  }
  err = dx >> 1;
  if ( y2 > y1 ) ystep = 1; else ystep = -1;
  y = y1;
  for( x = x1; x <= x2; x++ )
  {
    if ( swapxy == 0 ) 
    {
      ucg->arg.pixel.pos.x = x;
      ucg->arg.pixel.pos.y = y;
    }
    else 
    {
      ucg->arg.pixel.pos.x = y;
      ucg->arg.pixel.pos.y = x;
    }
    ucg_DrawPixelWithArg(ucg);  
    err -= (uint8_t)dy;
    if ( err < 0 ) 
    {
      y += ystep;
      err += dx;
    }
  }

}
