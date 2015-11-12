/*

  ucg_dev_ic_ld50t6160.c
  
  Specific code for the ld50t6160 controller (OLED displays)

  Universal uC Color Graphics Library
  
  Copyright (c) 2015, olikraus@gmail.com
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
  Graphics Memory Writing Direction: 0x05
  Bit 3: RGB/BGR
  Bit 0-2: Direction
    0x05, 0x00		dir 0
    0x05, 0x05		dir 1
    0x05, 0x03		dir 2
    0x05, 0x06		dir 3
  
  Data Reading/Writing Box: 0x0a
    8 bytes as arguments: xs, ys, xe, ye
    
*/

const ucg_pgm_uint8_t ucg_ld50t6160_set_pos_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  UCG_C11(0x05, 0x00),
  UCG_C10(0x0a), UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0), UCG_A2(0x007, 0x0f),					/* set x position */
  UCG_VARY(4,0x0f, 0), UCG_VARY(0,0x0f, 0), UCG_A2(0x09, 0x0f),		/* set y position */
  UCG_C10(0x0c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};


const ucg_pgm_uint8_t ucg_ld50t6160_set_pos_dir0_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  
  UCG_C11(0x05, 0x00),
  UCG_C10(0x0a), 
    UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0), UCG_A2(0x007, 0x0f),					/* set x position */
    UCG_VARY(4,0x0f, 0), UCG_VARY(0,0x0f, 0), UCG_A2(0x09, 0x0f),		/* set y position */
  UCG_C10(0x0c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_ld50t6160_set_pos_dir1_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  UCG_C11(0x05, 0x05),
  UCG_C10(0x0a), 
    //UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0), UCG_D2(0x007, 0x0f),					/* set x position */
    UCG_A2(0x00, 0x00), UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0), 		/* set x position */
    UCG_VARY(4,0x0f, 0), UCG_VARY(0,0x0f, 0), UCG_A2(0x09, 0x0f),		/* set y position */
  UCG_C10(0x0c),							/* write to RAM */  
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_ld50t6160_set_pos_dir2_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  
  UCG_C11(0x05, 0x03),
  UCG_C10(0x0a), 
    UCG_A2(0x00, 0x00), UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0),					/* set x position */
    UCG_A2(0x00, 0x00), UCG_VARY(4,0x0f, 0), UCG_VARY(0,0x0f, 0),		/* set y position */
  UCG_C10(0x0c),							/* write to RAM */  
  
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_ld50t6160_set_pos_dir3_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  
  UCG_C11(0x05, 0x06),
  UCG_C10(0x0a), 
    UCG_VARX(4,0x0f, 0), UCG_VARX(0,0x0f, 0), UCG_A2(0x07, 0x0f), 					/* set x position */
    UCG_A2(0x00, 0x00), UCG_VARY(4,0x0f, 0), UCG_VARY(0,0x0f, 0),		/* set y position */
  UCG_C10(0x0c),							/* write to RAM */  
  
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

static ucg_int_t ucg_handle_ld50t6160_l90fx(ucg_t *ucg)
{
  uint8_t c[3];
  //ucg_int_t tmp;
  if ( ucg_clip_l90fx(ucg) != 0 )
  {
    switch(ucg->arg.dir)
    {
      case 0: 
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir0_seq);	
	break;
      case 1: 
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir1_seq);	
	break;
      case 2: 
	//tmp = ucg->arg.pixel.pos.x;
	//ucg->arg.pixel.pos.x = 127-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir2_seq);	
	//ucg->arg.pixel.pos.x = tmp;
	break;
      case 3: 
      default: 
	//tmp = ucg->arg.pixel.pos.y;
	//ucg->arg.pixel.pos.y = 159-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir3_seq);	
	//ucg->arg.pixel.pos.y = tmp;
	break;
    }
    c[0] = ucg->arg.pixel.rgb.color[0]>>2;
    c[1] = ucg->arg.pixel.rgb.color[1]>>2;
    c[2] = ucg->arg.pixel.rgb.color[2]>>2;
    ucg_com_SendRepeat3Bytes(ucg, ucg->arg.len, c);
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}

/*
  L2TC (Glyph Output)
*/

