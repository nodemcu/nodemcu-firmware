/*

  ucg_dev_oled_160x128_samsung.c
  
  Specific code for the Samsung 160x128 OLED (LD50T6160 Controller)

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
    Samsung 1.8" OLED
    VCC_R (4) connected to 4.7uF
    VCC_R1 (41) connected to 4.7uF
    VCC connected to 18V
    VCC3 connected to 18V
    
    ==> Internal regulator mit be enabled
    ==> DC-DC must be switched off

    From the Samsung 1.8" OLED datasheet:
    
    PREC_WIDTH = 06h, (pre charge width?)
    PEAKDELAY = 01h, 
    Frame Freq.(*1)) = 02h(90Hz)
    
    Red : PEAKWIDTH = 03h, 
    Green : PEAKWIDTH = 05h, 
    Blue : PEAKWIDTH = 03h
    
    Red : DOT CURRENT = 9Ah, 
    Green : DOT CURRENT = 5Ch, 
    Blue : DOT CURRENT = B1h
    
*/


static const ucg_pgm_uint8_t ucg_samsung_160x128_init_seq[] = {
  /* init sequence for the Samsung OLED with LD50T6160 controller */
	UCG_CFG_CD(0,1),				/* DC=0 for command mode, DC=1 for data and args */
  	UCG_RST(1),					
	UCG_CS(1),					/* disable chip */
	UCG_DLY_MS(1),
  	UCG_RST(0),					
	UCG_DLY_MS(1),
  	UCG_RST(1),
	UCG_DLY_MS(150),
	UCG_CS(0),					/* enable chip */
  
  
	UCG_C10(0x01),				/* software reset */
	UCG_C11(0x02, 0x00),			/* display on/off: display off */
	UCG_C11(0x03, 0x00),			/* standby off, OSCA start */  
	UCG_C11(0x04, 0x02),			/* set frame rate: 0..7, 60Hz to 150Hz, default is 2=90Hz */
	UCG_C11(0x05, 0x00),			/* write dir, bit 3: RGB/BGR */
	UCG_C11(0x06, 0x00),			/* row scan dir (0=default) */
  
	UCG_C11(0x06, 0x00),			/* row scan dir (0=default) */
  
	UCG_C10(0x07),				/* set screen size */
	UCG_A2(0x00, 0x00),			/* x min = 0 */
	UCG_A2(0x07, 0x0f),			/* x max = 127 */
	UCG_A2(0x00, 0x00),			/* y min = 0 */
	UCG_A2(0x09, 0x0f),			/* y max = 159 */
	
  	UCG_C11(0x08, 0x00),			/* interface bus width: 6 bit */
  	UCG_C11(0x09, 0x07),			/* no data (RGB) masking (default) */
 
 
	UCG_C10(0x0a),				/* write box (probably not req. during init */
	UCG_A2(0x00, 0x00),			/* x min = 0 */
	UCG_A2(0x07, 0x0f),			/* x max = 127 */
	UCG_A2(0x00, 0x00),			/* y min = 0 */
	UCG_A2(0x09, 0x0f),			/* y max = 159 */

	UCG_C10(0x0e),				/* set some minimal dot current first */
	UCG_A2(0x00, 0x03),			/* red */
	UCG_A2(0x00, 0x03),			/* green */
	UCG_A2(0x00, 0x03),			/* blue */

	UCG_C11(0x1b, 0x03),			/* Pre-Charge Mode Select:Enable Pre-Charge and Peakboot for the Samsung OLED */
	UCG_C12(0x1c, 0x00, 0x06),		/* Pre-Charge Width, Samsung OLED: 0x06, controller default is 0x08 */

	UCG_C10(0x1d),				/* Peak Pulse Width Set */
	UCG_A2(0x00, 0x03),			/* red */
	UCG_A2(0x00, 0x05),			/* green */
	UCG_A2(0x00, 0x03),			/* blue */

  	UCG_C11(0x1e, 0x01),			/* Peak Pulse Delay Set, Samsung OLED: 0x01, controller default is 0x05  */
	
  /* 
    Row Scan Operation Set (cmd 0x1f)
    
    bits 0, 1: scan mode
      mode 00: alternate scan mode (Oliver: not sure if this is true, prob. this is seq. mode)	
      mode 01: seq. scan mode (Oliver: could be mode 00)
      mode 10: simultaneous scan mode (Oliver: No idea what this is)
      Samsung OLED: probably mode 00 
    bit 3:
      if set to 1, then all rows are connected to GND
      Samsung OLED: must be 0 for normal operation
    bit 4,5:	row timing setting
      00:	none
      01:	pre charge
      10:	pre charge + peak delay
      11:	pre charge + peak delay + peak boot
      Samsung OLED: No idea, maybe 11 must be set here
  */
  	UCG_C11(0x1f, 0x30),			/* Samsung OLED:  pre charge + peak delay + peak boot */
	
  	UCG_C11(0x2d, 0x00),			/* Write data through system interface (bit 4 = 0) */

/*
  ICON Display control
  bit 0/1 = 01: ALL ICON ON
  bit 0/1 = 10: normal display mode (depends on ICON data)
*/
  	UCG_C11(0x20, 0x01),			/* ICON Display control */
  	UCG_C11(0x21, 0x00),			/* ICON Stand-by Setting, bit 0 = 0: no standby, start OSCB */
  	UCG_C11(0x29, 0x00),			/* DC-DC Control for ICON: bit 0&1 = 01: stop DC-DC, use VICON (Samsung OLED has external 18V) */
  	UCG_C11(0x2a, 0x04),			/* ICON Frame Frequency Set: 89Hz */
	
  	UCG_C11(0x30, 0x13),			/* Internal Regulator for Row Scan, value taken from Samsung OLED flow chart */

	/* 
	  Set full dot current 
	  Red : DOT CURRENT = 9Ah, 
	  Green : DOT CURRENT = 5Ch, 
	  Blue : DOT CURRENT = B1h
	*/
	UCG_DLY_MS(10),
	UCG_C10(0x0e),				/* set dot current */
	UCG_A2(0x09, 0x0a),			/* red */
	UCG_A2(0x05, 0x0c),			/* green */
	UCG_A2(0x0b, 0x01),			/* blue */
	UCG_DLY_MS(10),

  	UCG_C11(0x02, 0x01),			/* Display on */

	UCG_CS(1),					/* disable chip */
	UCG_END(),					/* end of sequence */
};

ucg_int_t ucg_dev_ld50t6160_18x160x128_samsung(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_ld50t6160_18(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_samsung_160x128_init_seq);
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the controller procedures */
      return ucg_dev_ic_ld50t6160_18(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 128;
      ((ucg_wh_t *)data)->h = 160;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_ld50t6160_18(ucg, msg, data);  
}
