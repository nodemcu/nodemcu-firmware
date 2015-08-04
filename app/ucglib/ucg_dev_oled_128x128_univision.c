/*

  ucg_dev_oled_128x128_univision.c
  
  Specific code for the Univision 1.5" OLED (UG2828, 128x128 pixel, 65K Colors, SEPS225)
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

/* the following code is from the UG2828GDEAF02 Datasheet */
static const ucg_pgm_uint8_t aaa_ucg_univision_seps225_init_seq[] = {
	UCG_CFG_CD(0,1),				/* First arg: level for commands, Second arg: level for command arguments */
  	UCG_RST(1),					
	UCG_CS(1),					/* disable chip */
	UCG_DLY_MS(1),
  	UCG_RST(0),					
	UCG_DLY_MS(1),
  	UCG_RST(1),
	UCG_DLY_MS(50),
	UCG_CS(0),					/* enable chip */
  
  	UCG_C11(0x002, 0x001),		/* OSC_CTL: external resistor, internal OSC */
  	UCG_C11(0x004, 0x000),		/* REDUCE_CURRENT: normal operation, no current reduction */
  	UCG_C11(0x003, 0x030),		/* CLK_DIV: 90Hz, ratio 1 */
  
  	UCG_C11(0x008, 0x000),		/* Precharge time red */
  	UCG_C11(0x009, 0x000),		/* Precharge time green */
  	UCG_C11(0x00a, 0x000),			/* Precharge time blue */

  	UCG_C11(0x00b, 0x000),		/* Precharge current red */
  	UCG_C11(0x00c, 0x000),			/* Precharge current green */
  	UCG_C11(0x00d, 0x000),		/* Precharge current blue */


  	UCG_C11(0x010, 0x04a),			/* Driving current red */
  	UCG_C11(0x011, 0x025),		/* Driving current green, original value 0x011 replaced by 0x025 */
  	UCG_C11(0x012, 0x02f),			/* Driving current blue */

  	UCG_C11(0x013, 0x000),			/* Display mode: RGB,column=0..127,column data display control=Normal Dispaly */
 	UCG_C11(0x014, 0x001),		/* External interface: 18 bit, MPU, Bit size does not matter if MPU is selected*/
 	//UCG_C11(0x016, 0x076),		/* Memory Mode: 6bits triple transfer (would require 6bit words on SPI, 262K support, horizontal inc., vertical inc., data continuously written horizontally */
 	UCG_C11(0x016, 0x066),		/* Memory Mode: dual transfer (2x 8Bit), 65K support, horizontal inc., vertical inc., data continuously written horizontally */

	/* memory window */
  	UCG_C11(0x017, 0x000),		/* x0 */
  	UCG_C11(0x018, 0x07f),			/* x1 */
  	UCG_C11(0x019, 0x000),		/* y0 */
  	UCG_C11(0x01a, 0x07f),			/* y1 */

	/* memory start */
  	UCG_C11(0x020, 0x000),		/* x */
  	UCG_C11(0x021, 0x000),		/* y */

  	UCG_C11(0x028, 0x07f),		/* Duty */
  	UCG_C11(0x029, 0x000),		/* Display start line */

	/* DDRAM start */
  	UCG_C11(0x02e, 0x000),			/* x */
  	UCG_C11(0x02f, 0x000),			/* y */

  	UCG_C11(0x033, 0x000),		/* Display Screen Saver x0 */
  	UCG_C11(0x034, 0x07f),			/* Display Screen Saver x1 */
  	UCG_C11(0x035, 0x000),		/* Display Screen Saver y0 */
  	UCG_C11(0x036, 0x07f),			/* Display Screen Saver y1 */

  	UCG_C11(0x080, 0x000),		/* IREF: External */

	UCG_DLY_MS(100),				/* as suggested in the Univision manual */

  	UCG_C11(0x006, 0x001),		/* Display on */


	UCG_CS(1),					/* disable chip */
	UCG_END(),					/* end of sequence */
};

