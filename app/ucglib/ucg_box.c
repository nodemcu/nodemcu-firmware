/*

  ucg_box.c

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

void ucg_DrawBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h)
{
  while( h > 0 )
  {
    ucg_DrawHLine(ucg, x, y, w);
    h--;
    y++;
  }  
}

/*
  - clear the screen with black color
  - reset clip range to max
  - set draw color to white
*/
void ucg_ClearScreen(ucg_t *ucg)
{
  ucg_SetColor(ucg, 0, 0, 0, 0);
  ucg_SetMaxClipRange(ucg);
  ucg_DrawBox(ucg, 0, 0, ucg_GetWidth(ucg), ucg_GetHeight(ucg));
  ucg_SetColor(ucg, 0, 255, 255, 255);
}



void ucg_DrawRBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h, ucg_int_t r)
{
  ucg_int_t xl, yu;
  ucg_int_t yl, xr;

  xl = x;
  xl += r;
  yu = y;
  yu += r;
 
  xr = x;
  xr += w;
  xr -= r;
  xr -= 1;
  
  yl = y;
  yl += h;
  yl -= r; 
  yl -= 1;

  ucg_DrawDisc(ucg, xl, yu, r, UCG_DRAW_UPPER_LEFT);
  ucg_DrawDisc(ucg, xr, yu, r, UCG_DRAW_UPPER_RIGHT);
  ucg_DrawDisc(ucg, xl, yl, r, UCG_DRAW_LOWER_LEFT);
  ucg_DrawDisc(ucg, xr, yl, r, UCG_DRAW_LOWER_RIGHT);

  {
    ucg_int_t ww, hh;

    ww = w;
    ww -= r;
    ww -= r;
    ww -= 2;
    hh = h;
    hh -= r;
    hh -= r;
    hh -= 2;
    
    xl++;
    yu++;
    h--;
    ucg_DrawBox(ucg, xl, y, ww, r+1);
    ucg_DrawBox(ucg, xl, yl, ww, r+1);
    ucg_DrawBox(ucg, x, yu, w, hh);
  }
}


ucg_ccs_t ucg_ccs_box[6];	/* color component sliders used by GradientBox */

void ucg_DrawGradientBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h)
{
  uint8_t i;
  
  /* Setup ccs for l90se. This will be updated by ucg_clip_l90se if required */
  /* 8. Jan 2014: correct? */

  //printf("%d %d %d\n", ucg->arg.rgb[3].color[0], ucg->arg.rgb[3].color[1], ucg->arg.rgb[3].color[2]);
  
  for ( i = 0; i < 3; i++ )
  {
    ucg_ccs_init(ucg_ccs_box+i, ucg->arg.rgb[0].color[i], ucg->arg.rgb[2].color[i], h);
    ucg_ccs_init(ucg_ccs_box+i+3, ucg->arg.rgb[1].color[i], ucg->arg.rgb[3].color[i], h);
  }
  
  
  while( h > 0 )
  {
    ucg->arg.rgb[0].color[0] = ucg_ccs_box[0].current;
    ucg->arg.rgb[0].color[1] = ucg_ccs_box[1].current;
    ucg->arg.rgb[0].color[2] = ucg_ccs_box[2].current;
    ucg->arg.rgb[1].color[0] = ucg_ccs_box[3].current;
    ucg->arg.rgb[1].color[1] = ucg_ccs_box[4].current;
    ucg->arg.rgb[1].color[2] = ucg_ccs_box[5].current;
    //printf("%d %d %d\n", ucg_ccs_box[0].current, ucg_ccs_box[1].current, ucg_ccs_box[2].current);
    //printf("%d %d %d\n", ucg_ccs_box[3].current, ucg_ccs_box[4].current, ucg_ccs_box[5].current);
    ucg->arg.pixel.pos.x = x;
    ucg->arg.pixel.pos.y = y;
    ucg->arg.len = w;
    ucg->arg.dir = 0;
    ucg_DrawL90SEWithArg(ucg);
    for ( i = 0; i < 6; i++ )
      ucg_ccs_step(ucg_ccs_box+i);
    h--;
    y++;
  }
}


/* restrictions: w > 0 && h > 0 */
void ucg_DrawFrame(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h)
{
  ucg_int_t xtmp = x;
  
  ucg_DrawHLine(ucg, x, y, w);
  ucg_DrawVLine(ucg, x, y, h);
  x+=w;
  x--;
  ucg_DrawVLine(ucg, x, y, h);
  y+=h;
  y--;
  ucg_DrawHLine(ucg, xtmp, y, w);
}

void ucg_DrawRFrame(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h, ucg_int_t r)
{
  ucg_int_t xl, yu;

  xl = x;
  xl += r;
  yu = y;
  yu += r;
 
  {
    ucg_int_t yl, xr;
      
    xr = x;
    xr += w;
    xr -= r;
    xr -= 1;
    
    yl = y;
    yl += h;
    yl -= r; 
    yl -= 1;

    ucg_DrawCircle(ucg, xl, yu, r, UCG_DRAW_UPPER_LEFT);
    ucg_DrawCircle(ucg, xr, yu, r, UCG_DRAW_UPPER_RIGHT);
    ucg_DrawCircle(ucg, xl, yl, r, UCG_DRAW_LOWER_LEFT);
    ucg_DrawCircle(ucg, xr, yl, r, UCG_DRAW_LOWER_RIGHT);
  }

  {
    ucg_int_t ww, hh;

    ww = w;
    ww -= r;
    ww -= r;
    ww -= 2;
    hh = h;
    hh -= r;
    hh -= r;
    hh -= 2;
    
    xl++;
    yu++;
    h--;
    w--;
    ucg_DrawHLine(ucg, xl, y, ww);
    ucg_DrawHLine(ucg, xl, y+h, ww);
    ucg_DrawVLine(ucg, x,         yu, hh);
    ucg_DrawVLine(ucg, x+w, yu, hh);
  }
}
