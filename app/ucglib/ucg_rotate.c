/*

  ucg_rotate.c
  
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
#include <assert.h>

/* Side-Effects: Update dimension and reset clip range to max */
void ucg_UndoRotate(ucg_t *ucg)
{
  if ( ucg->rotate_chain_device_cb != NULL )
  {
    ucg->device_cb = ucg->rotate_chain_device_cb;
    ucg->rotate_chain_device_cb = NULL;
  }
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}

/*================================================*/
/* 90 degree */

static void ucg_rotate_90_xy(ucg_xy_t *xy, ucg_int_t display_width)
{   
    ucg_int_t x, y;
    y = xy->x;
    x = display_width;
    x -= xy->y;
    x--;
    xy->x = x;
    xy->y = y;  
  
}

ucg_int_t ucg_dev_rotate90(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_GET_DIMENSION:
      ucg->rotate_chain_device_cb(ucg, msg, &(ucg->rotate_dimension)); 
      ((ucg_wh_t *)data)->h = ucg->rotate_dimension.w;
      ((ucg_wh_t *)data)->w = ucg->rotate_dimension.h;
    
      //printf("rw=%d rh=%d\n", ucg->rotate_dimension.w, ucg->rotate_dimension.h);
      //printf("aw=%d ah=%d\n",  ((ucg_wh_t volatile * volatile )data)->w, ((ucg_wh_t volatile * volatile )data)->h);
      //printf("dw=%d dh=%d\n", ucg->dimension.w, ucg->dimension.h);
      return 1;
      
    case UCG_MSG_SET_CLIP_BOX:
      /* to rotate the box, the lower left corner will become the new xy value pair */
      /* so the unrotated lower left is put into "ul" */
      //printf("pre clipbox x=%d y=%d\n", ((ucg_box_t * )data)->ul.x, ((ucg_box_t * )data)->ul.y);
      ((ucg_box_t * )data)->ul.y += ((ucg_box_t * )data)->size.h-1;
      //printf("pre clipbox lower left x=%d y=%d\n", ((ucg_box_t * )data)->ul.x, ((ucg_box_t * )data)->ul.y);
      /* then apply rotation */
      ucg_rotate_90_xy(&(((ucg_box_t * )data)->ul), ucg->rotate_dimension.w); 
      /* finally, swap dimensions */
      {
        ucg_int_t tmp;
        tmp = ((ucg_box_t *)data)->size.w;
        ((ucg_box_t * )data)->size.w = ((ucg_box_t *)data)->size.h;
        ((ucg_box_t * )data)->size.h = tmp;
      }
      //printf("post clipbox x=%d y=%d\n", ((ucg_box_t * )data)->ul.x, ((ucg_box_t * )data)->ul.y);
      break;
    case UCG_MSG_DRAW_PIXEL:
    case UCG_MSG_DRAW_L90FX:
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
    case UCG_MSG_DRAW_L90BF:
#endif /* UCG_MSG_DRAW_L90BF */
    case UCG_MSG_DRAW_L90SE:
    //case UCG_MSG_DRAW_L90RL:
      ucg->arg.dir+=1;
      ucg->arg.dir&=3;
      //ucg_rotate_90_xy(&(ucg->arg.pixel.pos), ucg->rotate_dimension.w); 
      //printf("dw=%d dh=%d\n", ucg->dimension.w, ucg->dimension.h);
      //printf("pre x=%d y=%d\n", ucg->arg.pixel.pos.x, ucg->arg.pixel.pos.y);
      ucg_rotate_90_xy(&(ucg->arg.pixel.pos), ucg->rotate_dimension.w); 
      //printf("post x=%d y=%d\n", ucg->arg.pixel.pos.x, ucg->arg.pixel.pos.y);
      break;
  }
  return ucg->rotate_chain_device_cb(ucg, msg, data);  
}

/* Side-Effects: Update dimension and reset clip range to max */
void ucg_SetRotate90(ucg_t *ucg)
{
  ucg_UndoRotate(ucg);
  ucg->rotate_chain_device_cb = ucg->device_cb;
  ucg->device_cb = ucg_dev_rotate90;
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}

/*================================================*/
/* 180 degree */

