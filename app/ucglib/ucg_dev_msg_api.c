/*

  ucg_dev_msg_api.c 
  
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
#include <stddef.h>

void ucg_PowerDown(ucg_t *ucg)
{
  if ( ucg->is_power_up != 0 )
  {
    ucg->device_cb(ucg, UCG_MSG_DEV_POWER_DOWN, NULL);
    ucg->is_power_up = 0;
  }
}

ucg_int_t ucg_PowerUp(ucg_t *ucg)
{
  ucg_int_t r;
  /* power down first. will do nothing if power is already down */
  ucg_PowerDown(ucg);
  /* now try to power up the display */
  r = ucg->device_cb(ucg, UCG_MSG_DEV_POWER_UP, NULL);
  if ( r != 0 )
  {
    ucg->is_power_up = 1;
  }
  return r;
}

void ucg_SetClipBox(ucg_t *ucg, ucg_box_t *clip_box)
{
  ucg->device_cb(ucg, UCG_MSG_SET_CLIP_BOX, (void *)(clip_box));
}

void ucg_SetClipRange(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h)
{
  ucg_box_t clip_box;
  clip_box.ul.x = x;
  clip_box.ul.y = y;
  clip_box.size.w = w;
  clip_box.size.h = h;
  ucg_SetClipBox(ucg, &clip_box);
}

void ucg_SetMaxClipRange(ucg_t *ucg)
{
  ucg_box_t new_clip_box;
  new_clip_box.size = ucg->dimension;
  new_clip_box.ul.x = 0;
  new_clip_box.ul.y = 0;
  ucg_SetClipBox(ucg, &new_clip_box);
}

/* 
  Query the display dimension from the driver, reset clip window to maximum 
  new dimension
*/
void ucg_GetDimension(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_GET_DIMENSION, &(ucg->dimension));
  ucg_SetMaxClipRange(ucg);
}

void ucg_DrawPixelWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
}

void ucg_DrawL90FXWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_L90FX, &(ucg->arg));
}

#ifdef UCG_MSG_DRAW_L90TC
void ucg_DrawL90TCWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_L90TC, &(ucg->arg));
}
#endif /* UCG_MSG_DRAW_L90TC */

#ifdef UCG_MSG_DRAW_L90BF
void ucg_DrawL90BFWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_L90BF, &(ucg->arg));
}
#endif /* UCG_MSG_DRAW_L90BF */

void ucg_DrawL90SEWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_L90SE, &(ucg->arg));
}

/*
void ucg_DrawL90RLWithArg(ucg_t *ucg)
{
  ucg->device_cb(ucg, UCG_MSG_DRAW_L90RL, &(ucg->arg));
}
*/

