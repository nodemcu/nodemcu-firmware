/*

  ucg_dev_default_cb.c
  
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

/* 
  default device callback
  this should be (finally) called by any other device callback to handle
  messages, which are not yet handled.
*/

ucg_int_t ucg_dev_default_cb(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DRAW_L90SE:
      return ucg->ext_cb(ucg, msg, data);
    case UCG_MSG_SET_CLIP_BOX:
      ucg->clip_box = *(ucg_box_t *)data;
      break;
  }
  return 1;	/* all ok */
}

/*
  will be used as default cb if no extentions callback is provided
*/
ucg_int_t ucg_ext_none(ucg_t *ucg, ucg_int_t msg, void *data)
{
  return 1;	/* all ok */  
}



/*
  handle UCG_MSG_DRAW_L90FX message and make calls to "dev_cb" with UCG_MSG_DRAW_PIXEL
  return 1 if something has been drawn
*/
ucg_int_t ucg_handle_l90fx(ucg_t *ucg, ucg_dev_fnptr dev_cb)
{
  if ( ucg_clip_l90fx(ucg) != 0 )
  {
    ucg_int_t dx, dy;
    ucg_int_t i;
    switch(ucg->arg.dir)
    {
      case 0: dx = 1; dy = 0; break;
      case 1: dx = 0; dy = 1; break;
      case 2: dx = -1; dy = 0; break;
      case 3: dx = 0; dy = -1; break;
      default: dx = 1; dy = 0; break;	/* avoid compiler warning */
    }
    for( i = 0; i < ucg->arg.len; i++ )
    {
      dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
    }
    return 1;
  }
  return 0;
}


/*
  handle UCG_MSG_DRAW_L90TC message and make calls to "dev_cb" with UCG_MSG_DRAW_PIXEL
  return 1 if something has been drawn
*/
ucg_int_t ucg_handle_l90tc(ucg_t *ucg, ucg_dev_fnptr dev_cb)
{
  if ( ucg_clip_l90tc(ucg) != 0 )
  {
    ucg_int_t dx, dy;
    ucg_int_t i;
    unsigned char pixmap;
    uint8_t bitcnt;
    switch(ucg->arg.dir)
    {
      case 0: dx = 1; dy = 0; break;
      case 1: dx = 0; dy = 1; break;
      case 2: dx = -1; dy = 0; break;
      case 3: dx = 0; dy = -1; break;
      default: dx = 1; dy = 0; break;	/* avoid compiler warning */
    }
    //pixmap = *(ucg->arg.bitmap);
    pixmap = ucg_pgm_read(ucg->arg.bitmap);
    bitcnt = ucg->arg.pixel_skip;
    pixmap <<= bitcnt;
    for( i = 0; i < ucg->arg.len; i++ )
    {
      if ( (pixmap & 128) != 0 )
      {
	dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
      }
      pixmap<<=1;
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
      bitcnt++;
      if ( bitcnt >= 8 )
      {
	ucg->arg.bitmap++;
	//pixmap = *(ucg->arg.bitmap);
	pixmap = ucg_pgm_read(ucg->arg.bitmap);
	bitcnt = 0;
      }
    }
    return 1;
  }
  return 0;
}


/*
  handle UCG_MSG_DRAW_L90FB message and make calls to "dev_cb" with UCG_MSG_DRAW_PIXEL
  return 1 if something has been drawn
*/
ucg_int_t ucg_handle_l90bf(ucg_t *ucg, ucg_dev_fnptr dev_cb)
{
  if ( ucg_clip_l90tc(ucg) != 0 )
  {
    ucg_int_t dx, dy;
    ucg_int_t i, y;
    unsigned char pixmap;
    uint8_t bitcnt;
    switch(ucg->arg.dir)
    {
      case 0: dx = 1; dy = 0; break;
      case 1: dx = 0; dy = 1; break;
      case 2: dx = -1; dy = 0; break;
      case 3: dx = 0; dy = -1; break;
      default: dx = 1; dy = 0; break;	/* avoid compiler warning */
    }
    pixmap = ucg_pgm_read(ucg->arg.bitmap);
    bitcnt = ucg->arg.pixel_skip;
    pixmap <<= bitcnt;
    for( i = 0; i < ucg->arg.len; i++ )
    {
      for( y = 0; y < ucg->arg.scale; y++ )
      {
	if ( (pixmap & 128) == 0 )
	{
	  ucg->arg.pixel.rgb = ucg->arg.rgb[1];
	  dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
	}
	else
	{
	  ucg->arg.pixel.rgb = ucg->arg.rgb[0];
	  dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
	}
	ucg->arg.pixel.pos.x+=dx;
	ucg->arg.pixel.pos.y+=dy;
      }
      pixmap<<=1;
      bitcnt++;
      if ( bitcnt >= 8 )
      {
	ucg->arg.bitmap++;
	pixmap = ucg_pgm_read(ucg->arg.bitmap);
	bitcnt = 0;
      }
    }
    return 1;
  }
  return 0;
}

