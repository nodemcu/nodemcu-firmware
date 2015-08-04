/*

  ucg_pixel.c
  
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

void ucg_SetColor(ucg_t *ucg, uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
  //ucg->arg.pixel.rgb.color[0] = r;
  //ucg->arg.pixel.rgb.color[1] = g;
  //ucg->arg.pixel.rgb.color[2] = b;
  ucg->arg.rgb[idx].color[0] = r;
  ucg->arg.rgb[idx].color[1] = g;
  ucg->arg.rgb[idx].color[2] = b;
}


void ucg_DrawPixel(ucg_t *ucg, ucg_int_t x, ucg_int_t y)
{
  ucg->arg.pixel.rgb.color[0] = ucg->arg.rgb[0].color[0];
  ucg->arg.pixel.rgb.color[1] = ucg->arg.rgb[0].color[1];
  ucg->arg.pixel.rgb.color[2] = ucg->arg.rgb[0].color[2];
  
  ucg->arg.pixel.pos.x = x;
  ucg->arg.pixel.pos.y = y;
  ucg_DrawPixelWithArg(ucg);  
}

