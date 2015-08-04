/*

  ucg_dev_ic_seps225.c
  
  Specific code for the SEPS225 controller (OLED displays)
  128x128x18 display buffer
  Note: Only 65K color support, because 262K mode is not possible with 8 Bit SPI.

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

static uint8_t ucg_seps225_get_color_high_byte(ucg_t *ucg)
{
    return (ucg->arg.pixel.rgb.color[0]&0x0f8) | (((ucg->arg.pixel.rgb.color[1]) >>5));
}

static uint8_t ucg_seps225_get_color_low_byte(ucg_t *ucg)
{
    return ((((ucg->arg.pixel.rgb.color[1]))<<3)&0x0e0) | (((ucg->arg.pixel.rgb.color[2]) >>3));
}


/*
  Write Memory Mode 0x016
  Bit 0: HV
    HV= 0, data is continuously written horizontally 
    HV= 1, data is continuously written vertically 
  Bit 1: VC 
    VC= 0, vertical address counter is decreased 
    VC= 1, vertical address counter is increased 
  Bit 2: HC 
    HC= 0, horizontal address counter is decreased 
    HC= 1, horizontal address counter is increased 
  Bit 4..6: Dual Transfer, 65K Mode (Bit 4 = 0, Bit 5 = 1, Bit 6 = 1)
*/

static const ucg_pgm_uint8_t ucg_seps255_pos_dir0_seq[] = 
{
  UCG_CS(0),							/* enable chip */
  UCG_C10(0x020),	UCG_VARX(0,0x07f, 0), 	/* set x position */
  UCG_C10(0x021),	UCG_VARY(0,0x07f, 0), 	/* set y position */    
  UCG_C11(0x016, 0x064),				/* Memory Mode */
  UCG_C10(0x022),						/* prepare for data */
  UCG_DATA(),							/* change to data mode */
  UCG_END()
};

static const ucg_pgm_uint8_t ucg_seps255_pos_dir1_seq[] = 
{
  UCG_CS(0),							/* enable chip */
  UCG_C11(0x016, 0x063),				/* Memory Mode */
  UCG_C10(0x020),	UCG_VARX(0,0x07f, 0), 	/* set x position */
  UCG_C10(0x021),	UCG_VARY(0,0x07f, 0), 	/* set y position */    
  UCG_C10(0x022),						/* prepare for data */
  UCG_DATA(),							/* change to data mode */
  UCG_END()
};

#ifdef NOT_USED
static const ucg_pgm_uint8_t ucg_seps255_pos_dir2_seq[] = 
{
  UCG_CS(0),							/* enable chip */
  UCG_C11(0x016, 0x060),				/* Memory Mode */
  UCG_C10(0x020),	UCG_VARX(0,0x0ff, 0), 	/* set x position */
  UCG_C10(0x021),	UCG_VARY(0,0x0ff, 0), 	/* set y position */    
  UCG_C10(0x022),						/* prepare for data */
  UCG_DATA(),							/* change to data mode */
  UCG_END()
};

/* it seems that this mode does not work*/
static const ucg_pgm_uint8_t ucg_seps255_pos_dir3_seq[] = 
{
  UCG_CS(0),							/* enable chip */
  UCG_C11(0x016, 0x061),				/* Memory Mode */
  UCG_C10(0x020),	UCG_VARX(0,0x0ff, 0), 	/* set x position */
  UCG_C10(0x021),	UCG_VARY(0,0x0ff, 0), 	/* set y position */    
  UCG_C10(0x022),						/* prepare for data */
  UCG_DATA(),							/* change to data mode */
  UCG_END()
};
#endif 

ucg_int_t ucg_handle_seps225_l90fx(ucg_t *ucg)
{
  if ( ucg_clip_l90fx(ucg) != 0 )
  {
    switch(ucg->arg.dir)
    {
      case 0: 
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir0_seq);	
	break;
      case 1: 
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir1_seq);	
	break;
      case 2: 
	ucg->arg.pixel.pos.x -= ucg->arg.len;
	ucg->arg.pixel.pos.x++;
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir0_seq);	
	break;
      case 3: 
      default: 
	ucg->arg.pixel.pos.y -= ucg->arg.len;
	ucg->arg.pixel.pos.y++;
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir1_seq);	
	break;
    }
    
    {
      uint8_t c[2];
      c[0] = ucg_seps225_get_color_high_byte(ucg);
      c[1] = ucg_seps225_get_color_low_byte(ucg);
      ucg_com_SendRepeat2Bytes(ucg, ucg->arg.len, c);
    }
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}




/*
  L2TC (Bitmap Output)

  Not implemented for this controller, fallback to standard. However this msg is not used
  at all at the moment (Jul 2015)
  
*/

/*
  L2SE Color Change

*/


