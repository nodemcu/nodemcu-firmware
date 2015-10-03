/*

  ucg_dev_ic_pcf8833.c
  
  Specific code for the pcf8833 controller (TFT displays)

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

#define XOFFSET 0

#define WIDTH 132
#define HEIGHT 132

const ucg_pgm_uint8_t ucg_pcf8833_set_pos_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  UCG_C11( 0x036, 0x000),
  UCG_C10(0x02a),	UCG_VARX(0,0x0ff, 0), UCG_A1(WIDTH-1),					/* set x position */
  UCG_C10(0x02b),	UCG_VARY(0,0x0ff, 0), UCG_A1(HEIGHT-1),		/* set y position */
  UCG_C10(0x02c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};


const ucg_pgm_uint8_t ucg_pcf8833_set_pos_dir0_seq[] = 
{
  UCG_CS(0),					/* enable chip */

  /*
    bit 7: 	my
    bit 6: 	mx
    bit 5: 	x or y increment
    bit 4:	line order
    bit 3:	rgb
  */
  UCG_C11( 0x036, 0x000),
  UCG_C10(0x02a),	UCG_VARX(0,0x0ff, 0), UCG_A1(WIDTH-1),					/* set x position */
  UCG_C10(0x02b),	UCG_VARY(0,0x0ff, 0), UCG_A1(HEIGHT-1),		/* set y position */

  UCG_C10(0x02c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_pcf8833_set_pos_dir1_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  /*
    bit 7: 	my
    bit 6: 	mx
    bit 5: 	x or y increment
    bit 4:	line order
    bit 3:	rgb
  */
  UCG_C11( 0x036, 0x000),
  
  UCG_C10(0x02a),	UCG_VARX(0,0x0ff, 0), UCG_VARX(0,0x0ff, 0),					/* set x position */
  UCG_C10(0x02b),	UCG_VARY(0,0x0ff, 0), UCG_A1(HEIGHT-1),		/* set y position */

  UCG_C10(0x02c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_pcf8833_set_pos_dir2_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  
  /*
    bit 7: 	my
    bit 6: 	mx
    bit 5: 	x or y increment
    bit 4:	line order
    bit 3:	rgb
  */
  UCG_C11( 0x036, 0x040),
  //UCG_C11( 0x036, 0x020),			/* it seems that this command needs to be sent twice */
  UCG_C10(0x02a),	UCG_VARX(0,0x0ff, 0), UCG_A1(WIDTH-1),					/* set x position */
  UCG_C10(0x02b),	UCG_VARY(0,0x0ff, 0), UCG_A1(HEIGHT-1),		/* set y position */

  UCG_C10(0x02c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

const ucg_pgm_uint8_t ucg_pcf8833_set_pos_dir3_seq[] = 
{
  UCG_CS(0),					/* enable chip */
  
  /*
    bit 7: 	my
    bit 6: 	mx
    bit 5: 	x or y increment
    bit 4:	line order
    bit 3:	rgb
  */
  UCG_C11( 0x036, 0x080),
  //UCG_C11( 0x036, 0x0d0),		/* it seems that this command needs to be sent twice */
  UCG_C10(0x02a),	UCG_VARX(0,0x0ff, 0), UCG_VARX(0,0x0ff, 0),					/* set x position */
  UCG_C10(0x02b),	UCG_VARY(0,0x0ff, 0), UCG_A1(HEIGHT-1),		/* set y position */

  UCG_C10(0x02c),							/* write to RAM */
  UCG_DATA(),								/* change to data mode */
  UCG_END()
};

static uint8_t ucg_pcf8833_get_color_high_byte(ucg_t *ucg)
{
    return (ucg->arg.pixel.rgb.color[0]&0x0f8) | (((ucg->arg.pixel.rgb.color[1]) >>5));
}

static uint8_t ucg_pcf8833_get_color_low_byte(ucg_t *ucg)
{
    return ((((ucg->arg.pixel.rgb.color[1]))<<3)&0x0e0) | (((ucg->arg.pixel.rgb.color[2]) >>3));
}

ucg_int_t ucg_handle_pcf8833_l90fx(ucg_t *ucg)
{
  uint8_t c[3];
  ucg_int_t tmp;
  if ( ucg_clip_l90fx(ucg) != 0 )
  {
    switch(ucg->arg.dir)
    {
      case 0: 
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir0_seq);	
	break;
      case 1: 
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir1_seq);	
	break;
      case 2: 
	tmp = ucg->arg.pixel.pos.x;
	ucg->arg.pixel.pos.x = WIDTH-1-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir2_seq);	
	ucg->arg.pixel.pos.x = tmp;
	break;
      case 3: 
      default: 
	tmp = ucg->arg.pixel.pos.y;
	ucg->arg.pixel.pos.y = HEIGHT-1-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir3_seq);	
	ucg->arg.pixel.pos.y = tmp;
	break;
    }
    c[0] = ucg_pcf8833_get_color_high_byte(ucg);
    c[1] = ucg_pcf8833_get_color_low_byte(ucg);
    ucg_com_SendRepeat2Bytes(ucg, ucg->arg.len, c);
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}

/*
  L2TC (Glyph Output)
  
*/

/* with CmdDataSequence */ 
ucg_int_t ucg_handle_pcf8833_l90tc(ucg_t *ucg)
{
  if ( ucg_clip_l90tc(ucg) != 0 )
  {
    uint8_t buf[16];
    ucg_int_t dx, dy;
    ucg_int_t i;
    unsigned char pixmap;
    uint8_t bitcnt;
    ucg_com_SetCSLineStatus(ucg, 0);		/* enable chip */
    ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_seq);	

    buf[0] = 0x001;	// change to 0 (cmd mode)
    buf[1] = 0x02a;	// set x
    buf[2] = 0x002;	// change to 1 (arg mode)
    buf[3] = 0x000;	// x/y
    buf[4] = 0x000;	// no change
    buf[5] = 0x000;	// end value
    buf[6] = 0x001;	// change to 0 (cmd mode)
    buf[7] = 0x02c;	// write data
    buf[8] = 0x002;	// change to 1 (data mode)
    buf[9] = ucg_pcf8833_get_color_high_byte(ucg);
    buf[10] = 0x000;	// no change
    buf[11] = ucg_pcf8833_get_color_low_byte(ucg);
    
    switch(ucg->arg.dir)
    {
      case 0: 
	dx = 1; dy = 0; 
	buf[1] = 0x02a;	// set x
	buf[5] = WIDTH-1;	// end value
	break;
      case 1: 	
	dx = 0; dy = 1; 
        buf[1] = 0x02b;	// set y
	buf[5] = HEIGHT-1;	// end value
	break;
      case 2: 
	dx = -1; dy = 0; 
        buf[1] = 0x02a;	// set x
	buf[5] = WIDTH-1;	// end value
	break;
      case 3: 
      default:
	dx = 0; dy = -1; 
        buf[1] = 0x02b;	// set y
	buf[5] = HEIGHT-1;	// end value
	break;
    }
    pixmap = ucg_pgm_read(ucg->arg.bitmap);
    bitcnt = ucg->arg.pixel_skip;
    pixmap <<= bitcnt;
    
    for( i = 0; i < ucg->arg.len; i++ )
    {
      if ( (pixmap & 128) != 0 )
      {
	if ( (ucg->arg.dir&1) == 0 )
	{
	  buf[3] = ucg->arg.pixel.pos.x;
	}
	else
	{
	  buf[3] = ucg->arg.pixel.pos.y;
	}
	ucg_com_SendCmdDataSequence(ucg, 6, buf, 0);
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


ucg_int_t ucg_handle_pcf8833_l90se(ucg_t *ucg)
{
  uint8_t i;
  uint8_t c[3];
  ucg_int_t tmp;
  
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
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir0_seq);	
	break;
      case 1: 
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir1_seq);	
	break;
      case 2: 
	tmp = ucg->arg.pixel.pos.x;
	ucg->arg.pixel.pos.x = WIDTH-1-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir2_seq);	
	ucg->arg.pixel.pos.x = tmp;
	break;
      case 3: 
      default: 
	tmp = ucg->arg.pixel.pos.y;
	ucg->arg.pixel.pos.y = HEIGHT-1-tmp;
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_dir3_seq);	
	ucg->arg.pixel.pos.y = tmp;
	break;
    }
    
    for( i = 0; i < ucg->arg.len; i++ )
    {
      
      c[0] = (ucg->arg.ccs_line[0].current&0x0f8) | (((ucg->arg.ccs_line[1].current) >>5));
      c[1] = ((((ucg->arg.ccs_line[1].current))<<3)&0x0e0) | (((ucg->arg.ccs_line[2].current) >>3));
      
      ucg_com_SendRepeat2Bytes(ucg, 1, c);
      ucg_ccs_step(ucg->arg.ccs_line+0);
      ucg_ccs_step(ucg->arg.ccs_line+1);
      ucg_ccs_step(ucg->arg.ccs_line+2);
    }
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}

