/*

  ucg_dev_oled_96x64_univision.c
  
  Specific code for the Univision 0.95" OLED (UG-9664, 96x94 pixel, 65536 Colors, SSD1331)

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

static const ucg_pgm_uint8_t ucg_univision_ssd1331_init_seq[] = {
	UCG_CFG_CD(0,0),				/* First arg: level for commands, Second arg: level for command arguments */
  	UCG_RST(1),					
	UCG_CS(1),					/* disable chip */
	UCG_DLY_MS(1),
  	UCG_RST(0),					
	UCG_DLY_MS(1),
  	UCG_RST(1),
	UCG_DLY_MS(50),
	UCG_CS(0),					/* enable chip */
  
	//UCG_C11(0x0fd, 0x012),			/* Unlock normal commands, reset default: unlocked */
	UCG_C10(0x0ae),				/* Set Display Off */
  	//UCG_C10(0x0af),				/* Set Display On */
  	UCG_C11(0x0a0, 0x0b2),			/* 65k format 2, RGB Mode */
  	UCG_C11(0x0a1, 0x000),			/* Set Display Start Line */
  	UCG_C11(0x0a2, 0x000),			/* Set Display Offset */
  	UCG_C11(0x0a8, 0x03f),			/* Multiplex, reset value = 0x03f  */
  	UCG_C11(0x0ad, 0x08e),			/* select supply (must be set before 0x0af) */
  
  	UCG_C11(0x0b0, 0x00b),		/* Disable power save mode */
  	UCG_C11(0x0b1, 0x031),		/* Set Phase Length, reset default: 0x74 */
  	UCG_C11(0x0b3, 0x0f0),			/* Display Clock Divider/Osc, reset value=0x0d0 */

  	UCG_C12(0x015, 0x000, 0x05f),	/* Set Column Address */
  	UCG_C12(0x075, 0x000, 0x03f),	/* Set Row Address */

  	UCG_C11(0x081, 0x080),		/* contrast red, Adafruit: 0x091, UC9664: 0x080 */
  	UCG_C11(0x082, 0x080),		/* contrast green, Adafruit: 0x050, UC9664: 0x080 */
  	UCG_C11(0x083, 0x080),		/* contrast blue, Adafruit: 0x07d, UC9664: 0x080  */
  	UCG_C11(0x087, 0x00f),			/* master current/contrast 0x00..0x0f UG-9664: 0x0f, Adafruit: 0x06 */
  	UCG_C11(0x08a, 0x064),			/* second precharge speed red */
  	UCG_C11(0x08b, 0x078),		/* second precharge speed green */
  	UCG_C11(0x08c, 0x064),			/* second precharge speed blue */
  	UCG_C11(0x0bb, 0x03c),			/* set shared precharge level, default = 0x03e */
  	UCG_C11(0x0be, 0x03e),			/* voltage select, default = 0x03e */

	UCG_C10(0x0b9),				/* Reset internal grayscale lookup */
  	// UCG_C10(0x0b8),				/* Set CMD Grayscale Lookup, 63 Bytes follow */
	// UCG_A8(0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c),
	// UCG_A8(0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14),
	// UCG_A8(0x15,0x16,0x18,0x1a,0x1b,0x1C,0x1D,0x1F),
	// UCG_A8(0x21,0x23,0x25,0x27,0x2A,0x2D,0x30,0x33),
	// UCG_A8(0x36,0x39,0x3C,0x3F,0x42,0x45,0x48,0x4C),
	// UCG_A8(0x50,0x54,0x58,0x5C,0x60,0x64,0x68,0x6C),
	// UCG_A8(0x70,0x74,0x78,0x7D,0x82,0x87,0x8C,0x91),
	// UCG_A7(0x96,0x9B,0xA0,0xA5,0xAA,0xAF,0xB4),

	UCG_C10(0x0a4),				/* Normal display mode */
  	UCG_C10(0x0af),				/* Set Display On */
	
	
  	//UCG_C12(0x015, 0x030, 0x05f),	/* Set Column Address */
  	//UCG_C12(0x075, 0x010, 0x03f),	/* Set Row Address */
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
	//UCG_D3(0x0ff, 0, 0),
 
	UCG_CS(1),					/* disable chip */
	UCG_END(),					/* end of sequence */
};

ucg_int_t ucg_dev_ssd1331_18x96x64_univision(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_ssd1331_18(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_univision_ssd1331_init_seq);      
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the controller procedures */
      return ucg_dev_ic_ssd1331_18(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 96;
      ((ucg_wh_t *)data)->h = 64;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_ssd1331_18(ucg, msg, data);  
}
