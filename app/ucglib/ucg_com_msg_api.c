/*

  ucg_com_msg_api.c
  
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

int16_t ucg_com_template_cb(ucg_t *ucg, int16_t msg, uint32_t arg, uint8_t *data)
{
  switch(msg)
  {
    case UCG_COM_MSG_POWER_UP:
      break;
    case UCG_COM_MSG_POWER_DOWN:
      break;
    case UCG_COM_MSG_DELAY:
      break;
    case UCG_COM_MSG_CHANGE_RESET_LINE:
      break;
    case UCG_COM_MSG_CHANGE_CS_LINE:
      break;
    case UCG_COM_MSG_CHANGE_CD_LINE:
      break;
    case UCG_COM_MSG_SEND_BYTE:
      break;
    case UCG_COM_MSG_REPEAT_1_BYTE:
      break;
    case UCG_COM_MSG_REPEAT_2_BYTES:
      break;
    case UCG_COM_MSG_REPEAT_3_BYTES:
      break;
    case UCG_COM_MSG_SEND_STR:
      break;
    case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
      break;
  }
  return 1;
}


void ucg_com_PowerDown(ucg_t *ucg)
{
  if ( (ucg->com_status & UCG_COM_STATUS_MASK_POWER) != 0 )
    ucg->com_cb(ucg, UCG_COM_MSG_POWER_DOWN, 0, NULL);
  ucg->com_status &= ~UCG_COM_STATUS_MASK_POWER;
}

/*
  clk_speed in nano-seconds, range: 0..4095
*/
int16_t ucg_com_PowerUp(ucg_t *ucg, uint16_t serial_clk_speed, uint16_t parallel_clk_speed)
{
  int16_t r;
  ucg_com_info_t com_info;
  com_info.serial_clk_speed = serial_clk_speed;
  com_info.parallel_clk_speed = parallel_clk_speed;
  
  ucg_com_PowerDown(ucg);  
  ucg->com_initial_change_sent = 0;
  r = ucg->com_cb(ucg, UCG_COM_MSG_POWER_UP, 0UL, (uint8_t *)&com_info);
  if ( r != 0 )
  {
    ucg->com_status |= UCG_COM_STATUS_MASK_POWER;
  }
  return r;
}

void ucg_com_SetLineStatus(ucg_t *ucg, uint8_t level, uint8_t mask, uint8_t msg)
{
  if ( level == 0 )
  {
    if ( (ucg->com_initial_change_sent & mask) == 0 || (ucg->com_status & mask) == mask )
    {
      ucg->com_cb(ucg, msg, level, NULL);
      ucg->com_status &= ~mask;
      ucg->com_initial_change_sent |= mask;
    }
  }
  else
  {
    if ( (ucg->com_initial_change_sent & mask) == 0 || (ucg->com_status & mask) == 0 )
    {
      ucg->com_cb(ucg, msg, level, NULL);
      ucg->com_status |= mask;
      ucg->com_initial_change_sent |= mask;
    }
  }
}

void ucg_com_SetResetLineStatus(ucg_t *ucg, uint8_t level)
{
  ucg_com_SetLineStatus(ucg, level, UCG_COM_STATUS_MASK_RESET, UCG_COM_MSG_CHANGE_RESET_LINE);
}

void ucg_com_SetCSLineStatus(ucg_t *ucg, uint8_t level)
{
  ucg_com_SetLineStatus(ucg, level, UCG_COM_STATUS_MASK_CS, UCG_COM_MSG_CHANGE_CS_LINE);
}

void ucg_com_SetCDLineStatus(ucg_t *ucg, uint8_t level)
{
  ucg_com_SetLineStatus(ucg, level, UCG_COM_STATUS_MASK_CD, UCG_COM_MSG_CHANGE_CD_LINE);
}

/* delay in microseconds */
void ucg_com_DelayMicroseconds(ucg_t *ucg, uint16_t delay)
{
  ucg->com_cb(ucg, UCG_COM_MSG_DELAY, delay, NULL);
}

/* delay in milliseconds */
void ucg_com_DelayMilliseconds(ucg_t *ucg, uint16_t delay)
{
  while( delay > 0 )
  {
    ucg_com_DelayMicroseconds(ucg, 1000);
    delay--;
  }
}


#ifndef ucg_com_SendByte
void ucg_com_SendByte(ucg_t *ucg, uint8_t byte)
{
  ucg->com_cb(ucg, UCG_COM_MSG_SEND_BYTE, byte, NULL);
}
#endif

void ucg_com_SendRepeatByte(ucg_t *ucg, uint16_t cnt, uint8_t byte)
{
  ucg->com_cb(ucg, UCG_COM_MSG_REPEAT_1_BYTE, cnt, &byte);
}

