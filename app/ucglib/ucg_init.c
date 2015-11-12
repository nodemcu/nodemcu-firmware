/*

  ucg_init.c
  
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
#include <string.h> 	/* memset */

#ifdef __AVR__
uint8_t global_SREG_backup;		// used by the atomic macros
#endif


void ucg_init_struct(ucg_t *ucg)
{
  //memset(ucg, 0, sizeof(ucg_t));
  ucg->is_power_up = 0;
  ucg->rotate_chain_device_cb = 0;
  ucg->arg.scale = 1;
  //ucg->display_offset.x = 0;
  //ucg->display_offset.y = 0;
  ucg->font = 0;
  //ucg->font_mode = UCG_FONT_MODE_NONE;   Old font procedures
  ucg->font_decode.is_transparent = 1;  // new font procedures
  
  ucg->com_initial_change_sent = 0;
  ucg->com_status = 0;
  ucg->com_cfg_cd = 0;
}


ucg_int_t ucg_Init(ucg_t *ucg, ucg_dev_fnptr device_cb, ucg_dev_fnptr ext_cb, ucg_com_fnptr com_cb)
{
  ucg_int_t r;
  ucg_init_struct(ucg);
  if ( ext_cb == (ucg_dev_fnptr)0 )
    ucg->ext_cb = ucg_ext_none;
 else 
    ucg->ext_cb = ext_cb;
  ucg->device_cb = device_cb;
  ucg->com_cb = com_cb;
  ucg_SetFontPosBaseline(ucg);
  r = ucg_PowerUp(ucg);
  ucg_GetDimension(ucg);
  return r;
}

