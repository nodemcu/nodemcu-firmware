/*

  ucg_dev_tft_128x128_ili9163.c
  
  ILI9341 with 4-Wire SPI (SCK, SDI, CS, D/C and optional reset)

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



static const ucg_pgm_uint8_t ucg_tft_128x128_ili9163_init_seq[] = {
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

  //UCG_C14(0x0ed, 0x055, 0x001, 0x023, 0x001), 	/* power on sequence control (POR values) */
  //UCG_C11(0x0f7, 0x020), 		/* pump ratio control (POR value) */
  

  UCG_C10(0x20), 				/* not inverted */
  //UCG_C10(0x21), 				/* inverted */

  UCG_C11(0x03a, 0x066), 		/* set pixel format to 18 bit */
  //UCG_C11(0x03a, 0x055), 		/* set pixel format to 16 bit */

  //UCG_C12(0x0b1, 0x000, 0x01b), 	/* frame rate control (POR values) */

  //UCG_C14(0x0b6, 0x00a, 0x082 | (1<<5), 0x027, 0x000), 	/* display function control (POR values, except for shift direction bit) */
  
  //UCG_C11(0x0b7, 0x006), 		/* entry mode, bit 0: Low voltage detection control (0=off) */

  UCG_C12(0x0c0, 0x00a, 0x002), 	/* power control 1 */
  UCG_C11(0x0c1, 0x002), 		/* power control 2 (step up factor), POR=2 */
  UCG_C11(0x0c7, 0x0d0), 		/* VCOM control 2, enable VCOM control 1 */
  UCG_C12(0x0c5, 0x040, 0x04a), 	/* VCOM control 1, POR=31,3C */
  
 // UCG_C15(0x0cb, 0x039, 0x02c, 0x000, 0x034, 0x002), 	/* power control A (POR values) */
 // UCG_C13(0x0cf, 0x000, 0x081, 0x030), 	/* power control B (POR values) */

 // UCG_C13(0x0e8, 0x084, 0x011, 0x07a), 	/* timer driving control A (POR values) */

//  UCG_C12(0x0ea, 0x066, 0x000), 	/* timer driving control B (POR values) */
  //UCG_C12(0x0ea, 0x000, 0x000), 		/* timer driving control B  */

  //UCG_C10(0x28), 				/* display off */
  //UCG_C11(0x0bf, 0x003), 		/* backlight control 8 */
  UCG_C10(0x029), 				/* display on */
  //UCG_C11(0x051, 0x07f), 		/* brightness */
  //UCG_C11(0x053, 0x02c), 		/* control brightness */
  //UCG_C10(0x028), 				/* display off */


  UCG_C11( 0x036, 0x008),
  
  UCG_C14(  0x02a, 0x000, 0x000, 0x000, 0x07f),              /* Horizontal GRAM Address Set */
  UCG_C14(  0x02b, 0x000, 0x000, 0x000, 0x07f),              /* Vertical GRAM Address Set */
  UCG_C10(  0x02c),               /* Write Data to GRAM */

  
  UCG_CS(1),					/* disable chip */
  UCG_END(),					/* end of sequence */
};

ucg_int_t ucg_dev_ili9163_18x128x128(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_ili9163_18(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_tft_128x128_ili9163_init_seq);
      
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the conroller procedures */
      return ucg_dev_ic_ili9163_18(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 128;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_ili9163_18(ucg, msg, data);  
}