void ucg_com_SendRepeat2Bytes(ucg_t *ucg, uint16_t cnt, uint8_t *byte_ptr)
{
  ucg->com_cb(ucg, UCG_COM_MSG_REPEAT_2_BYTES, cnt, byte_ptr);
}


#ifndef ucg_com_SendRepeat3Bytes
void ucg_com_SendRepeat3Bytes(ucg_t *ucg, uint16_t cnt, uint8_t *byte_ptr)
{
  ucg->com_cb(ucg, UCG_COM_MSG_REPEAT_3_BYTES, cnt, byte_ptr);
}
#endif

void ucg_com_SendString(ucg_t *ucg, uint16_t cnt, const uint8_t *byte_ptr)
{
  ucg->com_cb(ucg, UCG_COM_MSG_SEND_STR, cnt, (uint8_t *)byte_ptr);
}

void ucg_com_SendStringP(ucg_t *ucg, uint16_t cnt, const ucg_pgm_uint8_t *byte_ptr)
{
  uint8_t b;
  while( cnt > 0 )
  {
    b = ucg_pgm_read(byte_ptr);
    //b = *byte_ptr;
    ucg->com_cb(ucg, UCG_COM_MSG_SEND_BYTE, b, NULL);
    byte_ptr++;
    cnt--;
  }
}


void ucg_com_SendCmdDataSequence(ucg_t *ucg, uint16_t cnt, const uint8_t *byte_ptr, uint8_t cd_line_status_at_end)
{
  ucg->com_cb(ucg, UCG_COM_MSG_SEND_CD_DATA_SEQUENCE, cnt, (uint8_t *)byte_ptr);
  ucg_com_SetCDLineStatus(ucg, cd_line_status_at_end);	// ensure that the status is set correctly for the CD line */
}

/*

Command-Sequence Language

CMD10		1x CMD Byte, 0x Argument Data Bytes
CMD20		2x CMD Byte, 0x Argument Data Bytes
CMD11		1x CMD Byte, 1x Argument Data Bytes
CMD21		2x CMD Byte, 1x Argument Data Bytes
CMD12		1x CMD Byte, 2x Argument Data Bytes
CMD22		2x CMD Byte, 2x Argument Data Bytes
CMD13		1x CMD Byte, 3x Argument Data Bytes
CMD23		2x CMD Byte, 3x Argument Data Bytes
CMD14		1x CMD Byte, 4x Argument Data Bytes
CMD24		2x CMD Byte, 4x Argument Data Bytes

DATA1		1x Data Bytes
DATA2		2x Data Bytes
DATA3		3x Data Bytes
DATA4		4x Data Bytes
DATA5		5x Data Bytes
DATA6		6x Data Bytes

RST_LOW		Output 0 on reset line
RST_HIGH		Output 1 on reset line

CS_LOW		Output 0 on chip select line
CS_HIGH		Output 1 on chip select line

DLY_MS		Delay for specified amount of milliseconds (0..2000)
DLY_US		Delay for specified amount of microconds (0..2000)

Configuration

CFG_CD(c,a)	Configure CMD/DATA line: "c" logic level CMD, "a" logic level CMD Args


0000xxxx			End Marker
0001xxxx			1x CMD Byte, 0..15 Argument Data Bytes
0010xxxx			2x CMD Byte, 0..15 Argument Data Bytes
0011xxxx			3x CMD Byte, 0..15 Argument Data Bytes
0100xxxx
0101xxxx
0110xxxx			Arg Bytes 0..15
0111xxxx			Data Bytes 0...15
1000xxxx	xxxxxxxx	DLY MS
1001xxxx	xxxxxxxx	DLY US
1010sssss aaaaaaaa oooooooo		(var0>>s)&a|o, send as argument
1011sssss aaaaaaaa oooooooo		(var1>>s)&a|o, send as argument
1100xxxx
1101xxxx
1110xxxx
11110000		RST_LOW
11110001		RST_HIGH
1111001x
11110100		CS_LOW
11110101		CS_HIGH
1111011x
111110xx
111111ca		CFG_CD(c,a)

#define C10(c0)				0x010, (c0)
#define C20(c0,c1)				0x020, (c0),(c1)
#define C11(c0,a0)				0x011, (c0),(a0)
#define C21(c0,c1,a0)			0x021, (c0),(c1),(a0)
#define C12(c0,a0,a1)			0x012, (c0),(a0),(a1)
#define C22(c0,c1,a0,a1)		0x022, (c0),(c1),(a0),(a1)
#define C13(c0,a0,a1,a2)		0x013, (c0),(a0),(a1),(a2)
#define C23(c0,c1,a0,a1,a2)		0x023, (c0),(c1),(a0),(a1),(a2)
#define C14(c0,a0,a1,a2,a3)		0x013, (c0),(a0),(a1),(a2),(a3)
#define C24(c0,c1,a0,a1,a2,a3)	0x023, (c0),(c1),(a0),(a1),(a2),(a3)

#define UCG_DATA()			0x070
#define D1(d0)				0x071, (d0)
#define D2(d0,d1)				0x072, (d0),(d1)
#define D3(d0,d1,d3)			0x073, (d0),(d1),(d2)
#define D4(d0,d1,d3,d4)			0x074, (d0),(d1),(d2),(d3)
#define D5(d0,d1,d3,d4,d5)		0x075, (d0),(d1),(d2),(d3),(d5)
#define D6(d0,d1,d3,d4,d5,d6)	0x076, (d0),(d1),(d2),(d3),(d5),(d6)

#define DLY_MS(t)				0x080 | (((t)>>8)&15), (t)&255
#define DLY_US(t)				0x090 | (((t)>>8)&15), (t)&255

  s: Shift right
  a: And mask
  o: Or mask
#define UCG_VARX(s,a,o)		0x0a0 | ((s)&15), (a), (o)
#define UCG_VARY(s,a,o)		0x0b0 | ((s)&15), (a), (o)

#define RST(level)				0x0f0 | ((level)&1)
#define CS(level)				0x0f4 | ((level)&1)
#define CFG_CD(c,a)			0x0fc | (((c)&1)<<1) | ((a)&1)

#define END()					0x00

*/