static void ucg_rotate_180_xy(ucg_t *ucg, ucg_xy_t *xy)
{
    ucg_int_t x, y;
    y = ucg->rotate_dimension.h;
    y -= xy->y;
    y--;
    xy->y = y;
  
    x = ucg->rotate_dimension.w;
    x -= xy->x;
    x--;
    xy->x = x;
  
}

ucg_int_t ucg_dev_rotate180(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_GET_DIMENSION:
      ucg->rotate_chain_device_cb(ucg, msg, &(ucg->rotate_dimension)); 
      *((ucg_wh_t *)data) = (ucg->rotate_dimension);
      return 1;
    case UCG_MSG_SET_CLIP_BOX:
      /* calculate and rotate lower right point of the clip box */
      ((ucg_box_t * )data)->ul.y += ((ucg_box_t * )data)->size.h-1;
      ((ucg_box_t * )data)->ul.x += ((ucg_box_t * )data)->size.w-1;
      ucg_rotate_180_xy(ucg, &(((ucg_box_t * )data)->ul)); 
      /* box dimensions are the same */
      break;
    case UCG_MSG_DRAW_PIXEL:
    case UCG_MSG_DRAW_L90FX:
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
    case UCG_MSG_DRAW_L90BF:
#endif /* UCG_MSG_DRAW_L90BF */
    case UCG_MSG_DRAW_L90SE:
    //case UCG_MSG_DRAW_L90RL:
      ucg->arg.dir+=2;
      ucg->arg.dir&=3;
      ucg_rotate_180_xy(ucg, &(ucg->arg.pixel.pos)); 
      break;
  }
  return ucg->rotate_chain_device_cb(ucg, msg, data);  
}

/* Side-Effects: Update dimension and reset clip range to max */
void ucg_SetRotate180(ucg_t *ucg)
{
  ucg_UndoRotate(ucg);
  ucg->rotate_chain_device_cb = ucg->device_cb;
  ucg->device_cb = ucg_dev_rotate180;
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}

/*================================================*/
/* 270 degree */

static void ucg_rotate_270_xy(ucg_t *ucg, ucg_xy_t *xy)
{
    ucg_int_t x, y;
    x = xy->y;
  
    y = ucg->rotate_dimension.h;
    y -= xy->x;
    y--;
  
    xy->y = y;
    xy->x = x;  
}

ucg_int_t ucg_dev_rotate270(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_GET_DIMENSION:
      ucg->rotate_chain_device_cb(ucg, msg, &(ucg->rotate_dimension)); 
      ((ucg_wh_t *)data)->h = ucg->rotate_dimension.w;
      ((ucg_wh_t *)data)->w = ucg->rotate_dimension.h;
      return 1;
    case UCG_MSG_SET_CLIP_BOX:
      /* calculate and rotate upper right point of the clip box */
      ((ucg_box_t * )data)->ul.x += ((ucg_box_t * )data)->size.w-1;
      ucg_rotate_270_xy(ucg, &(((ucg_box_t * )data)->ul)); 
      /* finally, swap dimensions */
      {
        ucg_int_t tmp;
        tmp = ((ucg_box_t *)data)->size.w;
        ((ucg_box_t * )data)->size.w = ((ucg_box_t *)data)->size.h;
        ((ucg_box_t * )data)->size.h = tmp;
      }
      break;
    case UCG_MSG_DRAW_PIXEL:
    case UCG_MSG_DRAW_L90FX:
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
    case UCG_MSG_DRAW_L90BF:
#endif /* UCG_MSG_DRAW_L90BF */
    case UCG_MSG_DRAW_L90SE:
//    case UCG_MSG_DRAW_L90RL:
      ucg->arg.dir+=3;
      ucg->arg.dir&=3;
      ucg_rotate_270_xy(ucg, &(ucg->arg.pixel.pos)); 
      break;
  }
  return ucg->rotate_chain_device_cb(ucg, msg, data);  
}

/* Side-Effects: Update dimension and reset clip range to max */
void ucg_SetRotate270(ucg_t *ucg)
{
  ucg_UndoRotate(ucg);
  ucg->rotate_chain_device_cb = ucg->device_cb;
  ucg->device_cb = ucg_dev_rotate270;
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}