ucg_int_t ucg_handle_seps225_l90se(ucg_t *ucg)
{
  uint8_t i;
  uint8_t c[3];
  
  /* Setup ccs for l90se. This will be updated by ucg_clip_l90se if required */

  switch(ucg->arg.dir)
  {
    case 0:
    case 1:
      for ( i = 0; i < 3; i++ )
      {
	ucg_ccs_init(ucg->arg.ccs_line+i, ucg->arg.rgb[0].color[i], ucg->arg.rgb[1].color[i], ucg->arg.len);
      }
      break;
    default:
      for ( i = 0; i < 3; i++ )
      {
	ucg_ccs_init(ucg->arg.ccs_line+i, ucg->arg.rgb[1].color[i], ucg->arg.rgb[0].color[i], ucg->arg.len);
      }
      break;
  }
  
  /* check if the line is visible */
  
  if ( ucg_clip_l90se(ucg) != 0 )
  {
    ucg_int_t dx, dy;
    ucg_int_t i, j;
    //uint8_t r, g, b;
    switch(ucg->arg.dir)
    {
      case 0: dx = 1; dy = 0; 
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir0_seq);	
	break;
      case 1: dx = 0; dy = 1; 
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir1_seq);	
	break;
      case 2: dx = 1; dy = 0; 
	ucg->arg.pixel.pos.x -= ucg->arg.len;
	ucg->arg.pixel.pos.x++;
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir0_seq);	
	break;
      case 3: dx = 0; dy = 1; 
	ucg->arg.pixel.pos.y -= ucg->arg.len;
	ucg->arg.pixel.pos.y++;
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir1_seq);	
	break;
      default: dx = 1; dy = 0; break;	/* avoid compiler warning */
    }
    for( i = 0; i < ucg->arg.len; i++ )
    {
      /*
      r = ucg->arg.ccs_line[0].current;
      r &= 0x0f8;
      g = ucg->arg.ccs_line[1].current;
      c[0] = g;
      c[0] >>= 5;
      c[0] |= r;
      g <<= 3;
      g &= 0x0e0;
      b = ucg->arg.ccs_line[2].current;
      b >>= 3;
      c[1] = g;
      c[1] |= b;
      */
      c[0] = (ucg->arg.ccs_line[0].current&0x0f8) | (((ucg->arg.ccs_line[1].current) >>5));
      c[1] = ((((ucg->arg.ccs_line[1].current))<<3)&0x0e0) | (((ucg->arg.ccs_line[2].current) >>3));
      ucg_com_SendRepeat2Bytes(ucg, 1, c);
      ucg->arg.pixel.pos.x+=dx;
      ucg->arg.pixel.pos.y+=dy;
      for ( j = 0; j < 3; j++ )
      {
	ucg_ccs_step(ucg->arg.ccs_line+j);
      }
    }
    ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
    return 1;
  }
  return 0;
}


static const ucg_pgm_uint8_t ucg_seps225_power_down_seq[] = {
	UCG_CS(0),					/* enable chip */
	UCG_C11(0x006, 0x000),		/* SEPS225: display off */
	UCG_C11(0x004, 0x001),		/* reduce current, power down */
	UCG_CS(1),					/* disable chip */
	/* delay of 100ms is suggested here for full power down */
	UCG_END(),					/* end of sequence */  
};


ucg_int_t ucg_dev_ic_seps225_16(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* setup com interface and provide information on the clock speed */
      /* of the serial and parallel interface. Values are nanoseconds. */
      return ucg_com_PowerUp(ucg, 50, 200);	/* adjusted for SEPS225 */
    case UCG_MSG_DEV_POWER_DOWN:
      ucg_com_SendCmdSeq(ucg, ucg_seps225_power_down_seq);
      return 1;
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 128;
      return 1;
    case UCG_MSG_DRAW_PIXEL:
      if ( ucg_clip_is_pixel_visible(ucg) !=0 )
      {
	uint8_t c[3];
	ucg_com_SendCmdSeq(ucg, ucg_seps255_pos_dir0_seq);	
	c[0] = ucg_seps225_get_color_high_byte(ucg);
	c[1] = ucg_seps225_get_color_low_byte(ucg);
	ucg_com_SendRepeat2Bytes(ucg, 1, c);
	ucg_com_SetCSLineStatus(ucg, 1);		/* disable chip */
      }
      return 1;
    case UCG_MSG_DRAW_L90FX:
      //ucg_handle_l90fx(ucg, ucg_dev_ic_seps225_16);
      ucg_handle_seps225_l90fx(ucg);
      return 1;
#ifdef UCG_MSG_DRAW_L90TC
    case UCG_MSG_DRAW_L90TC:
      ucg_handle_l90tc(ucg, ucg_dev_ic_seps225_18);
      return 1;
#endif /* UCG_MSG_DRAW_L90TC */
#ifdef UCG_MSG_DRAW_L90BF
     case UCG_MSG_DRAW_L90BF:
      ucg_handle_l90bf(ucg, ucg_dev_ic_seps225_18);
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

ucg_int_t ucg_ext_seps225_16(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DRAW_L90SE:
      //ucg_handle_l90se(ucg, ucg_dev_ic_seps225_16);
      ucg_handle_seps225_l90se(ucg);
      break;
  }
  return 1;
}