#ifdef UCG_MSG_DRAW_L90TC
static ucg_int_t ucg_handle_ld50t6160_l90tc(ucg_t *ucg)
{
  if ( ucg_clip_l90tc(ucg) != 0 )
  {
    uint8_t buf[3];
    ucg_int_t dx, dy;
    ucg_int_t i;
    const uint8_t *seq;
    unsigned char pixmap;
    uint8_t bitcnt;
    ucg_com_SetCSLineStatus(ucg, 0);		/* enable chip */
    switch(ucg->arg.dir)
    {
      case 0: 
	dx = 1; dy = 0; 
	seq = ucg_ld50t6160_set_pos_dir0_seq;
	break;
      case 1: 
	dx = 0; dy = 1; 
	seq = ucg_ld50t6160_set_pos_dir1_seq;
	break;
      case 2: 
	dx = -1; dy = 0; 
	seq = ucg_ld50t6160_set_pos_dir2_seq;
	break;
      case 3: 
      default:
	dx = 0; dy = -1; 
	seq = ucg_ld50t6160_set_pos_dir3_seq;
	break;
    }
    pixmap = ucg_pgm_read(ucg->arg.bitmap);
    bitcnt = ucg->arg.pixel_skip;
    pixmap <<= bitcnt;
    buf[0] = ucg->arg.pixel.rgb.color[0]>>2;
    buf[1] = ucg->arg.pixel.rgb.color[1]>>2;
    buf[2] = ucg->arg.pixel.rgb.color[2]>>2;
    
    for( i = 0; i < ucg->arg.len; i++ )
    {
      if ( (pixmap & 128) != 0 )
      {
	ucg_com_SendCmdSeq(ucg, seq);	
	ucg_com_SendRepeat3Bytes(ucg, 1, buf);
      }
      pixmap<<=1;
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
      bitcnt++;
      if ( bitcnt >= 8 )
      {
	ucg->arg.bitmap++;
	pixmap = ucg_pgm_read(ucg->arg.bitmap);
	bitcnt = 0;
      }
    }
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}
#endif /* UCG_MSG_DRAW_L90TC */

ucg_int_t ucg_handle_ld50t6160_l90se(ucg_t *ucg)
{
  uint8_t i;
  uint8_t c[3];
  //ucg_int_t tmp;
  
  /* Setup ccs for l90se. This will be updated by ucg_clip_l90se if required */
  
  for ( i = 0; i < 3; i++ )
  {
    ucg_ccs_init(ucg->arg.ccs_line+i, ucg->arg.rgb[0].color[i], ucg->arg.rgb[1].color[i], ucg->arg.len);
  }
  
  /* check if the line is visible */
  
  if ( ucg_clip_l90se(ucg) != 0 )
  {
    ucg_int_t i;
    switch(ucg->arg.dir)
    {
      case 0: 
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir0_seq);	
	break;
      case 1: 
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir1_seq);	
	break;
      case 2: 
	//tmp = ucg->arg.pixel.pos.x;
	//ucg->arg.pixel.pos.x = 239-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir2_seq);	
	//ucg->arg.pixel.pos.x = tmp;
	break;
      case 3: 
      default: 
	//tmp = ucg->arg.pixel.pos.y;
	//ucg->arg.pixel.pos.y = 319-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_dir3_seq);	
	//ucg->arg.pixel.pos.y = tmp;
	break;
    }
    
    for( i = 0; i < ucg->arg.len; i++ )
    {
      c[0] = ucg->arg.ccs_line[0].current>>2;
      c[1] = ucg->arg.ccs_line[1].current>>2; 
      c[2] = ucg->arg.ccs_line[2].current>>2;
      ucg_com_SendRepeat3Bytes(ucg, 1, c);
      ucg_ccs_step(ucg->arg.ccs_line+0);
      ucg_ccs_step(ucg->arg.ccs_line+1);
      ucg_ccs_step(ucg->arg.ccs_line+2);
    }
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}

static const ucg_pgm_uint8_t ucg_ld50t6160_power_down_seq[] = {
	UCG_CS(0),					/* enable chip */
	UCG_C11(0x02, 0x00),			/* display off */
  	UCG_C11(0x20, 0x00),			/* all ICON off */
  	UCG_C11(0x20, 0x01),			/* stop OSCB */
  

	UCG_C10(0x010),				/* sleep in */
	UCG_C10(0x28), 				/* display off */	
	UCG_CS(1),					/* disable chip */
	UCG_END(),					/* end of sequence */
};


ucg_int_t ucg_dev_ic_ld50t6160_18(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* setup com interface and provide information on the clock speed */
      /* of the serial and parallel interface. Values are nanoseconds. */
      return ucg_com_PowerUp(ucg, 100, 66);
    case UCG_MSG_DEV_POWER_DOWN:
      ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_power_down_seq);
      return 1;
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 160;
      return 1;
    case UCG_MSG_DRAW_PIXEL:
      if ( ucg_clip_is_pixel_visible(ucg) !=0 )
      {
	uint8_t c[3];
	ucg_com_SendCmdSeq(ucg, ucg_ld50t6160_set_pos_seq);	
	c[0] = ucg->arg.pixel.rgb.color[0]>>2;
	c[1] = ucg->arg.pixel.rgb.color[1]>>2;
	c[2] = ucg->arg.pixel.rgb.color[2]>>2;
	ucg_com_SendRepeat3Bytes(ucg, 1, c);
	ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
      }
      return 1;
    case UCG_MSG_DRAW_L90FX:
      //ucg_handle_l90fx(ucg, ucg_dev_ic_ld50t6160_18);
      ucg_handle_ld50t6160_l90fx(ucg);
      return 1;
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
      //ucg_handle_l90tc(ucg, ucg_dev_ic_ld50t6160_18);
      ucg_handle_ld50t6160_l90tc(ucg);
      return 1;
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
     case UCG_MSG_DRAW_L90BF:
      ucg_handle_l90bf(ucg, ucg_dev_ic_ld50t6160_18);
      return 1;
#endif /* UCG_MSG_DRAW_L90BF */
    /* msg UCG_MSG_DRAW_L90SE is handled by ucg_dev_default_cb */
    /*
    case UCG_MSG_DRAW_L90SE:
      return ucg->ext_cb(ucg, msg, data);
    */
  }
  return ucg_dev_default_cb(ucg, msg, data);  
}

ucg_int_t ucg_ext_ld50t6160_18(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DRAW_L90SE:
      //ucg_handle_l90se(ucg, ucg_dev_ic_ld50t6160_18);
      ucg_handle_ld50t6160_l90se(ucg);
      break;
  }
  return 1;
}