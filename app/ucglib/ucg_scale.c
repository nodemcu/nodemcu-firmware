/*

  ucg_scale.c
  
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

void ucg_UndoScale(ucg_t *ucg)
{
  if ( ucg->scale_chain_device_cb != NULL )
  {
    ucg->device_cb = ucg->scale_chain_device_cb;
    ucg->scale_chain_device_cb = NULL;
  }
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}

const ucg_fntpgm_uint8_t ucg_scale_2x2[16] UCG_FONT_SECTION("ucg_scale_2x2") =  
{ 0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f, 0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff };

 static void ucg_scale_2x2_send_next_half_byte(ucg_t *ucg, ucg_xy_t *xy, ucg_int_t msg, ucg_int_t len, ucg_int_t dir, uint8_t b)
{
  b &= 15;
  len *=2;


  
  ucg->arg.pixel.pos = *xy;
  switch(dir)
  {
    case 0: break;
    case 1: break;
    case 2: ucg->arg.pixel.pos.x++; break;
    default: case 3: ucg->arg.pixel.pos.y++; break;
  }
  
  ucg->arg.bitmap = ucg_scale_2x2+b;
  ucg->arg.len = len;
  ucg->arg.dir = dir;
  ucg->scale_chain_device_cb(ucg, msg, &(ucg->arg));  

  ucg->arg.pixel.pos = *xy;
  switch(dir)
  {
    case 0: ucg->arg.pixel.pos.y++; break;
    case 1: ucg->arg.pixel.pos.x++; break;
    case 2: ucg->arg.pixel.pos.y++; ucg->arg.pixel.pos.x++; break;
    default: case 3: ucg->arg.pixel.pos.x++; ucg->arg.pixel.pos.y++;  break;
  }
  ucg->arg.bitmap = ucg_scale_2x2+b;
  ucg->arg.len = len;
  ucg->arg.dir = dir;
  ucg->scale_chain_device_cb(ucg, msg, &(ucg->arg));  
  
  switch(dir)
  {
    case 0: xy->x+=len; break;
    case 1: xy->y+=len; break;
    case 2: xy->x-=len; break;
    default: case 3: xy->y-=len; break;
  }
  
}

ucg_int_t ucg_dev_scale2x2(ucg_t *ucg, ucg_int_t msg, void *data)
{
  ucg_xy_t xy;
  ucg_int_t len;
  ucg_int_t dir;
  switch(msg)
  {
    case UCG_MSG_GET_DIMENSION:
      ucg->scale_chain_device_cb(ucg, msg, data); 
      ((ucg_wh_t *)data)->h /= 2;
      ((ucg_wh_t *)data)->w /= 2;
    
      //printf("rw=%d rh=%d\n", ucg->rotate_dimension.w, ucg->rotate_dimension.h);
      //printf("aw=%d ah=%d\n",  ((ucg_wh_t volatile * volatile )data)->w, ((ucg_wh_t volatile * volatile )data)->h);
      //printf("dw=%d dh=%d\n", ucg->dimension.w, ucg->dimension.h);
      return 1;
      
    case UCG_MSG_SET_CLIP_BOX:
      ((ucg_box_t * )data)->ul.y *= 2; 
      ((ucg_box_t * )data)->ul.x *= 2; 
      ((ucg_box_t * )data)->size.h *= 2;
      ((ucg_box_t * )data)->size.w *= 2;
      
      //printf("post clipbox x=%d y=%d\n", ((ucg_box_t * )data)->ul.x, ((ucg_box_t * )data)->ul.y);
      break;
    case UCG_MSG_DRAW_PIXEL:
      xy = ucg->arg.pixel.pos;
      ucg->arg.pixel.pos.x *= 2;
      ucg->arg.pixel.pos.y *= 2;
      ucg->scale_chain_device_cb(ucg, msg, data); 
      ucg->arg.pixel.pos.x++;
      ucg->scale_chain_device_cb(ucg, msg, data); 
      ucg->arg.pixel.pos.y++;
      ucg->scale_chain_device_cb(ucg, msg, data); 
      ucg->arg.pixel.pos.x--;
      ucg->scale_chain_device_cb(ucg, msg, data); 
      ucg->arg.pixel.pos = xy;
      return 1;
    case UCG_MSG_DRAW_L90SE:
    case UCG_MSG_DRAW_L90FX:
      xy = ucg->arg.pixel.pos;
      len = ucg->arg.len;
      dir = ucg->arg.dir;
    
    
      ucg->arg.pixel.pos.x *= 2;
      ucg->arg.pixel.pos.y *= 2;

      switch(dir)
      {
	case 0: break;
	case 1: break;
	case 2: ucg->arg.pixel.pos.x++; break;
	default: case 3: ucg->arg.pixel.pos.y++; break;
      }
    
      ucg->arg.len *= 2;
      ucg->scale_chain_device_cb(ucg, msg, data);  
    
      ucg->arg.pixel.pos = xy;
      ucg->arg.pixel.pos.x *= 2;
      ucg->arg.pixel.pos.y *= 2;
      ucg->arg.len = len*2;
      ucg->arg.dir = dir;
      switch(dir)
      {
	case 0: ucg->arg.pixel.pos.y++; break;
	case 1: ucg->arg.pixel.pos.x++; break;
	case 2: ucg->arg.pixel.pos.y++; ucg->arg.pixel.pos.x++; break;
	default: case 3: ucg->arg.pixel.pos.x++; ucg->arg.pixel.pos.y++;  break;
      }
      ucg->scale_chain_device_cb(ucg, msg, data);
      
      ucg->arg.pixel.pos = xy;
      ucg->arg.len = len;
      ucg->arg.dir = dir;
      return 1;
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
    case UCG_MSG_DRAW_L90BF:
#endif /* UCG_MSG_DRAW_L90BF */