static void ucg_com_SendCmdArg(ucg_t *ucg, const ucg_pgm_uint8_t *data, uint8_t cmd_cnt, uint8_t arg_cnt)
{
  ucg_com_SetCDLineStatus(ucg, (ucg->com_cfg_cd>>1)&1 );
  ucg_com_SendStringP(ucg, cmd_cnt, data);
  if ( arg_cnt > 0 )
  {
    data += cmd_cnt;
    ucg_com_SetCDLineStatus(ucg, (ucg->com_cfg_cd)&1 );
    ucg_com_SendStringP(ucg, arg_cnt, data);
  }
}


//void ucg_com_SendCmdSeq(ucg_t *ucg, const ucg_pgm_uint8_t *data)
void ucg_com_SendCmdSeq(ucg_t *ucg, const ucg_pgm_uint8_t *data)
{
  uint8_t b;
  uint8_t bb;
  uint8_t hi;
  uint8_t lo;

  for(;;)
  {
    b = ucg_pgm_read(data);
    //b = *data;
    hi = (b) >> 4;
    lo = (b) & 0x0f;
    switch( hi )
    {
      case 0:
	return;		/* end marker */
      case 1:
      case 2:
      case 3:
	ucg_com_SendCmdArg(ucg, data+1, hi, lo);
	data+=1+hi+lo;
	break;
      case 6:
	ucg_com_SetCDLineStatus(ucg, (ucg->com_cfg_cd)&1 );
	ucg_com_SendStringP(ucg, lo, data+1);
	data+=1+lo;      
	break;
      case 7:	/* note: 0x070 is used to set data line status */
	ucg_com_SetCDLineStatus(ucg, ((ucg->com_cfg_cd>>1)&1)^1 );
	if ( lo > 0 )
	  ucg_com_SendStringP(ucg, lo, data+1);
	data+=1+lo;      
	break;
      case 8:
	data++;
	b = ucg_pgm_read(data);
	//b = *data;
	ucg_com_DelayMilliseconds(ucg, (((uint16_t)lo)<<8) + b );
	data++;
	break;
      case 9:
	data++;
	b = ucg_pgm_read(data);
	//b = *data;
	ucg_com_DelayMicroseconds(ucg, (((uint16_t)lo)<<8) + b );
	data++;
	break;
      case 10:
	data++;
	b = ucg_pgm_read(data);
	data++;
	bb = ucg_pgm_read(data);
	data++;
	//b = data[0];
	//bb = data[1];
	ucg_com_SetCDLineStatus(ucg, (ucg->com_cfg_cd)&1 );
	ucg_com_SendByte(ucg, (((uint8_t)(((ucg->arg.pixel.pos.x)>>lo)))&b)|bb );
	//data+=2;
	break;
      case 11:
	data++;
	b = ucg_pgm_read(data);
	data++;
	bb = ucg_pgm_read(data);
	data++;
	//b = data[0];
	//bb = data[1];
	ucg_com_SetCDLineStatus(ucg, (ucg->com_cfg_cd)&1 );
	ucg_com_SendByte(ucg, (((uint8_t)(((ucg->arg.pixel.pos.y)>>lo)))&b)|bb );
	//data+=2;
	break;
      case 15:
	hi = lo >> 2;
	lo &= 3;
	switch(hi)
	{
	  case 0:
	    ucg_com_SetResetLineStatus(ucg, lo&1);
	    break;
	  case 1:
	    ucg_com_SetCSLineStatus(ucg, lo&1);
	    break;
	  case 3:
	    ucg->com_cfg_cd = lo;
	    break;
	}
	data++;
	break;
      default:
	return;
    }  
  }
}