/*
  handle UCG_MSG_DRAW_L90SE message and make calls to "dev_cb" with UCG_MSG_DRAW_PIXEL
  return 1 if something has been drawn
*/

ucg_int_t ucg_handle_l90se(ucg_t *ucg, ucg_dev_fnptr dev_cb)
{
  uint8_t i;
  
  /* Setup ccs for l90se. This will be updated by ucg_clip_l90se if required */
  
  for ( i = 0; i < 3; i++ )
  {
    ucg_ccs_init(ucg->arg.ccs_line+i, ucg->arg.rgb[0].color[i], ucg->arg.rgb[1].color[i], ucg->arg.len);
  }
  
  /* check if the line is visible */
  
  if ( ucg_clip_l90se(ucg) != 0 )
  {
    ucg_int_t dx, dy;
    ucg_int_t i, j;
    switch(ucg->arg.dir)
    {
      case 0: dx = 1; dy = 0; break;
      case 1: dx = 0; dy = 1; break;
      case 2: dx = -1; dy = 0; break;
      case 3: dx = 0; dy = -1; break;
      default: dx = 1; dy = 0; break;	/* avoid compiler warning */
    }
    for( i = 0; i < ucg->arg.len; i++ )
    {
      ucg->arg.pixel.rgb.color[0] = ucg->arg.ccs_line[0].current;
      ucg->arg.pixel.rgb.color[1] = ucg->arg.ccs_line[1].current; 
      ucg->arg.pixel.rgb.color[2] = ucg->arg.ccs_line[2].current;
      dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
      for ( j = 0; j < 3; j++ )
      {
	ucg_ccs_step(ucg->arg.ccs_line+j);
      }
    }
    return 1;
  }
  return 0;
}

/*
  l90rl run lenth encoded constant color pattern

  yyllllll wwwwbbbb wwwwbbbb wwwwbbbb wwwwbbbb ...
  sequence terminates if wwwwbbbb = 0
  wwww: number of pixel to skip
  bbbb: number of pixel to draw with color
  llllll: number of bytes which follow, can be 0
*/

#ifdef ON_HOLD
void ucg_handle_l90rl(ucg_t *ucg, ucg_dev_fnptr dev_cb)
{
  ucg_int_t dx, dy;
  uint8_t i, cnt;
  uint8_t rl_code;
  ucg_int_t skip;
  
  switch(ucg->arg.dir)
  {
    case 0: dx = 1; dy = 0; break;
    case 1: dx = 0; dy = 1; break;
    case 2: dx = -1; dy = 0; break;
    case 3: dx = 0; dy = -1; break;
    default: dx = 1; dy = 0; break;	/* avoid compiler warning */
  }
    
  cnt = ucg_pgm_read(ucg->arg.bitmap);
  cnt &= 63;
  ucg->arg.bitmap++;
  for( i = 0; i < cnt; i++ )
  {
    rl_code = ucg_pgm_read(ucg->arg.bitmap);
    if ( rl_code == 0 )
      break;
    
    skip = (ucg_int_t)(rl_code >> 4);
    switch(ucg->arg.dir)
    {
      case 0: ucg->arg.pixel.pos.x+=skip; break;
      case 1: ucg->arg.pixel.pos.y+=skip; break;
      case 2: ucg->arg.pixel.pos.x-=skip; break;
      default:
      case 3: ucg->arg.pixel.pos.y-=skip; break;
    }

    rl_code &= 15;
    while( rl_code )
    {
      dev_cb(ucg, UCG_MSG_DRAW_PIXEL, NULL);
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
      rl_code--;
    }
    ucg->arg.bitmap++;
  }
}
#endif