static const ucg_pgm_uint8_t ucg_pcf8833_power_down_seq[] = {
	UCG_CS(0),					/* enable chip */
	UCG_C10(0x28), 				/* display off */	
	UCG_C10(0x10), 				/* sleep in */	
	UCG_CS(1),					/* disable chip */
	UCG_END(),					/* end of sequence */
};



ucg_int_t ucg_dev_ic_pcf8833_16(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      //ucg->display_offset.x = 2;
      //ucg->display_offset.y = 2;

      /* setup com interface and provide information on the clock speed */
      /* of the serial and parallel interface. Values are nanoseconds. */
      return ucg_com_PowerUp(ucg, 150, 160);
    case UCG_MSG_DEV_POWER_DOWN:
      ucg_com_SendCmdSeq(ucg, ucg_pcf8833_power_down_seq);
      return 1;
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = WIDTH;
      ((ucg_wh_t *)data)->h = HEIGHT;
      return 1;
    case UCG_MSG_DRAW_PIXEL:
      if ( ucg_clip_is_pixel_visible(ucg) !=0 )
      {
	uint8_t c[3];
	ucg_com_SendCmdSeq(ucg, ucg_pcf8833_set_pos_seq);	
	c[0] = ucg_pcf8833_get_color_high_byte(ucg);
	c[1] = ucg_pcf8833_get_color_low_byte(ucg);
	ucg_com_SendRepeat2Bytes(ucg, 1, c);
	ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
      }
      return 1;
    case UCG_MSG_DRAW_L90FX:
      //ucg_handle_l90fx(ucg, ucg_dev_ic_pcf8833_16);
      ucg_handle_pcf8833_l90fx(ucg);
      return 1;
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
      //ucg_handle_l90tc(ucg, ucg_dev_ic_pcf8833_16);
      ucg_handle_pcf8833_l90tc(ucg);
      return 1;	
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
     case UCG_MSG_DRAW_L90BF:
      ucg_handle_l90bf(ucg, ucg_dev_ic_pcf8833_16);
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

ucg_int_t ucg_ext_pcf8833_16(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DRAW_L90SE:
      //ucg_handle_l90se(ucg, ucg_dev_ic_pcf8833_16);
      ucg_handle_pcf8833_l90se(ucg);
      break;
  }
  return 1;
}
