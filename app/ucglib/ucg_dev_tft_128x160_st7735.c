/*

  ucg_dev_tft_128x160_st7735.c
  
  ST7735 with 4-Wire SPI (SCK, SDI, CS, D/C and optional reset)

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

//static const uint8_t ucg_dev_ssd1351_128x128_init_seq[] PROGMEM = {
static const ucg_pgm_uint8_t ucg_tft_128x160_st7735_init_seq[] = {
  UCG_CFG_CD(0,1),				/* DC=0 for command mode, DC=1 for data and args */
  UCG_RST(1),					
  UCG_CS(1),					/* disable chip */
  UCG_DLY_MS(5),
  UCG_RST(0),					
  UCG_DLY_MS(5),
  UCG_RST(1),
  UCG_DLY_MS(50),
  UCG_CS(0),					/* enable chip */
  
  UCG_C10(0x011),				/* sleep out */
  UCG_DLY_MS(10),
  //UCG_C10(0x038),				/* idle mode off */
  UCG_C10(0x013),				/* normal display on */

  

  UCG_C10(0x20), 				/* not inverted */
  //UCG_C10(0x21), 				/* inverted */

  UCG_C11(0x03a, 0x006), 		/* set pixel format to 18 bit */
  //UCG_C11(0x03a, 0x005), 		/* set pixel format to 16 bit */

  //UCG_C12(0x0b1, 0x000, 0x01b), 	/* frame rate control (POR values) */
  //UCG_C10(0x28), 				/* display off */
  //UCG_C11(0x0bf, 0x003), 		/* backlight control 8 */
  UCG_C10(0x029), 				/* display on */
  //UCG_C11(0x051, 0x07f), 		/* brightness */
  //UCG_C11(0x053, 0x02c), 		/* control brightness */
  //UCG_C10(0x028), 				/* display off */


  UCG_C11( 0x036, 0x000),		/* memory control */
  
  UCG_C14(  0x02a, 0x000, 0x000, 0x000, 0x07f),              /* Horizontal GRAM Address Set */
  UCG_C14(  0x02b, 0x000, 0x000, 0x000, 0x09f),              /* Vertical GRAM Address Set */
  UCG_C10(  0x02c),               /* Write Data to GRAM */

  
  UCG_CS(1),					/* disable chip */
  UCG_END(),					/* end of sequence */
};

ucg_int_t ucg_dev_st7735_18x128x160(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_st7735_18(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_tft_128x160_st7735_init_seq);
      
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the conroller procedures */
      return ucg_dev_ic_st7735_18(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 160;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_st7735_18(ucg, msg, data);  
}
