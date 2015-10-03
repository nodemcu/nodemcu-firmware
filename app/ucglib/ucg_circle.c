/*

  ucg_circle.c
  
  Circle Drawing Procedures

  Universal uC Color Graphics Library
  
  Copyright (c) 2014, olikraus@gmail.com
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

static void ucg_draw_circle_section(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t x0, ucg_int_t y0, uint8_t option) UCG_NOINLINE;

static void ucg_draw_circle_section(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t x0, ucg_int_t y0, uint8_t option)
{
    /* upper right */
    if ( option & UCG_DRAW_UPPER_RIGHT )
    {
      ucg_DrawPixel(ucg, x0 + x, y0 - y);
      ucg_DrawPixel(ucg, x0 + y, y0 - x);
    }
    
    /* upper left */
    if ( option & UCG_DRAW_UPPER_LEFT )
    {
      ucg_DrawPixel(ucg, x0 - x, y0 - y);
      ucg_DrawPixel(ucg, x0 - y, y0 - x);
    }
    
    /* lower right */
    if ( option & UCG_DRAW_LOWER_RIGHT )
    {
      ucg_DrawPixel(ucg, x0 + x, y0 + y);
      ucg_DrawPixel(ucg, x0 + y, y0 + x);
    }
    
    /* lower left */
    if ( option & UCG_DRAW_LOWER_LEFT )
    {
      ucg_DrawPixel(ucg, x0 - x, y0 + y);
      ucg_DrawPixel(ucg, x0 - y, y0 + x);
    }
}

void ucg_draw_circle(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option)
{
    ucg_int_t f;
    ucg_int_t ddF_x;
    ucg_int_t ddF_y;
    ucg_int_t x;
    ucg_int_t y;

    f = 1;
    f -= rad;
    ddF_x = 1;
    ddF_y = 0;
    ddF_y -= rad;
    ddF_y *= 2;
    x = 0;
    y = rad;

    ucg_draw_circle_section(ucg, x, y, x0, y0, option);
    
    while ( x < y )
    {
      if (f >= 0) 
      {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;

      ucg_draw_circle_section(ucg, x, y, x0, y0, option);    
    }
}

void ucg_DrawCircle(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option)
{
  /* check for bounding box */
  /*
  {
    ucg_int_t radp, radp2;
    
    radp = rad;
    radp++;
    radp2 = radp;
    radp2 *= 2;
    
    if ( ucg_IsBBXIntersection(ucg, x0-radp, y0-radp, radp2, radp2) == 0)
      return;    
  }
  */
  
  /* draw circle */
  ucg_draw_circle(ucg, x0, y0, rad, option);
}

static void ucg_draw_disc_section(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t x0, ucg_int_t y0, uint8_t option) UCG_NOINLINE;

static void ucg_draw_disc_section(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t x0, ucg_int_t y0, uint8_t option)
{
    /* upper right */
    if ( option & UCG_DRAW_UPPER_RIGHT )
    {
      ucg_DrawVLine(ucg, x0+x, y0-y, y+1);
      ucg_DrawVLine(ucg, x0+y, y0-x, x+1);
    }
    
    /* upper left */
    if ( option & UCG_DRAW_UPPER_LEFT )
    {
      ucg_DrawVLine(ucg, x0-x, y0-y, y+1);
      ucg_DrawVLine(ucg, x0-y, y0-x, x+1);
    }
    
    /* lower right */
    if ( option & UCG_DRAW_LOWER_RIGHT )
    {
      ucg_DrawVLine(ucg, x0+x, y0, y+1);
      ucg_DrawVLine(ucg, x0+y, y0, x+1);
    }
    
    /* lower left */
    if ( option & UCG_DRAW_LOWER_LEFT )
    {
      ucg_DrawVLine(ucg, x0-x, y0, y+1);
      ucg_DrawVLine(ucg, x0-y, y0, x+1);
    }
}

void ucg_draw_disc(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option)
{
  ucg_int_t f;
  ucg_int_t ddF_x;
  ucg_int_t ddF_y;
  ucg_int_t x;
  ucg_int_t y;

  f = 1;
  f -= rad;
  ddF_x = 1;
  ddF_y = 0;
  ddF_y -= rad;
  ddF_y *= 2;
  x = 0;
  y = rad;

  ucg_draw_disc_section(ucg, x, y, x0, y0, option);
  
  while ( x < y )
  {
    if (f >= 0) 
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    ucg_draw_disc_section(ucg, x, y, x0, y0, option);    
  }
}

void ucg_DrawDisc(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option)
{
  /* check for bounding box */
  /*
  {
    ucg_int_t radp, radp2;
    
    radp = rad;
    radp++;
    radp2 = radp;
    radp2 *= 2;
    
    if ( ucg_IsBBXIntersection(ucg, x0-radp, y0-radp, radp2, radp2) == 0)
      return;    
  }
  */
  
  /* draw disc */
  ucg_draw_disc(ucg, x0, y0, rad, option);
}