#if defined(UCG_MSG_DRAW_L90TC) || defined(UCG_MSG_DRAW_L90BF)
      xy = ucg->arg.pixel.pos;
      len = ucg->arg.len;
      dir = ucg->arg.dir;

      ucg->arg.pixel.pos.x *= 2;
      ucg->arg.pixel.pos.y *= 2;
    
      {
	const unsigned char *b = ucg->arg.bitmap;
	ucg_xy_t my_xy = ucg->arg.pixel.pos;
	ucg_int_t i;
	for( i = 8; i < len; i+=8 )
	{
	  ucg_scale_2x2_send_next_half_byte(ucg, &my_xy, msg, 4, dir, ucg_pgm_read(b)>>4);
	  ucg_scale_2x2_send_next_half_byte(ucg, &my_xy, msg, 4, dir, ucg_pgm_read(b));
	  b+=1;
	}
	i = len+8-i;
	if ( i > 4 )
	{
	  ucg_scale_2x2_send_next_half_byte(ucg, &my_xy, msg, 4, dir, ucg_pgm_read(b)>>4);
	  ucg_scale_2x2_send_next_half_byte(ucg, &my_xy, msg, i-4, dir, ucg_pgm_read(b));
	}
	else
	{
	  ucg_scale_2x2_send_next_half_byte(ucg, &my_xy, msg, i, dir, ucg_pgm_read(b)>>4);
	}
      }
      ucg->arg.pixel.pos = xy;
      ucg->arg.len = len;
      ucg->arg.dir = dir;
      return 1;
#endif 
  }
  return ucg->scale_chain_device_cb(ucg, msg, data);  
}

/* Side-Effects: Update dimension and reset clip range to max */
void ucg_SetScale2x2(ucg_t *ucg)
{
  ucg_UndoScale(ucg);
  ucg->scale_chain_device_cb = ucg->device_cb;
  ucg->device_cb = ucg_dev_scale2x2;
  ucg_GetDimension(ucg);
  ucg_SetMaxClipRange(ucg);
}