/* init seq. from the .ino example from ZhangFeng, PRC. http://vfdclock.jimdo.com */
static const ucg_pgm_uint8_t ucg_univision_seps225_init_seq[] = {
	UCG_CFG_CD(0,1),				/* First arg: level for commands, Second arg: level for command arguments */
  	UCG_RST(1),					
	UCG_CS(1),					/* disable chip */
	UCG_DLY_MS(1),
  	UCG_RST(0),					
	UCG_DLY_MS(1),
  	UCG_RST(1),
	UCG_DLY_MS(50),
	UCG_CS(0),					/* enable chip */
  
  	UCG_C11(0x006, 0x000),		/* Display off */
  
  	UCG_C11(0x002, 0x001),		/* OSC_CTL: external resistor, internal OSC */
  	UCG_C11(0x004, 0x000),		/* REDUCE_CURRENT: normal operation, no current reduction */
  	UCG_C11(0x003, 0x030),		/* CLK_DIV: 90Hz, ratio 1 */
  
	UCG_C11(0x005, 0x000),		/* Software Reset */
    
  
  	UCG_C11(0x008, 0x003),		/* Precharge time red */
  	UCG_C11(0x009, 0x004),		/* Precharge time green */
  	UCG_C11(0x00a, 0x005),			/* Precharge time blue */

  	UCG_C11(0x00b, 0x00b),		/* Precharge current red */
  	UCG_C11(0x00c, 0x00b),			/* Precharge current green */
  	UCG_C11(0x00d, 0x00e),		/* Precharge current blue */


  	UCG_C11(0x010, 0x03e),			/* Driving current red */
  	UCG_C11(0x011, 0x032),		/* Driving current green, original value 0x011 replaced by 0x025 */
  	UCG_C11(0x012, 0x03d),		/* Driving current blue */

  	UCG_C11(0x013, 0x000),			/* Display mode: RGB,column=0..127,column data display control=Normal Dispaly */
 	UCG_C11(0x014, 0x001),		/* External interface: 18 bit, MPU, Bit size does not matter if MPU is selected*/
 	//UCG_C11(0x016, 0x076),		/* Memory Mode: 6bits triple transfer (would require 6bit words on SPI, 262K support, horizontal inc., vertical inc., data continuously written horizontally */
	UCG_C11(0x015, 0x000),		
 	UCG_C11(0x016, 0x066),		/* Memory Mode: dual transfer (2x 8Bit), 65K support, horizontal inc., vertical inc., data continuously written horizontally */

	/* memory window */
  	UCG_C11(0x017, 0x000),		/* x0 */
  	UCG_C11(0x018, 0x07f),			/* x1 */
  	UCG_C11(0x019, 0x000),		/* y0 */
  	UCG_C11(0x01a, 0x07f),			/* y1 */

	/* memory start */
  	UCG_C11(0x020, 0x000),		/* x */
  	UCG_C11(0x021, 0x000),		/* y */

  	UCG_C11(0x028, 0x07f),		/* Duty */
  	UCG_C11(0x029, 0x000),		/* Display start line */

	/* DDRAM start */
  	UCG_C11(0x02e, 0x000),			/* x */
  	UCG_C11(0x02f, 0x000),			/* y */

  	UCG_C11(0x031, 0x000),		
  	UCG_C11(0x032, 0x000),		

  	UCG_C11(0x033, 0x000),		/* Display Screen Saver x0 */
  	UCG_C11(0x034, 0x07f),			/* Display Screen Saver x1 */
  	UCG_C11(0x035, 0x000),		/* Display Screen Saver y0 */
  	UCG_C11(0x036, 0x07f),			/* Display Screen Saver y1 */

  	UCG_C11(0x080, 0x000),		/* IREF: External */

	UCG_DLY_MS(100),				/* as suggested in the Univision manual */

  	UCG_C11(0x006, 0x001),		/* Display on */
  	//UCG_C10(0x022),


	UCG_CS(1),					/* disable chip */
	//UCG_DATA(),
	UCG_END(),					/* end of sequence */
};


static const ucg_pgm_uint8_t ucg_seps255_pos_dir0_seq[] = 
{
  UCG_CS(0),							/* enable chip */
  UCG_C11(0x016, 0x066),				/* Memory Mode */
  UCG_C11(0x020, 0),	 	/* set x position */
  UCG_C11(0x021, 0),	 	/* set y position */    
  UCG_C10(0x022),						/* prepare for data */
  UCG_DATA(),							/* change to data mode */
  UCG_END()
};

ucg_int_t ucg_dev_seps225_16x128x128_univision(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_seps225_16(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_univision_seps225_init_seq);      
      
      /* demonstration of the OLED error */
      /*
      {
	  uint8_t r,g,b, i;
	  uint8_t c[3];

	  ucg_com_SendCmdSeq(ucg,  ucg_seps255_pos_dir0_seq);      

	    r = 0;
	    g = 0;
	    b = 80;
	    c[0] = (r&0x0f8) | (((g) >>5));
	    c[1] = ((((g))<<3)&0x0e0) | (((b) >>3));
	    ucg_com_SendRepeat2Bytes(ucg, 128, c);


	    r = 0;
	    g = 0;
	    b = 0;
	    c[0] = (r&0x0f8) | (((g) >>5));
	    c[1] = ((((g))<<3)&0x0e0) | (((b) >>3));
	
	  for( i = 0; i < 126; i++ )
	  {
	    ucg_com_SendRepeat2Bytes(ucg, 128, c);
	  }

	    r = 0;
	    g = 0;
	    b = 255;
	    c[0] = (r&0x0f8) | (((g) >>5));
	    c[1] = ((((g))<<3)&0x0e0) | (((b) >>3));
	
	    ucg_com_SendRepeat2Bytes(ucg, 40, c);

	  
	  for(;;)
	    ;

      }
      */
      
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the controller procedures */
      return ucg_dev_ic_seps225_16(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 128;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_seps225_16(ucg, msg, data);  
}
