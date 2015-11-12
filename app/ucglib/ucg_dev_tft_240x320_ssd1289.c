/*

  ucg_dev_tft_240x320_ssd1289.c
  
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

#ifdef NOT_YET_IMPLEMENTED

static const ucg_pgm_uint8_t ucg_tft_240x320_ssd1289_init_seq[] = {
  UCG_CFG_CD(0,1),				/* DC=0 for command mode, DC=1 for data and args */
  UCG_RST(1),					
  UCG_CS(1),					/* disable chip */
  UCG_DLY_MS(5),
  UCG_RST(0),					
  UCG_DLY_MS(5),
  UCG_RST(1),
  UCG_DLY_MS(50),
  UCG_CS(0),					/* enable chip */
  UCG_C22(0x000, 0x000, 0x000, 0x001), 	/* enable oscilator */
  UCG_C22(0x000, 0x003, 0x066, 0x064),              /* Power control 1 (POR value) */
  
  //UCG_C22(0x000, 0x002, 0x007, 0x000),              /* LCD Driving Wave Control, bit 9: Set line inversion */
  //UCG_C22(0x000, 0x003, 0x010, 0x030),              /* Entry Mode, GRAM write direction and BGR (Bit 12)=1 (16 bit transfer, 65K Mode)*/
  //UCG_C22(0x000, 0x004, 0x000, 0x000),              /* Resize register, all 0: no resize */
  UCG_C22(0x000, 0x008, 0x002, 0x007),              /* Display Control 2: set the back porch and front porch */
  //UCG_C22(0x000, 0x009, 0x000, 0x000),              /* Display Control 3: normal scan */
  //UCG_C22(0x000, 0x00a, 0x000, 0x000),              /* Display Control 4: set to "no FMARK output" */
  UCG_C22(0x000, 0x00c, 0x000, 0x000),              /* RGB Display Interface Control 1, RIM=10 (3x6 Bit), 12 Jan 14: RIM=00  */
  //UCG_C22(0x000, 0x00d, 0x000, 0x000),              /* Frame Maker Position */
  //UCG_C22(0x000, 0x00f, 0x000, 0x000),               /* RGB Display Interface Control 2 */
  UCG_C22(0x000, 0x010, 0x000, 0x000),              /* Power Control 1: SAP, BT[3:0], AP, DSTB, SLP, STB, actual setting is done below */
  UCG_C22(0x000, 0x011, 0x000, 0x007),              /* Power Control 2: DC1[2:0], DC0[2:0], VC[2:0] */
  UCG_C22(0x000, 0x012, 0x000, 0x000),              /* Power Control 3: VREG1OUT voltage */
  UCG_C22(0x000, 0x013, 0x000, 0x000),              /* Power Control 4: VDV[4:0] for VCOM amplitude */
  UCG_C22(0x000, 0x007, 0x000, 0x001),              /* Display Control 1: Operate, but do not display */
  UCG_DLY_MS(100),         /* delay 100 ms */  /*  ITDB02 none D verion:  50ms */
  UCG_C22(  0x000, 0x010, 0x010, 0x090),              /* Power Control 1: SAP, BT[3:0], AP, DSTB, SLP, STB */
										  /*  12. Jan 14. Prev value: 0x016, 0x090 */
										  /*  ITDB02 none D verion:  0x010, 0x090 */
  //UCG_C22(  0x000, 0x010, 0x017, 0x0f0),              /* Power Control 1: Setting for max quality & power consumption: SAP(Bit 12)=1, BT[3:0]=7 (max), APE (Bit 8)=1, AP=7 (max), disable sleep: SLP=0, STB=0 */
  UCG_C22(  0x000, 0x011, 0x002, 0x027),              /* Power Control 2 VCI ration, step up circuits 1 & 2 */
  UCG_DLY_MS(50),         /* delay 50 ms */
  
  UCG_C22(  0x000, 0x012, 0x000, 0x01f),              /* Power Control 3: VCI: External, VCI*1.80 */
										  /*  12. Jan 14. Prev value: 0x000, 0x00d */
  UCG_DLY_MS(50),         /* delay 50 ms */
  UCG_C22(  0x000, 0x013, 0x015, 0x000),              /* Power Control 4: VDV[4:0] for VCOM amplitude */
										  /*  12. Jan 14. Prev value: 0x012, 0x009 */
  UCG_C22(  0x000, 0x029, 0x000, 0x027),              /* Power Control 7 */
										  /*  12. Jan 14. Prev value: 0x000, 0x00a */
  UCG_C22(  0x000, 0x02b, 0x000, 0x00d),              /* Frame Rate: 93 */
  //UCG_C22(  0x000, 0x02b, 0x000, 0x00b),              /* Frame Rate: 70, too less, some flicker visible */
  UCG_DLY_MS(50),         /* delay 50 ms */

  /* Gamma Control, values from iteadstudio.com reference software on google code */
  /*
  UCG_C22(0x00,0x30,0x00,0x00),
  UCG_C22(0x00,0x31,0x07,0x07),
  UCG_C22(0x00,0x32,0x03,0x07),
  UCG_C22(0x00,0x35,0x02,0x00),
  UCG_C22(0x00,0x36,0x00,0x08),
  UCG_C22(0x00,0x37,0x00,0x04),
  UCG_C22(0x00,0x38,0x00,0x00),
  UCG_C22(0x00,0x39,0x07,0x07),
  UCG_C22(0x00,0x3C,0x00,0x02),
  UCG_C22(0x00,0x3D,0x1D,0x04),
  */

  UCG_C22(  0x000, 0x020, 0x000, 0x000),              /* Horizontal GRAM Address Set */
  UCG_C22(  0x000, 0x021, 0x000, 0x000),              /* Vertical GRAM Address Set */
  
  UCG_C22(  0x000, 0x050, 0x000, 0x000),              /* Horizontal GRAM Start Address */
  UCG_C22(  0x000, 0x051, 0x000, 0x0EF),              /* Horizontal GRAM End Address: 239 */
  UCG_C22(  0x000, 0x052, 0x000, 0x000),              /* Vertical GRAM Start Address */
  UCG_C22(  0x000, 0x053, 0x001, 0x03F),              /* Vertical GRAM End Address: 319 */
  
  UCG_C22(  0x000, 0x060, 0x0a7, 0x000),              /* Driver Output Control 2, NL = 0x027 = 320 lines, GS (bit 15) = 1 */
  UCG_C22(  0x000, 0x061, 0x000, 0x001),              /* Base Image Display Control: NDL,VLE = 0 (Disbale Vertical Scroll), REV */
  //UCG_C22(  0x000, 0x06a, 0x000, 0x000),              /* Vertical Scroll Control */
  //UCG_C22(  0x000, 0x080, 0x000, 0x000),              /* Partial Image 1 Display Position */
  //UCG_C22(  0x000, 0x081, 0x000, 0x000),              /* Partial Image 1 RAM Start Address */
  //UCG_C22(  0x000, 0x082, 0x000, 0x000),              /* Partial Image 1 RAM End Address */
  //UCG_C22(  0x000, 0x083, 0x000, 0x000),              /* Partial Image 2 Display Position */
  //UCG_C22(  0x000, 0x084, 0x000, 0x000),              /* Partial Image 2 RAM Start Address */
  //UCG_C22(  0x000, 0x085, 0x000, 0x000),              /* Partial Image 2 RAM End Address */
  UCG_C22(  0x000, 0x090, 0x000, 0x010),              /* Panel Interface Control 1 */
  UCG_C22(  0x000, 0x092, 0x000, 0x000),              /* Panel Interface Control 2 */
            /* 0x006, 0x000 */
  UCG_C22(  0x000, 0x007, 0x001, 0x033),              /* Display Control 1: Operate, display ON, Partial image off */
  UCG_DLY_MS(10),               /* delay 10 ms */
  /* write test pattern */  
  //UCG_C22(  0x000, 0x020, 0x000, 0x000),              /* Horizontal GRAM Address Set */
  //UCG_C22(  0x000, 0x021, 0x000, 0x011),              /* Vertical GRAM Address Set */
  UCG_C20(  0x000, 0x022),               /* Write Data to GRAM */
	
  UCG_CS(1),					/* disable chip */
  UCG_END(),					/* end of sequence */
};

ucg_int_t ucg_dev_ssd1289_18x240x320_itdb02(ucg_t *ucg, ucg_int_t msg, void *data)
{
  switch(msg)
  {
    case UCG_MSG_DEV_POWER_UP:
      /* 1. Call to the controller procedures to setup the com interface */
      if ( ucg_dev_ic_ssd1289_18(ucg, msg, data) == 0 )
	return 0;

      /* 2. Send specific init sequence for this display module */
      ucg_com_SendCmdSeq(ucg, ucg_tft_240x320_ssd1289_init_seq);
      return 1;
      
    case UCG_MSG_DEV_POWER_DOWN:
      /* let do power down by the conroller procedures */
      return ucg_dev_ic_ssd1289_18(ucg, msg, data);  
    
    case UCG_MSG_GET_DIMENSION:
      ((ucg_wh_t *)data)->w = 240;
      ((ucg_wh_t *)data)->h = 320;
      return 1;
  }
  
  /* all other messages are handled by the controller procedures */
  return ucg_dev_ic_ssd1289_18(ucg, msg, data);  
}

#endif