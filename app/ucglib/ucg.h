/*

  ucg.h

  ucglib = universal color graphics library
  ucglib = micro controller graphics library
  
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
  

  SW layers
  
  High Level Procedures
    [ hline, init message interface ]
  display callback procedure
    [Calls to]
  device dev cb 		
    [calls to COM API]
  com callback


  font data:
  offset	bytes	description
  0		1		glyph_cnt		number of glyphs
  1		1		bbx_mode	0: proportional, 1: common height, 2: monospace, 3: multiple of 8
  2		1		bits_per_0	glyph rle parameter
  3		1		bits_per_1	glyph rle parameter

  4		1		bits_per_char_width		glyph rle parameter
  5		1		bits_per_char_height	glyph rle parameter
  6		1		bits_per_char_x		glyph rle parameter
  7		1		bits_per_char_y		glyph rle parameter
  8		1		bits_per_delta_x		glyph rle parameter

  9		1		max_char_width
  10		1		max_char_height
  11		1		x offset
  12		1		y offset (descent)
  
  13		1		ascent (capital A)
  14		1		descent (lower g)
  15		1		ascent '('
  16		1		descent ')'
  
  17		1		start pos 'A' high byte
  18		1		start pos 'A' low byte

  19		1		start pos 'a' high byte
  20		1		start pos 'a' low byte


*/

#ifndef _UCG_H
#define _UCG_H

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C"
{
#endif

#if defined(ARDUINO)
#ifndef USE_PIN_LIST
#define USE_PIN_LIST
#endif
#endif

#ifdef __GNUC__
#  define UCG_NOINLINE __attribute__((noinline))
#  define UCG_SECTION(name) __attribute__ ((section (name)))
#  if defined(__MSPGCC__)
/* mspgcc does not have .progmem sections. Use -fdata-sections. */
#    define UCG_FONT_SECTION(name)
#  elif defined(__AVR__)
#    define UCG_FONT_SECTION(name) UCG_SECTION(".progmem." name)
#  else
#    define UCG_FONT_SECTION(name)
#  endif
#else
#  define UCG_NOINLINE
#  define UCG_SECTION(name)
#  define UCG_FONT_SECTION(name)
#endif

#if defined(__AVR__)
#include <avr/pgmspace.h>
/* UCG_PROGMEM is used by the XBM example */
#define UCG_PROGMEM UCG_SECTION(".progmem.data")
typedef uint8_t PROGMEM ucg_pgm_uint8_t;
typedef uint8_t ucg_fntpgm_uint8_t;
#define ucg_pgm_read(adr) pgm_read_byte_near(adr)
#define UCG_PSTR(s) ((ucg_pgm_uint8_t *)PSTR(s))
#else
#define UCG_PROGMEM
#define PROGMEM
typedef uint8_t ucg_pgm_uint8_t;
typedef uint8_t ucg_fntpgm_uint8_t;
#define ucg_pgm_read(adr) (*(const ucg_pgm_uint8_t *)(adr)) 
#define UCG_PSTR(s) ((ucg_pgm_uint8_t *)(s))
#endif

/*================================================*/
/* several type and forward definitions */

typedef int16_t ucg_int_t;
typedef struct _ucg_t ucg_t;
typedef struct _ucg_xy_t ucg_xy_t;
typedef struct _ucg_wh_t ucg_wh_t;
typedef struct _ucg_box_t ucg_box_t;
typedef struct _ucg_color_t ucg_color_t;
typedef struct _ucg_ccs_t ucg_ccs_t;
typedef struct _ucg_pixel_t ucg_pixel_t;
typedef struct _ucg_arg_t ucg_arg_t;
typedef struct _ucg_com_info_t ucg_com_info_t;

typedef ucg_int_t (*ucg_dev_fnptr)(ucg_t *ucg, ucg_int_t msg, void *data); 
typedef int16_t (*ucg_com_fnptr)(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data); 
typedef ucg_int_t (*ucg_font_calc_vref_fnptr)(ucg_t *ucg);
//typedef ucg_int_t (*ucg_font_mode_fnptr)(ucg_t *ucg, ucg_int_t x, ucg_int_t y, uint8_t dir, uint8_t encoding);


/*================================================*/
/* list of supported display modules */

ucg_int_t ucg_dev_ssd1351_18x128x128_ilsoft(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ssd1351_18x128x128_ft(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ili9325_18x240x320_itdb02(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ili9325_spi_18x240x320(ucg_t *ucg, ucg_int_t msg, void *data); /*  1 May 2014: Currently, this is not working */
ucg_int_t ucg_dev_ili9341_18x240x320(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ili9163_18x128x128(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_st7735_18x128x160(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_pcf8833_16x132x132(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ld50t6160_18x160x128_samsung(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ssd1331_18x96x64_univision(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_seps225_16x128x128_univision(ucg_t *ucg, ucg_int_t msg, void *data);



/*================================================*/
/* 
  list of extensions for the controllers 
  
  each module can have the "none" extension (ucg_ext_none) or the specific
  extensions, that matches the controller name and color depth.
  
  example: for the module ucg_dev_ssd1351_18x128x128_ilsoft
  valid extensions are:
    ucg_ext_none
    ucg_ext_ssd1351_18
*/

ucg_int_t ucg_ext_none(ucg_t *ucg, ucg_int_t msg, void *data);

ucg_int_t ucg_ext_ssd1351_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ili9325_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ili9325_spi_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ili9341_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ili9163_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_st7735_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_pcf8833_16(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ld50t6160_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_ssd1331_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_ext_seps225_16(ucg_t *ucg, ucg_int_t msg, void *data);


/*================================================*/
/* list of supported display controllers */

ucg_int_t ucg_dev_ic_ssd1351_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ili9325_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ili9325_spi_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ili9341_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ili9163_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_st7735_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_pcf8833_16(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ld50t6160_18(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_dev_ic_ssd1331_18(ucg_t *ucg, ucg_int_t msg, void *data);   /* actually this display only has 65k colors */
ucg_int_t ucg_dev_ic_seps225_16(ucg_t *ucg, ucg_int_t msg, void *data);   /* this display could do 262k colors, but only 65k are supported via SPI */


/*================================================*/
/* struct declarations */

struct _ucg_xy_t
{
  ucg_int_t x;
  ucg_int_t y;
};

struct _ucg_wh_t
{
  ucg_int_t w;  
  ucg_int_t h;
};

struct _ucg_box_t
{
  ucg_xy_t ul;
  ucg_wh_t size;
};

struct _ucg_color_t
{
  uint8_t color[3];		/* 0: Red, 1: Green, 2: Blue */
};

struct _ucg_ccs_t
{
  uint8_t current;	/* contains the current color component */
  uint8_t start;
  ucg_int_t dir;		/* 1 if start < end or -1 if start > end */
  ucg_int_t num;
  ucg_int_t quot;
  
  ucg_int_t den;
  ucg_int_t rem;  
  ucg_int_t frac;
};

struct _ucg_pixel_t
{
  ucg_xy_t pos;
  ucg_color_t rgb;  
};

struct _ucg_arg_t
{
  ucg_pixel_t pixel;
  ucg_int_t len;
  ucg_int_t dir;
  ucg_int_t offset;			/* calculated offset from the inital point to the start of the clip window (ucg_clip_l90fx) */
  ucg_int_t scale;			/* upscale factor, used by UCG_MSG_DRAW_L90BF */
  const unsigned char *bitmap;
  ucg_int_t pixel_skip;		/* within the "bitmap" skip the specified number of pixel with the bit. pixel_skip is always <= 7 */
  ucg_color_t rgb[4];			/* start and end color for L90SE , two more colors for the gradient box */
  ucg_ccs_t ccs_line[3];		/* color component sliders used by L90SE */
};

#define UCG_FONT_HEIGHT_MODE_TEXT 0
#define UCG_FONT_HEIGHT_MODE_XTEXT 1
#define UCG_FONT_HEIGHT_MODE_ALL 2

struct _ucg_com_info_t
{
  uint16_t serial_clk_speed;
  uint16_t parallel_clk_speed;
};


struct _ucg_font_info_t
{
  /* offset 0 */
  uint8_t glyph_cnt;
  uint8_t bbx_mode;
  uint8_t bits_per_0;
  uint8_t bits_per_1;
  
  /* offset 4 */
  uint8_t bits_per_char_width;
  uint8_t bits_per_char_height;		
  uint8_t bits_per_char_x;
  uint8_t bits_per_char_y;
  uint8_t bits_per_delta_x;
  
  /* offset 9 */
  int8_t max_char_width;
  int8_t max_char_height; /* overall height, NOT ascent. Instead ascent = max_char_height + y_offset */
  int8_t x_offset;
  int8_t y_offset;
  
  /* offset 13 */
  int8_t  ascent_A;
  int8_t  descent_g;
  int8_t  ascent_para;
  int8_t  descent_para;
  
  /* offset 17 */
  uint16_t start_pos_upper_A;
  uint16_t start_pos_lower_a;  
};
typedef struct _ucg_font_info_t ucg_font_info_t;

struct _ucg_font_decode_t
{
  const uint8_t *decode_ptr;			/* pointer to the compressed data */
  
  ucg_int_t target_x;
  ucg_int_t target_y;
  
  int8_t x;						/* local coordinates, (0,0) is upper left */
  int8_t y;
  int8_t glyph_width;	
  int8_t glyph_height;

  uint8_t decode_bit_pos;			/* bitpos inside a byte of the compressed data */
  uint8_t is_transparent;
  uint8_t dir;				/* direction */
};
typedef struct _ucg_font_decode_t ucg_font_decode_t;



#ifdef USE_PIN_LIST
#define UCG_PIN_RST 0
#define UCG_PIN_CD 1
#define UCG_PIN_CS 2
#define UCG_PIN_SCL 3
#define UCG_PIN_WR 3
#define UCG_PIN_SDA 4

#define UCG_PIN_D0 5
#define UCG_PIN_D1 6
#define UCG_PIN_D2 7
#define UCG_PIN_D3 8
#define UCG_PIN_D4 9
#define UCG_PIN_D5 10
#define UCG_PIN_D6 11
#define UCG_PIN_D7 12

#define UCG_PIN_COUNT 13

#define UCG_PIN_VAL_NONE 255
#endif

struct _ucg_t
{
  unsigned is_power_up:1;
  /* the dimension of the display */
  ucg_wh_t dimension;
  /* display callback procedure to handle display specific code */
  //ucg_dev_fnptr display_cb;
  /* controller and device specific code, high level procedure will call this */
  ucg_dev_fnptr device_cb;
  /* name of the extension cb. will be called by device_cb if required */
  ucg_dev_fnptr ext_cb;
  /* if rotation is applied, than this cb is called after rotation */
  ucg_dev_fnptr rotate_chain_device_cb;
  ucg_wh_t rotate_dimension;

  /* if rotation is applied, than this cb is called by the scale device */
  ucg_dev_fnptr scale_chain_device_cb;
  
  /* communication interface */
  ucg_com_fnptr com_cb;
  
  /* offset, that is additionally added to UCG_VARX/UCG_VARY */
  /* seems to be required for the Nokia display */
  // ucg_xy_t display_offset;
  
  /* data which is passed to the cb procedures */
  /* can be modified by the cb procedures (rotation, clipping, etc) */
  ucg_arg_t arg;
  /* current window to which all drawing is clipped */
  /* should be modified via UCG_MSG_SET_CLIP_BOX by a device callback. */
  /* by default this is done by ucg_dev_default_cb */
  ucg_box_t clip_box;
  

  /* information about the current font */
  const unsigned char *font;             /* current font for all text procedures */
  ucg_font_calc_vref_fnptr font_calc_vref;
  //ucg_font_mode_fnptr font_mode;		/* OBSOLETE?? UCG_FONT_MODE_TRANSPARENT, UCG_FONT_MODE_SOLID, UCG_FONT_MODE_NONE */

  ucg_font_decode_t font_decode;		/* new font decode structure */
  ucg_font_info_t font_info;			/* new font info structure */

  int8_t glyph_dx;			/* OBSOLETE */
  int8_t glyph_x;			/* OBSOLETE */
  int8_t glyph_y;			/* OBSOLETE */
  uint8_t glyph_width;		/* OBSOLETE */
  uint8_t glyph_height;		/* OBSOLETE */
  
  uint8_t font_height_mode;
  int8_t font_ref_ascent;
  int8_t font_ref_descent;

  /* only for Arduino/C++ Interface */
#ifdef USE_PIN_LIST
  uint8_t pin_list[UCG_PIN_COUNT];

#ifdef __AVR__
  volatile uint8_t *data_port[UCG_PIN_COUNT];
  uint8_t data_mask[UCG_PIN_COUNT];
#endif

#endif

  /* 
    Small amount of RAM for the com interface (com_cb).
    Might be unused on unix systems, where the com sub system is 
    not required, but should be usefull for all uC projects.
  */
  uint8_t com_initial_change_sent;	/* Bit 0: CD/A0 Line Status, Bit 1: CS Line Status, Bit 2: Reset Line Status */
  uint8_t com_status;		/* Bit 0: CD/A0 Line Status, Bit 1: CS Line Status, Bit 2: Reset Line Status,  Bit 3: 1 for power up */
  uint8_t com_cfg_cd;		/* Bit 0: Argument Level, Bit 1: Command Level */
  
  
};

#define ucg_GetWidth(ucg) ((ucg)->dimension.w)
#define ucg_GetHeight(ucg) ((ucg)->dimension.h)

#define UCG_MSG_DEV_POWER_UP	10
#define UCG_MSG_DEV_POWER_DOWN 11
#define UCG_MSG_SET_CLIP_BOX 12
#define UCG_MSG_GET_DIMENSION 15

/* draw pixel with color from idx 0 */
#define UCG_MSG_DRAW_PIXEL 20
#define UCG_MSG_DRAW_L90FX 21
/* draw  bit pattern, transparent and draw color (idx 0) color */
//#define UCG_MSG_DRAW_L90TC 22		/* can be commented, used by ucg_DrawTransparentBitmapLine */
#define UCG_MSG_DRAW_L90SE 23		/* this part of the extension */
//#define UCG_MSG_DRAW_L90RL 24	/* not yet implemented */
/* draw  bit pattern with foreground (idx 1) and background (idx 0) color */
//#define UCG_MSG_DRAW_L90BF 25	 /* can be commented, used by ucg_DrawBitmapLine */


#define UCG_COM_STATUS_MASK_POWER 8
#define UCG_COM_STATUS_MASK_RESET 4
#define UCG_COM_STATUS_MASK_CS 2
#define UCG_COM_STATUS_MASK_CD 1

/*
  arg:	0
  data:	ucg_com_info_t *
  return:	0 for error
  note: 
    - power up is the first command, which is sent
*/
#define UCG_COM_MSG_POWER_UP 10

/*
  note: power down my be followed only by power up command
*/
#define UCG_COM_MSG_POWER_DOWN 11

/*
  arg:	delay in microseconds  (0..4095) 
*/
#define UCG_COM_MSG_DELAY 12

/*
  ucg->com_status	contains previous status of reset line
  arg:			new logic level for reset line
*/
#define UCG_COM_MSG_CHANGE_RESET_LINE 13
/*
  ucg->com_status	contains previous status of cs line
  arg:	new logic level for cs line
*/
#define UCG_COM_MSG_CHANGE_CS_LINE 14

/*
  ucg->com_status	contains previous status of cd line
  arg:	new logic level for cd line
*/
#define UCG_COM_MSG_CHANGE_CD_LINE 15

/*
  ucg->com_status	current status of Reset, CS and CD line (ucg->com_status)
  arg:			byte for display
*/
#define UCG_COM_MSG_SEND_BYTE 16

/*
  ucg->com_status	current status of Reset, CS and CD line (ucg->com_status)
  arg:			how often to repeat the 2/3 byte sequence 	
  data:			pointer to two or three bytes
*/
#define UCG_COM_MSG_REPEAT_1_BYTE 17
#define UCG_COM_MSG_REPEAT_2_BYTES 18
#define UCG_COM_MSG_REPEAT_3_BYTES 19

/*
  ucg->com_status	current status of Reset, CS and CD line (ucg->com_status)
  arg:			length of string 	
  data:			string
*/
#define UCG_COM_MSG_SEND_STR 20

/*
  ucg->com_status	current status of Reset, CS and CD line (ucg->com_status)
  arg:			number of cd_info and data pairs (half value of total byte cnt) 	
  data:			uint8_t with CD and data information
	cd_info data cd_info data cd_info data cd_info data ... cd_info data cd_info data
	cd_info is the level, which is directly applied to the CD line. This means,
	information applied to UCG_CFG_CD is not relevant.
*/
#define UCG_COM_MSG_SEND_CD_DATA_SEQUENCE 21



/*================================================*/
/* interrupt safe code */
#define UCG_INTERRUPT_SAFE
#if defined(UCG_INTERRUPT_SAFE)
#  if defined(__AVR__)
extern uint8_t global_SREG_backup;	/* ucg_init.c */
#    define UCG_ATOMIC_START()		do { global_SREG_backup = SREG; cli(); } while(0)
#    define UCG_ATOMIC_END()			SREG = global_SREG_backup
#    define UCG_ATOMIC_OR(ptr, val) 	do { uint8_t tmpSREG = SREG; cli(); (*(ptr) |= (val)); SREG = tmpSREG; } while(0)
#    define UCG_ATOMIC_AND(ptr, val) 	do { uint8_t tmpSREG = SREG; cli(); (*(ptr) &= (val)); SREG = tmpSREG; } while(0)
#  else
#    define UCG_ATOMIC_OR(ptr, val) (*(ptr) |= (val))
#    define UCG_ATOMIC_AND(ptr, val) (*(ptr) &= (val))
#    define UCG_ATOMIC_START()
#    define UCG_ATOMIC_END()
#  endif /* __AVR__ */
#else
#  define UCG_ATOMIC_OR(ptr, val) (*(ptr) |= (val))
#  define UCG_ATOMIC_AND(ptr, val) (*(ptr) &= (val))
#  define UCG_ATOMIC_START()
#  define UCG_ATOMIC_END()
#endif /* UCG_INTERRUPT_SAFE */

/*================================================*/
/* ucg_dev_msg_api.c */
void ucg_PowerDown(ucg_t *ucg);
ucg_int_t ucg_PowerUp(ucg_t *ucg);
void ucg_SetClipBox(ucg_t *ucg, ucg_box_t *clip_box);
void ucg_SetClipRange(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h);
void ucg_SetMaxClipRange(ucg_t *ucg);
void ucg_GetDimension(ucg_t *ucg);
void ucg_DrawPixelWithArg(ucg_t *ucg);
void ucg_DrawL90FXWithArg(ucg_t *ucg);
void ucg_DrawL90TCWithArg(ucg_t *ucg);
void ucg_DrawL90BFWithArg(ucg_t *ucg);
void ucg_DrawL90SEWithArg(ucg_t *ucg);
/* void ucg_DrawL90RLWithArg(ucg_t *ucg); */

/*================================================*/
/* ucg_init.c */
ucg_int_t ucg_Init(ucg_t *ucg, ucg_dev_fnptr device_cb, ucg_dev_fnptr ext_cb, ucg_com_fnptr com_cb);


/*================================================*/
/* ucg_dev_sdl.c */
ucg_int_t ucg_sdl_dev_cb(ucg_t *ucg, ucg_int_t msg, void *data);

/*================================================*/
/* ucg_pixel.c */
void ucg_SetColor(ucg_t *ucg, uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void ucg_DrawPixel(ucg_t *ucg, ucg_int_t x, ucg_int_t y);

/*================================================*/
/* ucg_line.c */
void ucg_Draw90Line(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len, ucg_int_t dir, ucg_int_t col_idx);
void ucg_DrawHLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len);
void ucg_DrawVLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len);
void ucg_DrawHRLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len);
void ucg_DrawLine(ucg_t *ucg, ucg_int_t x1, ucg_int_t y1, ucg_int_t x2, ucg_int_t y2);
/* the following procedure is only available with the extended callback */
void ucg_DrawGradientLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t len, ucg_int_t dir);


/*================================================*/
/* ucg_box.c */
void ucg_DrawBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h);
void ucg_ClearScreen(ucg_t *ucg);
void ucg_DrawRBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h, ucg_int_t r);
void ucg_DrawGradientBox(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h);
void ucg_DrawFrame(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h);
void ucg_DrawRFrame(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t w, ucg_int_t h, ucg_int_t r);


/*================================================*/
/* ucg_circle.c */
#define UCG_DRAW_UPPER_RIGHT 0x01
#define UCG_DRAW_UPPER_LEFT  0x02
#define UCG_DRAW_LOWER_LEFT 0x04
#define UCG_DRAW_LOWER_RIGHT  0x08
#define UCG_DRAW_ALL (UCG_DRAW_UPPER_RIGHT|UCG_DRAW_UPPER_LEFT|UCG_DRAW_LOWER_RIGHT|UCG_DRAW_LOWER_LEFT)
void ucg_DrawDisc(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option);
void ucg_DrawCircle(ucg_t *ucg, ucg_int_t x0, ucg_int_t y0, ucg_int_t rad, uint8_t option);

/*================================================*/
/* ucg_bitmap.c */
void ucg_DrawTransparentBitmapLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t dir, ucg_int_t len, const unsigned char *bitmap);
void ucg_DrawBitmapLine(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t dir, ucg_int_t len, const unsigned char *bitmap);
/* void ucg_DrawRLBitmap(ucg_t *ucg, ucg_int_t x, ucg_int_t y, ucg_int_t dir, const unsigned char *rl_bitmap); */


/*================================================*/
/* ucg_rotate.c */
void ucg_UndoRotate(ucg_t *ucg);
void ucg_SetRotate90(ucg_t *ucg);
void ucg_SetRotate180(ucg_t *ucg);
void ucg_SetRotate270(ucg_t *ucg);

/*================================================*/
/* ucg_scale.c */
void ucg_UndoScale(ucg_t *ucg);
void ucg_SetScale2x2(ucg_t *ucg);


/*================================================*/
/* ucg_polygon.c */

typedef int16_t pg_word_t;

#define PG_NOINLINE UCG_NOINLINE

struct pg_point_struct
{
  pg_word_t x;
  pg_word_t y;
};

typedef struct _pg_struct pg_struct;	/* forward declaration */

struct pg_edge_struct
{
  pg_word_t x_direction;	/* 1, if x2 is greater than x1, -1 otherwise */
  pg_word_t height;
  pg_word_t current_x_offset;
  pg_word_t error_offset;
  
  /* --- line loop --- */
  pg_word_t current_y;
  pg_word_t max_y;
  pg_word_t current_x;
  pg_word_t error;

  /* --- outer loop --- */
  uint8_t (*next_idx_fn)(pg_struct *pg, uint8_t i);
  uint8_t curr_idx;
};

/* maximum number of points in the polygon */
/* can be redefined, but highest possible value is 254 */
#define PG_MAX_POINTS 4

/* index numbers for the pge structures below */
#define PG_LEFT 0
#define PG_RIGHT 1


struct _pg_struct
{
  struct pg_point_struct list[PG_MAX_POINTS];
  uint8_t cnt;
  uint8_t is_min_y_not_flat;
  pg_word_t total_scan_line_cnt;
  struct pg_edge_struct pge[2];	/* left and right line draw structures */
};

void pg_ClearPolygonXY(pg_struct *pg);
void pg_AddPolygonXY(pg_struct *pg, ucg_t *ucg, int16_t x, int16_t y);
void pg_DrawPolygon(pg_struct *pg, ucg_t *ucg);
void ucg_ClearPolygonXY(void);
void ucg_AddPolygonXY(ucg_t *ucg, int16_t x, int16_t y);
void ucg_DrawPolygon(ucg_t *ucg);
void ucg_DrawTriangle(ucg_t *ucg, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
/* the polygon procedure only works for convex tetragons (http://en.wikipedia.org/wiki/Convex_polygon) */
void ucg_DrawTetragon(ucg_t *ucg, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3);


/*================================================*/
/* ucg_font.c */

//ucg_int_t ucg_draw_transparent_glyph(ucg_t *ucg, ucg_int_t x, ucg_int_t y, uint8_t dir, uint8_t encoding);
//ucg_int_t ucg_draw_solid_glyph(ucg_t *ucg, ucg_int_t x, ucg_int_t y, uint8_t dir, uint8_t encoding);

// old font procedures
//#define UCG_FONT_MODE_TRANSPARENT ucg_draw_transparent_glyph
//#define UCG_FONT_MODE_SOLID ucg_draw_solid_glyph
//#define UCG_FONT_MODE_NONE ((ucg_font_mode_fnptr)0)

// new font procedures
#define UCG_FONT_MODE_TRANSPARENT 1
#define UCG_FONT_MODE_SOLID 0
#define UCG_FONT_MODE_NONE 1


/* Information on a specific given font */
uint8_t ucg_font_GetFontStartEncoding(const void *font);
uint8_t ucg_font_GetFontEndEncoding(const void *font);

uint8_t ucg_font_GetCapitalAHeight(const void *font);

int8_t ucg_font_GetFontAscent(const void *font);
int8_t ucg_font_GetFontDescent(const void *font);

int8_t ucg_font_GetFontXAscent(const void *font);
int8_t ucg_font_GetFontXDescent(const void *font);

size_t ucg_font_GetSize(const void *font);

/* Information on the current font */

uint8_t ucg_GetFontBBXWidth(ucg_t *ucg);
uint8_t ucg_GetFontBBXHeight(ucg_t *ucg);
uint8_t ucg_GetFontCapitalAHeight(ucg_t *ucg) UCG_NOINLINE; 
uint8_t ucg_IsGlyph(ucg_t *ucg, uint8_t requested_encoding);
int8_t ucg_GetGlyphWidth(ucg_t *ucg, uint8_t requested_encoding);

#define ucg_GetFontAscent(ucg)	((ucg)->font_ref_ascent)
#define ucg_GetFontDescent(ucg)	((ucg)->font_ref_descent)

/* Drawing procedures */

ucg_int_t ucg_DrawGlyph(ucg_t *ucg, ucg_int_t x, ucg_int_t y, uint8_t dir, uint8_t encoding);
ucg_int_t ucg_DrawString(ucg_t *ucg, ucg_int_t x, ucg_int_t y, uint8_t dir, const char *str);

/* Mode selection/Font assignment */

void ucg_SetFontRefHeightText(ucg_t *ucg);
void ucg_SetFontRefHeightExtendedText(ucg_t *ucg);
void ucg_SetFontRefHeightAll(ucg_t *ucg);

void ucg_SetFontPosBaseline(ucg_t *ucg) UCG_NOINLINE;
void ucg_SetFontPosBottom(ucg_t *ucg);
void ucg_SetFontPosTop(ucg_t *ucg);
void ucg_SetFontPosCenter(ucg_t *ucg);

void ucg_SetFont(ucg_t *ucg, const ucg_fntpgm_uint8_t  *font);
//void ucg_SetFontMode(ucg_t *ucg, ucg_font_mode_fnptr font_mode);
void ucg_SetFontMode(ucg_t *ucg, uint8_t is_transparent);

ucg_int_t ucg_GetStrWidth(ucg_t *ucg, const char *s);


/*================================================*/
/* LOW LEVEL PROCEDRUES, ONLY CALLED BY DEV CB */

/*================================================*/
/* ucg_clip.c */
ucg_int_t ucg_clip_is_pixel_visible(ucg_t *ucg);
ucg_int_t ucg_clip_l90fx(ucg_t *ucg);
ucg_int_t ucg_clip_l90tc(ucg_t *ucg);
ucg_int_t ucg_clip_l90se(ucg_t *ucg);


/*================================================*/
/* ucg_ccs.c */
void ucg_ccs_init(ucg_ccs_t *ccs, uint8_t start, uint8_t end, ucg_int_t steps);
void ucg_ccs_step(ucg_ccs_t *ccs);
void ucg_ccs_seek(ucg_ccs_t *ccs, ucg_int_t pos);


/*================================================*/
/* ucg_dev_default_cb.c */
ucg_int_t ucg_dev_default_cb(ucg_t *ucg, ucg_int_t msg, void *data);
ucg_int_t ucg_handle_l90fx(ucg_t *ucg, ucg_dev_fnptr dev_cb);
ucg_int_t ucg_handle_l90tc(ucg_t *ucg, ucg_dev_fnptr dev_cb);
ucg_int_t ucg_handle_l90se(ucg_t *ucg, ucg_dev_fnptr dev_cb);
ucg_int_t ucg_handle_l90bf(ucg_t *ucg, ucg_dev_fnptr dev_cb);
void ucg_handle_l90rl(ucg_t *ucg, ucg_dev_fnptr dev_cb);


/*================================================*/
/* ucg_com_msg_api.c */

/* send command bytes and optional arguments */
#define UCG_C10(c0)				0x010, (c0)
#define UCG_C20(c0,c1)				0x020, (c0),(c1)
#define UCG_C11(c0,a0)				0x011, (c0),(a0)
#define UCG_C21(c0,c1,a0)			0x021, (c0),(c1),(a0)
#define UCG_C12(c0,a0,a1)			0x012, (c0),(a0),(a1)
#define UCG_C22(c0,c1,a0,a1)		0x022, (c0),(c1),(a0),(a1)
#define UCG_C13(c0,a0,a1,a2)		0x013, (c0),(a0),(a1),(a2)
#define UCG_C23(c0,c1,a0,a1,a2)		0x023, (c0),(c1),(a0),(a1),(a2)
#define UCG_C14(c0,a0,a1,a2,a3)		0x014, (c0),(a0),(a1),(a2),(a3)
#define UCG_C24(c0,c1,a0,a1,a2,a3)	0x024, (c0),(c1),(a0),(a1),(a2),(a3)
#define UCG_C15(c0,a0,a1,a2,a3,a4)	0x015, (c0),(a0),(a1),(a2),(a3),(a4)

/* send one or more argument bytes */
#define UCG_A1(d0)					0x061, (d0)
#define UCG_A2(d0,d1)					0x062, (d0),(d1)
#define UCG_A3(d0,d1,d2)				0x063, (d0),(d1),(d2)
#define UCG_A4(d0,d1,d2,d3)				0x064, (d0),(d1),(d2),(d3)
#define UCG_A5(d0,d1,d2,d3,d4)			0x065, (d0),(d1),(d2),(d3),(d4)
#define UCG_A6(d0,d1,d2,d3,d4,d5)		0x066, (d0),(d1),(d2),(d3),(d4),(d5)
#define UCG_A7(d0,d1,d2,d3,d4,d5,d6)		0x067, (d0),(d1),(d2),(d3),(d4),(d5),(d6)
#define UCG_A8(d0,d1,d2,d3,d4,d5,d6,d7)	0x068, (d0),(d1),(d2),(d3),(d4),(d5),(d6),(d7)

/* force data mode on CD line */
#define UCG_DATA()					0x070
/* send one or more data bytes */
#define UCG_D1(d0)				0x071, (d0)
#define UCG_D2(d0,d1)				0x072, (d0),(d1)
#define UCG_D3(d0,d1,d2)			0x073, (d0),(d1),(d2)
#define UCG_D4(d0,d1,d2,d3)			0x074, (d0),(d1),(d2),(d3)
#define UCG_D5(d0,d1,d2,d3,d4)		0x075, (d0),(d1),(d2),(d3),(d4)
#define UCG_D6(d0,d1,d2,d3,d4,d5)	0x076, (d0),(d1),(d2),(d3),(d4),(d5)

/* delay by specified value. t = [0..4095] */
#define UCG_DLY_MS(t)				0x080 | (((t)>>8)&15), (t)&255
#define UCG_DLY_US(t)				0x090 | (((t)>>8)&15), (t)&255

/* access procedures to ucg->arg.pixel.pos.x und ucg->arg.pixel.pos.y */
#define UCG_VARX(s,a,o)				0x0a0 | ((s)&15), (a), (o)
#define UCG_VARY(s,a,o)				0x0b0 | ((s)&15), (a), (o)

/* force specific level on RST und CS */
#define UCG_RST(level)				0x0f0 | ((level)&1)
#define UCG_CS(level)				0x0f4 | ((level)&1)

/* Configure CD line for command, arguments and data */
/* Configure CMD/DATA line: "c" logic level CMD, "a" logic level CMD Args */
#define UCG_CFG_CD(c,a)			0x0fc | (((c)&1)<<1) | ((a)&1)

/* Termination byte */
#define UCG_END()					0x00

/*
#define ucg_com_SendByte(ucg, byte) \
  (ucg)->com_cb((ucg), UCG_COM_MSG_SEND_BYTE, (byte), NULL)
*/

#define ucg_com_SendRepeat3Bytes(ucg, cnt, byte_ptr) \
  (ucg)->com_cb((ucg), UCG_COM_MSG_REPEAT_3_BYTES, (cnt), (byte_ptr))

void ucg_com_PowerDown(ucg_t *ucg);
int16_t ucg_com_PowerUp(ucg_t *ucg, uint16_t serial_clk_speed, uint16_t parallel_clk_speed);  /* values are nano seconds */
void ucg_com_SetLineStatus(ucg_t *ucg, uint8_t level, uint8_t mask, uint8_t msg) UCG_NOINLINE;
void ucg_com_SetResetLineStatus(ucg_t *ucg, uint8_t level);
void ucg_com_SetCSLineStatus(ucg_t *ucg, uint8_t level);
void ucg_com_SetCDLineStatus(ucg_t *ucg, uint8_t level);
void ucg_com_DelayMicroseconds(ucg_t *ucg, uint16_t delay) UCG_NOINLINE;
void ucg_com_DelayMilliseconds(ucg_t *ucg, uint16_t delay) UCG_NOINLINE;
#ifndef ucg_com_SendByte
void ucg_com_SendByte(ucg_t *ucg, uint8_t byte);
#endif
void ucg_com_SendRepeatByte(ucg_t *ucg, uint16_t cnt, uint8_t byte);
void ucg_com_SendRepeat2Bytes(ucg_t *ucg, uint16_t cnt, uint8_t *byte_ptr);
#ifndef ucg_com_SendRepeat3Bytes
void ucg_com_SendRepeat3Bytes(ucg_t *ucg, uint16_t cnt, uint8_t *byte_ptr);
#endif
void ucg_com_SendString(ucg_t *ucg, uint16_t cnt, const uint8_t *byte_ptr);
void ucg_com_SendCmdDataSequence(ucg_t *ucg, uint16_t cnt, const uint8_t *byte_ptr, uint8_t cd_line_status_at_end);
void ucg_com_SendCmdSeq(ucg_t *ucg, const ucg_pgm_uint8_t *data);


/*================================================*/
/* ucg_dev_tga.c */
int tga_init(uint16_t w, uint16_t h);
void tga_save(const char *name);

ucg_int_t ucg_dev_tga(ucg_t *ucg, ucg_int_t msg, void *data);




/*================================================*/

#ifdef OLD_FONTS

extern const ucg_fntpgm_uint8_t ucg_font_04b_03b[] UCG_FONT_SECTION("ucg_font_04b_03b");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03bn[] UCG_FONT_SECTION("ucg_font_04b_03bn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03br[] UCG_FONT_SECTION("ucg_font_04b_03br");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03[] UCG_FONT_SECTION("ucg_font_04b_03");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03n[] UCG_FONT_SECTION("ucg_font_04b_03n");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03r[] UCG_FONT_SECTION("ucg_font_04b_03r");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24[] UCG_FONT_SECTION("ucg_font_04b_24");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24n[] UCG_FONT_SECTION("ucg_font_04b_24n");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24r[] UCG_FONT_SECTION("ucg_font_04b_24r");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_67_75[] UCG_FONT_SECTION("ucg_font_10x20_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_75r[] UCG_FONT_SECTION("ucg_font_10x20_75r");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_78_79[] UCG_FONT_SECTION("ucg_font_10x20_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_10x20[] UCG_FONT_SECTION("ucg_font_10x20");
extern const ucg_fntpgm_uint8_t ucg_font_10x20r[] UCG_FONT_SECTION("ucg_font_10x20r");
extern const ucg_fntpgm_uint8_t ucg_font_4x6[] UCG_FONT_SECTION("ucg_font_4x6");
extern const ucg_fntpgm_uint8_t ucg_font_4x6r[] UCG_FONT_SECTION("ucg_font_4x6r");
extern const ucg_fntpgm_uint8_t ucg_font_5x7[] UCG_FONT_SECTION("ucg_font_5x7");
extern const ucg_fntpgm_uint8_t ucg_font_5x7r[] UCG_FONT_SECTION("ucg_font_5x7r");
extern const ucg_fntpgm_uint8_t ucg_font_5x8[] UCG_FONT_SECTION("ucg_font_5x8");
extern const ucg_fntpgm_uint8_t ucg_font_5x8r[] UCG_FONT_SECTION("ucg_font_5x8r");
extern const ucg_fntpgm_uint8_t ucg_font_6x10[] UCG_FONT_SECTION("ucg_font_6x10");
extern const ucg_fntpgm_uint8_t ucg_font_6x10r[] UCG_FONT_SECTION("ucg_font_6x10r");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_67_75[] UCG_FONT_SECTION("ucg_font_6x12_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_75r[] UCG_FONT_SECTION("ucg_font_6x12_75r");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_78_79[] UCG_FONT_SECTION("ucg_font_6x12_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_6x12[] UCG_FONT_SECTION("ucg_font_6x12");
extern const ucg_fntpgm_uint8_t ucg_font_6x12r[] UCG_FONT_SECTION("ucg_font_6x12r");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_67_75[] UCG_FONT_SECTION("ucg_font_6x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_75r[] UCG_FONT_SECTION("ucg_font_6x13_75r");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_78_79[] UCG_FONT_SECTION("ucg_font_6x13_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_6x13B[] UCG_FONT_SECTION("ucg_font_6x13B");
extern const ucg_fntpgm_uint8_t ucg_font_6x13Br[] UCG_FONT_SECTION("ucg_font_6x13Br");
extern const ucg_fntpgm_uint8_t ucg_font_6x13[] UCG_FONT_SECTION("ucg_font_6x13");
extern const ucg_fntpgm_uint8_t ucg_font_6x13O[] UCG_FONT_SECTION("ucg_font_6x13O");
extern const ucg_fntpgm_uint8_t ucg_font_6x13Or[] UCG_FONT_SECTION("ucg_font_6x13Or");
extern const ucg_fntpgm_uint8_t ucg_font_6x13r[] UCG_FONT_SECTION("ucg_font_6x13r");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_67_75[] UCG_FONT_SECTION("ucg_font_7x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_75r[] UCG_FONT_SECTION("ucg_font_7x13_75r");
extern const ucg_fntpgm_uint8_t ucg_font_7x13B[] UCG_FONT_SECTION("ucg_font_7x13B");
extern const ucg_fntpgm_uint8_t ucg_font_7x13Br[] UCG_FONT_SECTION("ucg_font_7x13Br");
extern const ucg_fntpgm_uint8_t ucg_font_7x13[] UCG_FONT_SECTION("ucg_font_7x13");
extern const ucg_fntpgm_uint8_t ucg_font_7x13O[] UCG_FONT_SECTION("ucg_font_7x13O");
extern const ucg_fntpgm_uint8_t ucg_font_7x13Or[] UCG_FONT_SECTION("ucg_font_7x13Or");
extern const ucg_fntpgm_uint8_t ucg_font_7x13r[] UCG_FONT_SECTION("ucg_font_7x13r");
extern const ucg_fntpgm_uint8_t ucg_font_7x14B[] UCG_FONT_SECTION("ucg_font_7x14B");
extern const ucg_fntpgm_uint8_t ucg_font_7x14Br[] UCG_FONT_SECTION("ucg_font_7x14Br");
extern const ucg_fntpgm_uint8_t ucg_font_7x14[] UCG_FONT_SECTION("ucg_font_7x14");
extern const ucg_fntpgm_uint8_t ucg_font_7x14r[] UCG_FONT_SECTION("ucg_font_7x14r");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_67_75[] UCG_FONT_SECTION("ucg_font_8x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_75r[] UCG_FONT_SECTION("ucg_font_8x13_75r");
extern const ucg_fntpgm_uint8_t ucg_font_8x13B[] UCG_FONT_SECTION("ucg_font_8x13B");
extern const ucg_fntpgm_uint8_t ucg_font_8x13Br[] UCG_FONT_SECTION("ucg_font_8x13Br");
extern const ucg_fntpgm_uint8_t ucg_font_8x13[] UCG_FONT_SECTION("ucg_font_8x13");
extern const ucg_fntpgm_uint8_t ucg_font_8x13O[] UCG_FONT_SECTION("ucg_font_8x13O");
extern const ucg_fntpgm_uint8_t ucg_font_8x13Or[] UCG_FONT_SECTION("ucg_font_8x13Or");
extern const ucg_fntpgm_uint8_t ucg_font_8x13r[] UCG_FONT_SECTION("ucg_font_8x13r");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_67_75[] UCG_FONT_SECTION("ucg_font_9x15_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_75r[] UCG_FONT_SECTION("ucg_font_9x15_75r");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_78_79[] UCG_FONT_SECTION("ucg_font_9x15_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_9x15B[] UCG_FONT_SECTION("ucg_font_9x15B");
extern const ucg_fntpgm_uint8_t ucg_font_9x15Br[] UCG_FONT_SECTION("ucg_font_9x15Br");
extern const ucg_fntpgm_uint8_t ucg_font_9x15[] UCG_FONT_SECTION("ucg_font_9x15");
extern const ucg_fntpgm_uint8_t ucg_font_9x15r[] UCG_FONT_SECTION("ucg_font_9x15r");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_67_75[] UCG_FONT_SECTION("ucg_font_9x18_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_75r[] UCG_FONT_SECTION("ucg_font_9x18_75r");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_78_79[] UCG_FONT_SECTION("ucg_font_9x18_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_9x18B[] UCG_FONT_SECTION("ucg_font_9x18B");
extern const ucg_fntpgm_uint8_t ucg_font_9x18Br[] UCG_FONT_SECTION("ucg_font_9x18Br");
extern const ucg_fntpgm_uint8_t ucg_font_9x18[] UCG_FONT_SECTION("ucg_font_9x18");
extern const ucg_fntpgm_uint8_t ucg_font_9x18r[] UCG_FONT_SECTION("ucg_font_9x18r");
extern const ucg_fntpgm_uint8_t ucg_font_baby[] UCG_FONT_SECTION("ucg_font_baby");
extern const ucg_fntpgm_uint8_t ucg_font_babyn[] UCG_FONT_SECTION("ucg_font_babyn");
extern const ucg_fntpgm_uint8_t ucg_font_babyr[] UCG_FONT_SECTION("ucg_font_babyr");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07[] UCG_FONT_SECTION("ucg_font_blipfest_07");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07n[] UCG_FONT_SECTION("ucg_font_blipfest_07n");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07r[] UCG_FONT_SECTION("ucg_font_blipfest_07r");
extern const ucg_fntpgm_uint8_t ucg_font_chikita[] UCG_FONT_SECTION("ucg_font_chikita");
extern const ucg_fntpgm_uint8_t ucg_font_chikitan[] UCG_FONT_SECTION("ucg_font_chikitan");
extern const ucg_fntpgm_uint8_t ucg_font_chikitar[] UCG_FONT_SECTION("ucg_font_chikitar");
extern const ucg_fntpgm_uint8_t ucg_font_courB08[] UCG_FONT_SECTION("ucg_font_courB08");
extern const ucg_fntpgm_uint8_t ucg_font_courB08r[] UCG_FONT_SECTION("ucg_font_courB08r");
extern const ucg_fntpgm_uint8_t ucg_font_courB10[] UCG_FONT_SECTION("ucg_font_courB10");
extern const ucg_fntpgm_uint8_t ucg_font_courB10r[] UCG_FONT_SECTION("ucg_font_courB10r");
extern const ucg_fntpgm_uint8_t ucg_font_courB12[] UCG_FONT_SECTION("ucg_font_courB12");
extern const ucg_fntpgm_uint8_t ucg_font_courB12r[] UCG_FONT_SECTION("ucg_font_courB12r");
extern const ucg_fntpgm_uint8_t ucg_font_courB14[] UCG_FONT_SECTION("ucg_font_courB14");
extern const ucg_fntpgm_uint8_t ucg_font_courB14r[] UCG_FONT_SECTION("ucg_font_courB14r");
extern const ucg_fntpgm_uint8_t ucg_font_courB18[] UCG_FONT_SECTION("ucg_font_courB18");
extern const ucg_fntpgm_uint8_t ucg_font_courB18r[] UCG_FONT_SECTION("ucg_font_courB18r");
extern const ucg_fntpgm_uint8_t ucg_font_courB24[] UCG_FONT_SECTION("ucg_font_courB24");
extern const ucg_fntpgm_uint8_t ucg_font_courB24r[] UCG_FONT_SECTION("ucg_font_courB24r");
extern const ucg_fntpgm_uint8_t ucg_font_courB24n[] UCG_FONT_SECTION("ucg_font_courB24n");
extern const ucg_fntpgm_uint8_t ucg_font_courR08[] UCG_FONT_SECTION("ucg_font_courR08");
extern const ucg_fntpgm_uint8_t ucg_font_courR08r[] UCG_FONT_SECTION("ucg_font_courR08r");
extern const ucg_fntpgm_uint8_t ucg_font_courR10[] UCG_FONT_SECTION("ucg_font_courR10");
extern const ucg_fntpgm_uint8_t ucg_font_courR10r[] UCG_FONT_SECTION("ucg_font_courR10r");
extern const ucg_fntpgm_uint8_t ucg_font_courR12[] UCG_FONT_SECTION("ucg_font_courR12");
extern const ucg_fntpgm_uint8_t ucg_font_courR12r[] UCG_FONT_SECTION("ucg_font_courR12r");
extern const ucg_fntpgm_uint8_t ucg_font_courR14[] UCG_FONT_SECTION("ucg_font_courR14");
extern const ucg_fntpgm_uint8_t ucg_font_courR14r[] UCG_FONT_SECTION("ucg_font_courR14r");
extern const ucg_fntpgm_uint8_t ucg_font_courR18[] UCG_FONT_SECTION("ucg_font_courR18");
extern const ucg_fntpgm_uint8_t ucg_font_courR18r[] UCG_FONT_SECTION("ucg_font_courR18r");
extern const ucg_fntpgm_uint8_t ucg_font_courR24[] UCG_FONT_SECTION("ucg_font_courR24");
extern const ucg_fntpgm_uint8_t ucg_font_courR24n[] UCG_FONT_SECTION("ucg_font_courR24n");
extern const ucg_fntpgm_uint8_t ucg_font_courR24r[] UCG_FONT_SECTION("ucg_font_courR24r");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_67_75[] UCG_FONT_SECTION("ucg_font_cu12_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_75r[] UCG_FONT_SECTION("ucg_font_cu12_75r");
extern const ucg_fntpgm_uint8_t ucg_font_cu12[] UCG_FONT_SECTION("ucg_font_cu12");
extern const ucg_fntpgm_uint8_t ucg_font_cursor[] UCG_FONT_SECTION("ucg_font_cursor");
extern const ucg_fntpgm_uint8_t ucg_font_cursorr[] UCG_FONT_SECTION("ucg_font_cursorr");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0[] UCG_FONT_SECTION("ucg_font_fixed_v0");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0n[] UCG_FONT_SECTION("ucg_font_fixed_v0n");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0r[] UCG_FONT_SECTION("ucg_font_fixed_v0r");
extern const ucg_fntpgm_uint8_t ucg_font_freedoomr10r[] UCG_FONT_SECTION("ucg_font_freedoomr10r");
extern const ucg_fntpgm_uint8_t ucg_font_freedoomr25n[] UCG_FONT_SECTION("ucg_font_freedoomr25n");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08[] UCG_FONT_SECTION("ucg_font_helvB08");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08r[] UCG_FONT_SECTION("ucg_font_helvB08r");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10[] UCG_FONT_SECTION("ucg_font_helvB10");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10r[] UCG_FONT_SECTION("ucg_font_helvB10r");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12[] UCG_FONT_SECTION("ucg_font_helvB12");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12r[] UCG_FONT_SECTION("ucg_font_helvB12r");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14[] UCG_FONT_SECTION("ucg_font_helvB14");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14r[] UCG_FONT_SECTION("ucg_font_helvB14r");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18[] UCG_FONT_SECTION("ucg_font_helvB18");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18r[] UCG_FONT_SECTION("ucg_font_helvB18r");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24[] UCG_FONT_SECTION("ucg_font_helvB24");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24n[] UCG_FONT_SECTION("ucg_font_helvB24n");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24r[] UCG_FONT_SECTION("ucg_font_helvB24r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08[] UCG_FONT_SECTION("ucg_font_helvR08");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08r[] UCG_FONT_SECTION("ucg_font_helvR08r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10[] UCG_FONT_SECTION("ucg_font_helvR10");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10r[] UCG_FONT_SECTION("ucg_font_helvR10r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12[] UCG_FONT_SECTION("ucg_font_helvR12");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12r[] UCG_FONT_SECTION("ucg_font_helvR12r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14[] UCG_FONT_SECTION("ucg_font_helvR14");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14r[] UCG_FONT_SECTION("ucg_font_helvR14r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18[] UCG_FONT_SECTION("ucg_font_helvR18");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18r[] UCG_FONT_SECTION("ucg_font_helvR18r");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24[] UCG_FONT_SECTION("ucg_font_helvR24");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24n[] UCG_FONT_SECTION("ucg_font_helvR24n");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24r[] UCG_FONT_SECTION("ucg_font_helvR24r");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternaten[] UCG_FONT_SECTION("ucg_font_lucasfont_alternaten");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternater[] UCG_FONT_SECTION("ucg_font_lucasfont_alternater");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_5[] UCG_FONT_SECTION("ucg_font_m2icon_5");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_7[] UCG_FONT_SECTION("ucg_font_m2icon_7");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_9[] UCG_FONT_SECTION("ucg_font_m2icon_9");
extern const ucg_fntpgm_uint8_t ucg_font_micro[] UCG_FONT_SECTION("ucg_font_micro");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08[] UCG_FONT_SECTION("ucg_font_ncenB08");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08r[] UCG_FONT_SECTION("ucg_font_ncenB08r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10[] UCG_FONT_SECTION("ucg_font_ncenB10");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10r[] UCG_FONT_SECTION("ucg_font_ncenB10r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12[] UCG_FONT_SECTION("ucg_font_ncenB12");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12r[] UCG_FONT_SECTION("ucg_font_ncenB12r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14[] UCG_FONT_SECTION("ucg_font_ncenB14");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14r[] UCG_FONT_SECTION("ucg_font_ncenB14r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18[] UCG_FONT_SECTION("ucg_font_ncenB18");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18r[] UCG_FONT_SECTION("ucg_font_ncenB18r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24[] UCG_FONT_SECTION("ucg_font_ncenB24");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24n[] UCG_FONT_SECTION("ucg_font_ncenB24n");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24r[] UCG_FONT_SECTION("ucg_font_ncenB24r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08[] UCG_FONT_SECTION("ucg_font_ncenR08");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08r[] UCG_FONT_SECTION("ucg_font_ncenR08r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10[] UCG_FONT_SECTION("ucg_font_ncenR10");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10r[] UCG_FONT_SECTION("ucg_font_ncenR10r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12[] UCG_FONT_SECTION("ucg_font_ncenR12");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12r[] UCG_FONT_SECTION("ucg_font_ncenR12r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14[] UCG_FONT_SECTION("ucg_font_ncenR14");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14r[] UCG_FONT_SECTION("ucg_font_ncenR14r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18[] UCG_FONT_SECTION("ucg_font_ncenR18");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18r[] UCG_FONT_SECTION("ucg_font_ncenR18r");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24[] UCG_FONT_SECTION("ucg_font_ncenR24");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24n[] UCG_FONT_SECTION("ucg_font_ncenR24n");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24r[] UCG_FONT_SECTION("ucg_font_ncenR24r");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01[] UCG_FONT_SECTION("ucg_font_orgv01");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01n[] UCG_FONT_SECTION("ucg_font_orgv01n");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01r[] UCG_FONT_SECTION("ucg_font_orgv01r");
extern const ucg_fntpgm_uint8_t ucg_font_p01type[] UCG_FONT_SECTION("ucg_font_p01type");
extern const ucg_fntpgm_uint8_t ucg_font_p01typen[] UCG_FONT_SECTION("ucg_font_p01typen");
extern const ucg_fntpgm_uint8_t ucg_font_p01typer[] UCG_FONT_SECTION("ucg_font_p01typer");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro[] UCG_FONT_SECTION("ucg_font_pixelle_micro");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micron[] UCG_FONT_SECTION("ucg_font_pixelle_micron");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micror[] UCG_FONT_SECTION("ucg_font_pixelle_micror");
extern const ucg_fntpgm_uint8_t ucg_font_profont10[] UCG_FONT_SECTION("ucg_font_profont10");
extern const ucg_fntpgm_uint8_t ucg_font_profont10r[] UCG_FONT_SECTION("ucg_font_profont10r");
extern const ucg_fntpgm_uint8_t ucg_font_profont11[] UCG_FONT_SECTION("ucg_font_profont11");
extern const ucg_fntpgm_uint8_t ucg_font_profont11r[] UCG_FONT_SECTION("ucg_font_profont11r");
extern const ucg_fntpgm_uint8_t ucg_font_profont12[] UCG_FONT_SECTION("ucg_font_profont12");
extern const ucg_fntpgm_uint8_t ucg_font_profont12r[] UCG_FONT_SECTION("ucg_font_profont12r");
extern const ucg_fntpgm_uint8_t ucg_font_profont15[] UCG_FONT_SECTION("ucg_font_profont15");
extern const ucg_fntpgm_uint8_t ucg_font_profont15r[] UCG_FONT_SECTION("ucg_font_profont15r");
extern const ucg_fntpgm_uint8_t ucg_font_profont17[] UCG_FONT_SECTION("ucg_font_profont17");
extern const ucg_fntpgm_uint8_t ucg_font_profont17r[] UCG_FONT_SECTION("ucg_font_profont17r");
extern const ucg_fntpgm_uint8_t ucg_font_profont22[] UCG_FONT_SECTION("ucg_font_profont22");
extern const ucg_fntpgm_uint8_t ucg_font_profont22n[] UCG_FONT_SECTION("ucg_font_profont22n");
extern const ucg_fntpgm_uint8_t ucg_font_profont22r[] UCG_FONT_SECTION("ucg_font_profont22r");
extern const ucg_fntpgm_uint8_t ucg_font_profont29[] UCG_FONT_SECTION("ucg_font_profont29");
extern const ucg_fntpgm_uint8_t ucg_font_profont29n[] UCG_FONT_SECTION("ucg_font_profont29n");
extern const ucg_fntpgm_uint8_t ucg_font_profont29r[] UCG_FONT_SECTION("ucg_font_profont29r");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro[] UCG_FONT_SECTION("ucg_font_robot_de_niro");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niron[] UCG_FONT_SECTION("ucg_font_robot_de_niron");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niror[] UCG_FONT_SECTION("ucg_font_robot_de_niror");
extern const ucg_fntpgm_uint8_t ucg_font_symb08[] UCG_FONT_SECTION("ucg_font_symb08");
extern const ucg_fntpgm_uint8_t ucg_font_symb08r[] UCG_FONT_SECTION("ucg_font_symb08r");
extern const ucg_fntpgm_uint8_t ucg_font_symb10[] UCG_FONT_SECTION("ucg_font_symb10");
extern const ucg_fntpgm_uint8_t ucg_font_symb10r[] UCG_FONT_SECTION("ucg_font_symb10r");
extern const ucg_fntpgm_uint8_t ucg_font_symb12[] UCG_FONT_SECTION("ucg_font_symb12");
extern const ucg_fntpgm_uint8_t ucg_font_symb12r[] UCG_FONT_SECTION("ucg_font_symb12r");
extern const ucg_fntpgm_uint8_t ucg_font_symb14[] UCG_FONT_SECTION("ucg_font_symb14");
extern const ucg_fntpgm_uint8_t ucg_font_symb14r[] UCG_FONT_SECTION("ucg_font_symb14r");
extern const ucg_fntpgm_uint8_t ucg_font_symb18[] UCG_FONT_SECTION("ucg_font_symb18");
extern const ucg_fntpgm_uint8_t ucg_font_symb18r[] UCG_FONT_SECTION("ucg_font_symb18r");
extern const ucg_fntpgm_uint8_t ucg_font_symb24[] UCG_FONT_SECTION("ucg_font_symb24");
extern const ucg_fntpgm_uint8_t ucg_font_symb24r[] UCG_FONT_SECTION("ucg_font_symb24r");
extern const ucg_fntpgm_uint8_t ucg_font_timB08[] UCG_FONT_SECTION("ucg_font_timB08");
extern const ucg_fntpgm_uint8_t ucg_font_timB08r[] UCG_FONT_SECTION("ucg_font_timB08r");
extern const ucg_fntpgm_uint8_t ucg_font_timB10[] UCG_FONT_SECTION("ucg_font_timB10");
extern const ucg_fntpgm_uint8_t ucg_font_timB10r[] UCG_FONT_SECTION("ucg_font_timB10r");
extern const ucg_fntpgm_uint8_t ucg_font_timB12[] UCG_FONT_SECTION("ucg_font_timB12");
extern const ucg_fntpgm_uint8_t ucg_font_timB12r[] UCG_FONT_SECTION("ucg_font_timB12r");
extern const ucg_fntpgm_uint8_t ucg_font_timB14[] UCG_FONT_SECTION("ucg_font_timB14");
extern const ucg_fntpgm_uint8_t ucg_font_timB14r[] UCG_FONT_SECTION("ucg_font_timB14r");
extern const ucg_fntpgm_uint8_t ucg_font_timB18[] UCG_FONT_SECTION("ucg_font_timB18");
extern const ucg_fntpgm_uint8_t ucg_font_timB18r[] UCG_FONT_SECTION("ucg_font_timB18r");
extern const ucg_fntpgm_uint8_t ucg_font_timB24[] UCG_FONT_SECTION("ucg_font_timB24");
extern const ucg_fntpgm_uint8_t ucg_font_timB24n[] UCG_FONT_SECTION("ucg_font_timB24n");
extern const ucg_fntpgm_uint8_t ucg_font_timB24r[] UCG_FONT_SECTION("ucg_font_timB24r");
extern const ucg_fntpgm_uint8_t ucg_font_timR08[] UCG_FONT_SECTION("ucg_font_timR08");
extern const ucg_fntpgm_uint8_t ucg_font_timR08r[] UCG_FONT_SECTION("ucg_font_timR08r");
extern const ucg_fntpgm_uint8_t ucg_font_timR10[] UCG_FONT_SECTION("ucg_font_timR10");
extern const ucg_fntpgm_uint8_t ucg_font_timR10r[] UCG_FONT_SECTION("ucg_font_timR10r");
extern const ucg_fntpgm_uint8_t ucg_font_timR12[] UCG_FONT_SECTION("ucg_font_timR12");
extern const ucg_fntpgm_uint8_t ucg_font_timR12r[] UCG_FONT_SECTION("ucg_font_timR12r");
extern const ucg_fntpgm_uint8_t ucg_font_timR14[] UCG_FONT_SECTION("ucg_font_timR14");
extern const ucg_fntpgm_uint8_t ucg_font_timR14r[] UCG_FONT_SECTION("ucg_font_timR14r");
extern const ucg_fntpgm_uint8_t ucg_font_timR18[] UCG_FONT_SECTION("ucg_font_timR18");
extern const ucg_fntpgm_uint8_t ucg_font_timR18r[] UCG_FONT_SECTION("ucg_font_timR18r");
extern const ucg_fntpgm_uint8_t ucg_font_timR24[] UCG_FONT_SECTION("ucg_font_timR24");
extern const ucg_fntpgm_uint8_t ucg_font_timR24n[] UCG_FONT_SECTION("ucg_font_timR24n");
extern const ucg_fntpgm_uint8_t ucg_font_timR24r[] UCG_FONT_SECTION("ucg_font_timR24r");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb[] UCG_FONT_SECTION("ucg_font_tpssb");
extern const ucg_fntpgm_uint8_t ucg_font_tpssbn[] UCG_FONT_SECTION("ucg_font_tpssbn");
extern const ucg_fntpgm_uint8_t ucg_font_tpssbr[] UCG_FONT_SECTION("ucg_font_tpssbr");
extern const ucg_fntpgm_uint8_t ucg_font_tpss[] UCG_FONT_SECTION("ucg_font_tpss");
extern const ucg_fntpgm_uint8_t ucg_font_tpssn[] UCG_FONT_SECTION("ucg_font_tpssn");
extern const ucg_fntpgm_uint8_t ucg_font_tpssr[] UCG_FONT_SECTION("ucg_font_tpssr");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square[] UCG_FONT_SECTION("ucg_font_trixel_square");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_squaren[] UCG_FONT_SECTION("ucg_font_trixel_squaren");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_squarer[] UCG_FONT_SECTION("ucg_font_trixel_squarer");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4[] UCG_FONT_SECTION("ucg_font_u8glib_4");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4r[] UCG_FONT_SECTION("ucg_font_u8glib_4r");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_0_8[] UCG_FONT_SECTION("ucg_font_unifont_0_8");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_12_13[] UCG_FONT_SECTION("ucg_font_unifont_12_13");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_18_19[] UCG_FONT_SECTION("ucg_font_unifont_18_19");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_2_3[] UCG_FONT_SECTION("ucg_font_unifont_2_3");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_4_5[] UCG_FONT_SECTION("ucg_font_unifont_4_5");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_67_75[] UCG_FONT_SECTION("ucg_font_unifont_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_72_73[] UCG_FONT_SECTION("ucg_font_unifont_72_73");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_75r[] UCG_FONT_SECTION("ucg_font_unifont_75r");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_76[] UCG_FONT_SECTION("ucg_font_unifont_76");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_77[] UCG_FONT_SECTION("ucg_font_unifont_77");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_78_79[] UCG_FONT_SECTION("ucg_font_unifont_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_86[] UCG_FONT_SECTION("ucg_font_unifont_86");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_8_9[] UCG_FONT_SECTION("ucg_font_unifont_8_9");
extern const ucg_fntpgm_uint8_t ucg_font_unifont[] UCG_FONT_SECTION("ucg_font_unifont");
extern const ucg_fntpgm_uint8_t ucg_font_unifontr[] UCG_FONT_SECTION("ucg_font_unifontr");
extern const ucg_fntpgm_uint8_t ucg_font_fub11[] UCG_FONT_SECTION("ucg_font_fub11");
extern const ucg_fntpgm_uint8_t ucg_font_fub11n[] UCG_FONT_SECTION("ucg_font_fub11n");
extern const ucg_fntpgm_uint8_t ucg_font_fub11r[] UCG_FONT_SECTION("ucg_font_fub11r");
extern const ucg_fntpgm_uint8_t ucg_font_fub14[] UCG_FONT_SECTION("ucg_font_fub14");
extern const ucg_fntpgm_uint8_t ucg_font_fub14n[] UCG_FONT_SECTION("ucg_font_fub14n");
extern const ucg_fntpgm_uint8_t ucg_font_fub14r[] UCG_FONT_SECTION("ucg_font_fub14r");
extern const ucg_fntpgm_uint8_t ucg_font_fub17[] UCG_FONT_SECTION("ucg_font_fub17");
extern const ucg_fntpgm_uint8_t ucg_font_fub17n[] UCG_FONT_SECTION("ucg_font_fub17n");
extern const ucg_fntpgm_uint8_t ucg_font_fub17r[] UCG_FONT_SECTION("ucg_font_fub17r");
extern const ucg_fntpgm_uint8_t ucg_font_fub20[] UCG_FONT_SECTION("ucg_font_fub20");
extern const ucg_fntpgm_uint8_t ucg_font_fub20n[] UCG_FONT_SECTION("ucg_font_fub20n");
extern const ucg_fntpgm_uint8_t ucg_font_fub20r[] UCG_FONT_SECTION("ucg_font_fub20r");
extern const ucg_fntpgm_uint8_t ucg_font_fub25[] UCG_FONT_SECTION("ucg_font_fub25");
extern const ucg_fntpgm_uint8_t ucg_font_fub25n[] UCG_FONT_SECTION("ucg_font_fub25n");
extern const ucg_fntpgm_uint8_t ucg_font_fub25r[] UCG_FONT_SECTION("ucg_font_fub25r");
extern const ucg_fntpgm_uint8_t ucg_font_fub30[] UCG_FONT_SECTION("ucg_font_fub30");
extern const ucg_fntpgm_uint8_t ucg_font_fub30n[] UCG_FONT_SECTION("ucg_font_fub30n");
extern const ucg_fntpgm_uint8_t ucg_font_fub30r[] UCG_FONT_SECTION("ucg_font_fub30r");
extern const ucg_fntpgm_uint8_t ucg_font_fub35n[] UCG_FONT_SECTION("ucg_font_fub35n");
extern const ucg_fntpgm_uint8_t ucg_font_fub42n[] UCG_FONT_SECTION("ucg_font_fub42n");
extern const ucg_fntpgm_uint8_t ucg_font_fub49n[] UCG_FONT_SECTION("ucg_font_fub49n");
extern const ucg_fntpgm_uint8_t ucg_font_fur11[] UCG_FONT_SECTION("ucg_font_fur11");
extern const ucg_fntpgm_uint8_t ucg_font_fur11n[] UCG_FONT_SECTION("ucg_font_fur11n");
extern const ucg_fntpgm_uint8_t ucg_font_fur11r[] UCG_FONT_SECTION("ucg_font_fur11r");
extern const ucg_fntpgm_uint8_t ucg_font_fur14[] UCG_FONT_SECTION("ucg_font_fur14");
extern const ucg_fntpgm_uint8_t ucg_font_fur14n[] UCG_FONT_SECTION("ucg_font_fur14n");
extern const ucg_fntpgm_uint8_t ucg_font_fur14r[] UCG_FONT_SECTION("ucg_font_fur14r");
extern const ucg_fntpgm_uint8_t ucg_font_fur17[] UCG_FONT_SECTION("ucg_font_fur17");
extern const ucg_fntpgm_uint8_t ucg_font_fur17n[] UCG_FONT_SECTION("ucg_font_fur17n");
extern const ucg_fntpgm_uint8_t ucg_font_fur17r[] UCG_FONT_SECTION("ucg_font_fur17r");
extern const ucg_fntpgm_uint8_t ucg_font_fur20[] UCG_FONT_SECTION("ucg_font_fur20");
extern const ucg_fntpgm_uint8_t ucg_font_fur20n[] UCG_FONT_SECTION("ucg_font_fur20n");
extern const ucg_fntpgm_uint8_t ucg_font_fur20r[] UCG_FONT_SECTION("ucg_font_fur20r");
extern const ucg_fntpgm_uint8_t ucg_font_fur25[] UCG_FONT_SECTION("ucg_font_fur25");
extern const ucg_fntpgm_uint8_t ucg_font_fur25n[] UCG_FONT_SECTION("ucg_font_fur25n");
extern const ucg_fntpgm_uint8_t ucg_font_fur25r[] UCG_FONT_SECTION("ucg_font_fur25r");
extern const ucg_fntpgm_uint8_t ucg_font_fur30[] UCG_FONT_SECTION("ucg_font_fur30");
extern const ucg_fntpgm_uint8_t ucg_font_fur30n[] UCG_FONT_SECTION("ucg_font_fur30n");
extern const ucg_fntpgm_uint8_t ucg_font_fur30r[] UCG_FONT_SECTION("ucg_font_fur30r");
extern const ucg_fntpgm_uint8_t ucg_font_fur35n[] UCG_FONT_SECTION("ucg_font_fur35n");
extern const ucg_fntpgm_uint8_t ucg_font_fur42n[] UCG_FONT_SECTION("ucg_font_fur42n");
extern const ucg_fntpgm_uint8_t ucg_font_fur49n[] UCG_FONT_SECTION("ucg_font_fur49n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16[] UCG_FONT_SECTION("ucg_font_logisoso16");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16n[] UCG_FONT_SECTION("ucg_font_logisoso16n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16r[] UCG_FONT_SECTION("ucg_font_logisoso16r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18[] UCG_FONT_SECTION("ucg_font_logisoso18");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18n[] UCG_FONT_SECTION("ucg_font_logisoso18n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18r[] UCG_FONT_SECTION("ucg_font_logisoso18r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20[] UCG_FONT_SECTION("ucg_font_logisoso20");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20n[] UCG_FONT_SECTION("ucg_font_logisoso20n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20r[] UCG_FONT_SECTION("ucg_font_logisoso20r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22[] UCG_FONT_SECTION("ucg_font_logisoso22");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22n[] UCG_FONT_SECTION("ucg_font_logisoso22n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22r[] UCG_FONT_SECTION("ucg_font_logisoso22r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24[] UCG_FONT_SECTION("ucg_font_logisoso24");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24n[] UCG_FONT_SECTION("ucg_font_logisoso24n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24r[] UCG_FONT_SECTION("ucg_font_logisoso24r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26[] UCG_FONT_SECTION("ucg_font_logisoso26");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26n[] UCG_FONT_SECTION("ucg_font_logisoso26n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26r[] UCG_FONT_SECTION("ucg_font_logisoso26r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28[] UCG_FONT_SECTION("ucg_font_logisoso28");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28n[] UCG_FONT_SECTION("ucg_font_logisoso28n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28r[] UCG_FONT_SECTION("ucg_font_logisoso28r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30[] UCG_FONT_SECTION("ucg_font_logisoso30");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30n[] UCG_FONT_SECTION("ucg_font_logisoso30n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30r[] UCG_FONT_SECTION("ucg_font_logisoso30r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32[] UCG_FONT_SECTION("ucg_font_logisoso32");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32n[] UCG_FONT_SECTION("ucg_font_logisoso32n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32r[] UCG_FONT_SECTION("ucg_font_logisoso32r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34[] UCG_FONT_SECTION("ucg_font_logisoso34");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34n[] UCG_FONT_SECTION("ucg_font_logisoso34n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34r[] UCG_FONT_SECTION("ucg_font_logisoso34r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38[] UCG_FONT_SECTION("ucg_font_logisoso38");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38n[] UCG_FONT_SECTION("ucg_font_logisoso38n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38r[] UCG_FONT_SECTION("ucg_font_logisoso38r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42[] UCG_FONT_SECTION("ucg_font_logisoso42");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42n[] UCG_FONT_SECTION("ucg_font_logisoso42n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42r[] UCG_FONT_SECTION("ucg_font_logisoso42r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso46n[] UCG_FONT_SECTION("ucg_font_logisoso46n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso46r[] UCG_FONT_SECTION("ucg_font_logisoso46r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso50n[] UCG_FONT_SECTION("ucg_font_logisoso50n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso50r[] UCG_FONT_SECTION("ucg_font_logisoso50r");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso54n[] UCG_FONT_SECTION("ucg_font_logisoso54n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso58n[] UCG_FONT_SECTION("ucg_font_logisoso58n");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso62n[] UCG_FONT_SECTION("ucg_font_logisoso62n");
extern const ucg_fntpgm_uint8_t ucg_font_osb18[] UCG_FONT_SECTION("ucg_font_osb18");
extern const ucg_fntpgm_uint8_t ucg_font_osb18n[] UCG_FONT_SECTION("ucg_font_osb18n");
extern const ucg_fntpgm_uint8_t ucg_font_osb18r[] UCG_FONT_SECTION("ucg_font_osb18r");
extern const ucg_fntpgm_uint8_t ucg_font_osb21[] UCG_FONT_SECTION("ucg_font_osb21");
extern const ucg_fntpgm_uint8_t ucg_font_osb21n[] UCG_FONT_SECTION("ucg_font_osb21n");
extern const ucg_fntpgm_uint8_t ucg_font_osb21r[] UCG_FONT_SECTION("ucg_font_osb21r");
extern const ucg_fntpgm_uint8_t ucg_font_osb26[] UCG_FONT_SECTION("ucg_font_osb26");
extern const ucg_fntpgm_uint8_t ucg_font_osb26n[] UCG_FONT_SECTION("ucg_font_osb26n");
extern const ucg_fntpgm_uint8_t ucg_font_osb26r[] UCG_FONT_SECTION("ucg_font_osb26r");
extern const ucg_fntpgm_uint8_t ucg_font_osb29[] UCG_FONT_SECTION("ucg_font_osb29");
extern const ucg_fntpgm_uint8_t ucg_font_osb29n[] UCG_FONT_SECTION("ucg_font_osb29n");
extern const ucg_fntpgm_uint8_t ucg_font_osb29r[] UCG_FONT_SECTION("ucg_font_osb29r");
extern const ucg_fntpgm_uint8_t ucg_font_osb35[] UCG_FONT_SECTION("ucg_font_osb35");
extern const ucg_fntpgm_uint8_t ucg_font_osb35n[] UCG_FONT_SECTION("ucg_font_osb35n");
extern const ucg_fntpgm_uint8_t ucg_font_osb35r[] UCG_FONT_SECTION("ucg_font_osb35r");
extern const ucg_fntpgm_uint8_t ucg_font_osr18[] UCG_FONT_SECTION("ucg_font_osr18");
extern const ucg_fntpgm_uint8_t ucg_font_osr18n[] UCG_FONT_SECTION("ucg_font_osr18n");
extern const ucg_fntpgm_uint8_t ucg_font_osr18r[] UCG_FONT_SECTION("ucg_font_osr18r");
extern const ucg_fntpgm_uint8_t ucg_font_osr21[] UCG_FONT_SECTION("ucg_font_osr21");
extern const ucg_fntpgm_uint8_t ucg_font_osr21n[] UCG_FONT_SECTION("ucg_font_osr21n");
extern const ucg_fntpgm_uint8_t ucg_font_osr21r[] UCG_FONT_SECTION("ucg_font_osr21r");
extern const ucg_fntpgm_uint8_t ucg_font_osr26[] UCG_FONT_SECTION("ucg_font_osr26");
extern const ucg_fntpgm_uint8_t ucg_font_osr26n[] UCG_FONT_SECTION("ucg_font_osr26n");
extern const ucg_fntpgm_uint8_t ucg_font_osr26r[] UCG_FONT_SECTION("ucg_font_osr26r");
extern const ucg_fntpgm_uint8_t ucg_font_osr29[] UCG_FONT_SECTION("ucg_font_osr29");
extern const ucg_fntpgm_uint8_t ucg_font_osr29n[] UCG_FONT_SECTION("ucg_font_osr29n");
extern const ucg_fntpgm_uint8_t ucg_font_osr29r[] UCG_FONT_SECTION("ucg_font_osr29r");
extern const ucg_fntpgm_uint8_t ucg_font_osr35[] UCG_FONT_SECTION("ucg_font_osr35");
extern const ucg_fntpgm_uint8_t ucg_font_osr35n[] UCG_FONT_SECTION("ucg_font_osr35n");
extern const ucg_fntpgm_uint8_t ucg_font_osr35r[] UCG_FONT_SECTION("ucg_font_osr35r");

#else

extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_hf[] UCG_FONT_SECTION("ucg_font_04b_03b_hf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_hn[] UCG_FONT_SECTION("ucg_font_04b_03b_hn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_hr[] UCG_FONT_SECTION("ucg_font_04b_03b_hr");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_tf[] UCG_FONT_SECTION("ucg_font_04b_03b_tf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_tn[] UCG_FONT_SECTION("ucg_font_04b_03b_tn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03b_tr[] UCG_FONT_SECTION("ucg_font_04b_03b_tr");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_hf[] UCG_FONT_SECTION("ucg_font_04b_03_hf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_hn[] UCG_FONT_SECTION("ucg_font_04b_03_hn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_hr[] UCG_FONT_SECTION("ucg_font_04b_03_hr");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_tf[] UCG_FONT_SECTION("ucg_font_04b_03_tf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_tn[] UCG_FONT_SECTION("ucg_font_04b_03_tn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_03_tr[] UCG_FONT_SECTION("ucg_font_04b_03_tr");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_hf[] UCG_FONT_SECTION("ucg_font_04b_24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_hn[] UCG_FONT_SECTION("ucg_font_04b_24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_hr[] UCG_FONT_SECTION("ucg_font_04b_24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_tf[] UCG_FONT_SECTION("ucg_font_04b_24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_tn[] UCG_FONT_SECTION("ucg_font_04b_24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_04b_24_tr[] UCG_FONT_SECTION("ucg_font_04b_24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_67_75[] UCG_FONT_SECTION("ucg_font_10x20_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_78_79[] UCG_FONT_SECTION("ucg_font_10x20_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_mf[] UCG_FONT_SECTION("ucg_font_10x20_mf");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_mr[] UCG_FONT_SECTION("ucg_font_10x20_mr");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_tf[] UCG_FONT_SECTION("ucg_font_10x20_tf");
extern const ucg_fntpgm_uint8_t ucg_font_10x20_tr[] UCG_FONT_SECTION("ucg_font_10x20_tr");
extern const ucg_fntpgm_uint8_t ucg_font_4x6_mf[] UCG_FONT_SECTION("ucg_font_4x6_mf");
extern const ucg_fntpgm_uint8_t ucg_font_4x6_mr[] UCG_FONT_SECTION("ucg_font_4x6_mr");
extern const ucg_fntpgm_uint8_t ucg_font_4x6_tf[] UCG_FONT_SECTION("ucg_font_4x6_tf");
extern const ucg_fntpgm_uint8_t ucg_font_4x6_tr[] UCG_FONT_SECTION("ucg_font_4x6_tr");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_8f[] UCG_FONT_SECTION("ucg_font_5x7_8f");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_8r[] UCG_FONT_SECTION("ucg_font_5x7_8r");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_mf[] UCG_FONT_SECTION("ucg_font_5x7_mf");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_mr[] UCG_FONT_SECTION("ucg_font_5x7_mr");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_tf[] UCG_FONT_SECTION("ucg_font_5x7_tf");
extern const ucg_fntpgm_uint8_t ucg_font_5x7_tr[] UCG_FONT_SECTION("ucg_font_5x7_tr");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_8f[] UCG_FONT_SECTION("ucg_font_5x8_8f");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_8r[] UCG_FONT_SECTION("ucg_font_5x8_8r");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_mf[] UCG_FONT_SECTION("ucg_font_5x8_mf");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_mr[] UCG_FONT_SECTION("ucg_font_5x8_mr");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_tf[] UCG_FONT_SECTION("ucg_font_5x8_tf");
extern const ucg_fntpgm_uint8_t ucg_font_5x8_tr[] UCG_FONT_SECTION("ucg_font_5x8_tr");
extern const ucg_fntpgm_uint8_t ucg_font_6x10_mf[] UCG_FONT_SECTION("ucg_font_6x10_mf");
extern const ucg_fntpgm_uint8_t ucg_font_6x10_mr[] UCG_FONT_SECTION("ucg_font_6x10_mr");
extern const ucg_fntpgm_uint8_t ucg_font_6x10_tf[] UCG_FONT_SECTION("ucg_font_6x10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_6x10_tr[] UCG_FONT_SECTION("ucg_font_6x10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_67_75[] UCG_FONT_SECTION("ucg_font_6x12_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_75[] UCG_FONT_SECTION("ucg_font_6x12_75");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_78_79[] UCG_FONT_SECTION("ucg_font_6x12_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_mf[] UCG_FONT_SECTION("ucg_font_6x12_mf");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_mr[] UCG_FONT_SECTION("ucg_font_6x12_mr");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_tf[] UCG_FONT_SECTION("ucg_font_6x12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_6x12_tr[] UCG_FONT_SECTION("ucg_font_6x12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_67_75[] UCG_FONT_SECTION("ucg_font_6x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_78_79[] UCG_FONT_SECTION("ucg_font_6x13_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_6x13B_tf[] UCG_FONT_SECTION("ucg_font_6x13B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_6x13B_tr[] UCG_FONT_SECTION("ucg_font_6x13B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_mf[] UCG_FONT_SECTION("ucg_font_6x13_mf");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_mr[] UCG_FONT_SECTION("ucg_font_6x13_mr");
extern const ucg_fntpgm_uint8_t ucg_font_6x13O_tf[] UCG_FONT_SECTION("ucg_font_6x13O_tf");
extern const ucg_fntpgm_uint8_t ucg_font_6x13O_tr[] UCG_FONT_SECTION("ucg_font_6x13O_tr");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_tf[] UCG_FONT_SECTION("ucg_font_6x13_tf");
extern const ucg_fntpgm_uint8_t ucg_font_6x13_tr[] UCG_FONT_SECTION("ucg_font_6x13_tr");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_67_75[] UCG_FONT_SECTION("ucg_font_7x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_78_79[] UCG_FONT_SECTION("ucg_font_7x13_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_7x13B_tf[] UCG_FONT_SECTION("ucg_font_7x13B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_7x13B_tr[] UCG_FONT_SECTION("ucg_font_7x13B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_mf[] UCG_FONT_SECTION("ucg_font_7x13_mf");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_mr[] UCG_FONT_SECTION("ucg_font_7x13_mr");
extern const ucg_fntpgm_uint8_t ucg_font_7x13O_tf[] UCG_FONT_SECTION("ucg_font_7x13O_tf");
extern const ucg_fntpgm_uint8_t ucg_font_7x13O_tr[] UCG_FONT_SECTION("ucg_font_7x13O_tr");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_tf[] UCG_FONT_SECTION("ucg_font_7x13_tf");
extern const ucg_fntpgm_uint8_t ucg_font_7x13_tr[] UCG_FONT_SECTION("ucg_font_7x13_tr");
extern const ucg_fntpgm_uint8_t ucg_font_7x14B_mf[] UCG_FONT_SECTION("ucg_font_7x14B_mf");
extern const ucg_fntpgm_uint8_t ucg_font_7x14B_mr[] UCG_FONT_SECTION("ucg_font_7x14B_mr");
extern const ucg_fntpgm_uint8_t ucg_font_7x14B_tf[] UCG_FONT_SECTION("ucg_font_7x14B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_7x14B_tr[] UCG_FONT_SECTION("ucg_font_7x14B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_7x14_mf[] UCG_FONT_SECTION("ucg_font_7x14_mf");
extern const ucg_fntpgm_uint8_t ucg_font_7x14_mr[] UCG_FONT_SECTION("ucg_font_7x14_mr");
extern const ucg_fntpgm_uint8_t ucg_font_7x14_tf[] UCG_FONT_SECTION("ucg_font_7x14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_7x14_tr[] UCG_FONT_SECTION("ucg_font_7x14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_67_75[] UCG_FONT_SECTION("ucg_font_8x13_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_8x13B_mf[] UCG_FONT_SECTION("ucg_font_8x13B_mf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13B_mr[] UCG_FONT_SECTION("ucg_font_8x13B_mr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13B_tf[] UCG_FONT_SECTION("ucg_font_8x13B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13B_tr[] UCG_FONT_SECTION("ucg_font_8x13B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_mf[] UCG_FONT_SECTION("ucg_font_8x13_mf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_mr[] UCG_FONT_SECTION("ucg_font_8x13_mr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13O_mf[] UCG_FONT_SECTION("ucg_font_8x13O_mf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13O_mr[] UCG_FONT_SECTION("ucg_font_8x13O_mr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13O_tf[] UCG_FONT_SECTION("ucg_font_8x13O_tf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13O_tr[] UCG_FONT_SECTION("ucg_font_8x13O_tr");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_tf[] UCG_FONT_SECTION("ucg_font_8x13_tf");
extern const ucg_fntpgm_uint8_t ucg_font_8x13_tr[] UCG_FONT_SECTION("ucg_font_8x13_tr");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_67_75[] UCG_FONT_SECTION("ucg_font_9x15_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_78_79[] UCG_FONT_SECTION("ucg_font_9x15_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_9x15B_mf[] UCG_FONT_SECTION("ucg_font_9x15B_mf");
extern const ucg_fntpgm_uint8_t ucg_font_9x15B_mr[] UCG_FONT_SECTION("ucg_font_9x15B_mr");
extern const ucg_fntpgm_uint8_t ucg_font_9x15B_tf[] UCG_FONT_SECTION("ucg_font_9x15B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_9x15B_tr[] UCG_FONT_SECTION("ucg_font_9x15B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_mf[] UCG_FONT_SECTION("ucg_font_9x15_mf");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_mr[] UCG_FONT_SECTION("ucg_font_9x15_mr");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_tf[] UCG_FONT_SECTION("ucg_font_9x15_tf");
extern const ucg_fntpgm_uint8_t ucg_font_9x15_tr[] UCG_FONT_SECTION("ucg_font_9x15_tr");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_67_75[] UCG_FONT_SECTION("ucg_font_9x18_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_78_79[] UCG_FONT_SECTION("ucg_font_9x18_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_9x18B_mf[] UCG_FONT_SECTION("ucg_font_9x18B_mf");
extern const ucg_fntpgm_uint8_t ucg_font_9x18B_mr[] UCG_FONT_SECTION("ucg_font_9x18B_mr");
extern const ucg_fntpgm_uint8_t ucg_font_9x18B_tf[] UCG_FONT_SECTION("ucg_font_9x18B_tf");
extern const ucg_fntpgm_uint8_t ucg_font_9x18B_tr[] UCG_FONT_SECTION("ucg_font_9x18B_tr");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_mf[] UCG_FONT_SECTION("ucg_font_9x18_mf");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_mr[] UCG_FONT_SECTION("ucg_font_9x18_mr");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_tf[] UCG_FONT_SECTION("ucg_font_9x18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_9x18_tr[] UCG_FONT_SECTION("ucg_font_9x18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_amstrad_cpc_8f[] UCG_FONT_SECTION("ucg_font_amstrad_cpc_8f");
extern const ucg_fntpgm_uint8_t ucg_font_amstrad_cpc_8r[] UCG_FONT_SECTION("ucg_font_amstrad_cpc_8r");
extern const ucg_fntpgm_uint8_t ucg_font_baby_hf[] UCG_FONT_SECTION("ucg_font_baby_hf");
extern const ucg_fntpgm_uint8_t ucg_font_baby_hn[] UCG_FONT_SECTION("ucg_font_baby_hn");
extern const ucg_fntpgm_uint8_t ucg_font_baby_hr[] UCG_FONT_SECTION("ucg_font_baby_hr");
extern const ucg_fntpgm_uint8_t ucg_font_baby_tf[] UCG_FONT_SECTION("ucg_font_baby_tf");
extern const ucg_fntpgm_uint8_t ucg_font_baby_tn[] UCG_FONT_SECTION("ucg_font_baby_tn");
extern const ucg_fntpgm_uint8_t ucg_font_baby_tr[] UCG_FONT_SECTION("ucg_font_baby_tr");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_hf[] UCG_FONT_SECTION("ucg_font_blipfest_07_hf");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_hn[] UCG_FONT_SECTION("ucg_font_blipfest_07_hn");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_hr[] UCG_FONT_SECTION("ucg_font_blipfest_07_hr");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_tf[] UCG_FONT_SECTION("ucg_font_blipfest_07_tf");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_tn[] UCG_FONT_SECTION("ucg_font_blipfest_07_tn");
extern const ucg_fntpgm_uint8_t ucg_font_blipfest_07_tr[] UCG_FONT_SECTION("ucg_font_blipfest_07_tr");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_hf[] UCG_FONT_SECTION("ucg_font_chikita_hf");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_hn[] UCG_FONT_SECTION("ucg_font_chikita_hn");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_hr[] UCG_FONT_SECTION("ucg_font_chikita_hr");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_tf[] UCG_FONT_SECTION("ucg_font_chikita_tf");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_tn[] UCG_FONT_SECTION("ucg_font_chikita_tn");
extern const ucg_fntpgm_uint8_t ucg_font_chikita_tr[] UCG_FONT_SECTION("ucg_font_chikita_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB08_mf[] UCG_FONT_SECTION("ucg_font_courB08_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB08_mr[] UCG_FONT_SECTION("ucg_font_courB08_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB08_tf[] UCG_FONT_SECTION("ucg_font_courB08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB08_tr[] UCG_FONT_SECTION("ucg_font_courB08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB10_mf[] UCG_FONT_SECTION("ucg_font_courB10_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB10_mr[] UCG_FONT_SECTION("ucg_font_courB10_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB10_tf[] UCG_FONT_SECTION("ucg_font_courB10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB10_tr[] UCG_FONT_SECTION("ucg_font_courB10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB12_mf[] UCG_FONT_SECTION("ucg_font_courB12_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB12_mr[] UCG_FONT_SECTION("ucg_font_courB12_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB12_tf[] UCG_FONT_SECTION("ucg_font_courB12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB12_tr[] UCG_FONT_SECTION("ucg_font_courB12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB14_mf[] UCG_FONT_SECTION("ucg_font_courB14_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB14_mr[] UCG_FONT_SECTION("ucg_font_courB14_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB14_tf[] UCG_FONT_SECTION("ucg_font_courB14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB14_tr[] UCG_FONT_SECTION("ucg_font_courB14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_mf[] UCG_FONT_SECTION("ucg_font_courB18_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_mn[] UCG_FONT_SECTION("ucg_font_courB18_mn");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_mr[] UCG_FONT_SECTION("ucg_font_courB18_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_tf[] UCG_FONT_SECTION("ucg_font_courB18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_tn[] UCG_FONT_SECTION("ucg_font_courB18_tn");
extern const ucg_fntpgm_uint8_t ucg_font_courB18_tr[] UCG_FONT_SECTION("ucg_font_courB18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_mf[] UCG_FONT_SECTION("ucg_font_courB24_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_mn[] UCG_FONT_SECTION("ucg_font_courB24_mn");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_mr[] UCG_FONT_SECTION("ucg_font_courB24_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_tf[] UCG_FONT_SECTION("ucg_font_courB24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_tn[] UCG_FONT_SECTION("ucg_font_courB24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_courB24_tr[] UCG_FONT_SECTION("ucg_font_courB24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR08_mf[] UCG_FONT_SECTION("ucg_font_courR08_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR08_mr[] UCG_FONT_SECTION("ucg_font_courR08_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR08_tf[] UCG_FONT_SECTION("ucg_font_courR08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR08_tr[] UCG_FONT_SECTION("ucg_font_courR08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR10_mf[] UCG_FONT_SECTION("ucg_font_courR10_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR10_mr[] UCG_FONT_SECTION("ucg_font_courR10_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR10_tf[] UCG_FONT_SECTION("ucg_font_courR10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR10_tr[] UCG_FONT_SECTION("ucg_font_courR10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR12_mf[] UCG_FONT_SECTION("ucg_font_courR12_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR12_mr[] UCG_FONT_SECTION("ucg_font_courR12_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR12_tf[] UCG_FONT_SECTION("ucg_font_courR12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR12_tr[] UCG_FONT_SECTION("ucg_font_courR12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR14_mf[] UCG_FONT_SECTION("ucg_font_courR14_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR14_mr[] UCG_FONT_SECTION("ucg_font_courR14_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR14_tf[] UCG_FONT_SECTION("ucg_font_courR14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR14_tr[] UCG_FONT_SECTION("ucg_font_courR14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR18_mf[] UCG_FONT_SECTION("ucg_font_courR18_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR18_mr[] UCG_FONT_SECTION("ucg_font_courR18_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR18_tf[] UCG_FONT_SECTION("ucg_font_courR18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR18_tr[] UCG_FONT_SECTION("ucg_font_courR18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_mf[] UCG_FONT_SECTION("ucg_font_courR24_mf");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_mn[] UCG_FONT_SECTION("ucg_font_courR24_mn");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_mr[] UCG_FONT_SECTION("ucg_font_courR24_mr");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_tf[] UCG_FONT_SECTION("ucg_font_courR24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_tn[] UCG_FONT_SECTION("ucg_font_courR24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_courR24_tr[] UCG_FONT_SECTION("ucg_font_courR24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_67_75[] UCG_FONT_SECTION("ucg_font_cu12_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_75[] UCG_FONT_SECTION("ucg_font_cu12_75");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_hf[] UCG_FONT_SECTION("ucg_font_cu12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_mf[] UCG_FONT_SECTION("ucg_font_cu12_mf");
extern const ucg_fntpgm_uint8_t ucg_font_cu12_tf[] UCG_FONT_SECTION("ucg_font_cu12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_cursor_tf[] UCG_FONT_SECTION("ucg_font_cursor_tf");
extern const ucg_fntpgm_uint8_t ucg_font_cursor_tr[] UCG_FONT_SECTION("ucg_font_cursor_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_hf[] UCG_FONT_SECTION("ucg_font_fixed_v0_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_hn[] UCG_FONT_SECTION("ucg_font_fixed_v0_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_hr[] UCG_FONT_SECTION("ucg_font_fixed_v0_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_mr[] UCG_FONT_SECTION("ucg_font_fixed_v0_mr");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_tf[] UCG_FONT_SECTION("ucg_font_fixed_v0_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_tn[] UCG_FONT_SECTION("ucg_font_fixed_v0_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fixed_v0_tr[] UCG_FONT_SECTION("ucg_font_fixed_v0_tr");
extern const ucg_fntpgm_uint8_t ucg_font_freedoomr10_tr[] UCG_FONT_SECTION("ucg_font_freedoomr10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_freedoomr25_tn[] UCG_FONT_SECTION("ucg_font_freedoomr25_tn");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08_hf[] UCG_FONT_SECTION("ucg_font_helvB08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08_hr[] UCG_FONT_SECTION("ucg_font_helvB08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08_tf[] UCG_FONT_SECTION("ucg_font_helvB08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB08_tr[] UCG_FONT_SECTION("ucg_font_helvB08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10_hf[] UCG_FONT_SECTION("ucg_font_helvB10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10_hr[] UCG_FONT_SECTION("ucg_font_helvB10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10_tf[] UCG_FONT_SECTION("ucg_font_helvB10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB10_tr[] UCG_FONT_SECTION("ucg_font_helvB10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12_hf[] UCG_FONT_SECTION("ucg_font_helvB12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12_hr[] UCG_FONT_SECTION("ucg_font_helvB12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12_tf[] UCG_FONT_SECTION("ucg_font_helvB12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB12_tr[] UCG_FONT_SECTION("ucg_font_helvB12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14_hf[] UCG_FONT_SECTION("ucg_font_helvB14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14_hr[] UCG_FONT_SECTION("ucg_font_helvB14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14_tf[] UCG_FONT_SECTION("ucg_font_helvB14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB14_tr[] UCG_FONT_SECTION("ucg_font_helvB14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18_hf[] UCG_FONT_SECTION("ucg_font_helvB18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18_hr[] UCG_FONT_SECTION("ucg_font_helvB18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18_tf[] UCG_FONT_SECTION("ucg_font_helvB18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB18_tr[] UCG_FONT_SECTION("ucg_font_helvB18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_hf[] UCG_FONT_SECTION("ucg_font_helvB24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_hn[] UCG_FONT_SECTION("ucg_font_helvB24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_hr[] UCG_FONT_SECTION("ucg_font_helvB24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_tf[] UCG_FONT_SECTION("ucg_font_helvB24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_tn[] UCG_FONT_SECTION("ucg_font_helvB24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_helvB24_tr[] UCG_FONT_SECTION("ucg_font_helvB24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08_hf[] UCG_FONT_SECTION("ucg_font_helvR08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08_hr[] UCG_FONT_SECTION("ucg_font_helvR08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08_tf[] UCG_FONT_SECTION("ucg_font_helvR08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR08_tr[] UCG_FONT_SECTION("ucg_font_helvR08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10_hf[] UCG_FONT_SECTION("ucg_font_helvR10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10_hr[] UCG_FONT_SECTION("ucg_font_helvR10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10_tf[] UCG_FONT_SECTION("ucg_font_helvR10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR10_tr[] UCG_FONT_SECTION("ucg_font_helvR10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12_hf[] UCG_FONT_SECTION("ucg_font_helvR12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12_hr[] UCG_FONT_SECTION("ucg_font_helvR12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12_tf[] UCG_FONT_SECTION("ucg_font_helvR12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR12_tr[] UCG_FONT_SECTION("ucg_font_helvR12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14_hf[] UCG_FONT_SECTION("ucg_font_helvR14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14_hr[] UCG_FONT_SECTION("ucg_font_helvR14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14_tf[] UCG_FONT_SECTION("ucg_font_helvR14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR14_tr[] UCG_FONT_SECTION("ucg_font_helvR14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18_hf[] UCG_FONT_SECTION("ucg_font_helvR18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18_hr[] UCG_FONT_SECTION("ucg_font_helvR18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18_tf[] UCG_FONT_SECTION("ucg_font_helvR18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR18_tr[] UCG_FONT_SECTION("ucg_font_helvR18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_hf[] UCG_FONT_SECTION("ucg_font_helvR24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_hn[] UCG_FONT_SECTION("ucg_font_helvR24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_hr[] UCG_FONT_SECTION("ucg_font_helvR24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_tf[] UCG_FONT_SECTION("ucg_font_helvR24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_tn[] UCG_FONT_SECTION("ucg_font_helvR24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_helvR24_tr[] UCG_FONT_SECTION("ucg_font_helvR24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_hf[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_hf");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_hn[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_hn");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_hr[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_hr");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_tf[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_tf");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_tn[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_tn");
extern const ucg_fntpgm_uint8_t ucg_font_lucasfont_alternate_tr[] UCG_FONT_SECTION("ucg_font_lucasfont_alternate_tr");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_5[] UCG_FONT_SECTION("ucg_font_m2icon_5");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_7[] UCG_FONT_SECTION("ucg_font_m2icon_7");
extern const ucg_fntpgm_uint8_t ucg_font_m2icon_9[] UCG_FONT_SECTION("ucg_font_m2icon_9");
extern const ucg_fntpgm_uint8_t ucg_font_micro_mf[] UCG_FONT_SECTION("ucg_font_micro_mf");
extern const ucg_fntpgm_uint8_t ucg_font_micro_tf[] UCG_FONT_SECTION("ucg_font_micro_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08_hf[] UCG_FONT_SECTION("ucg_font_ncenB08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08_hr[] UCG_FONT_SECTION("ucg_font_ncenB08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08_tf[] UCG_FONT_SECTION("ucg_font_ncenB08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB08_tr[] UCG_FONT_SECTION("ucg_font_ncenB08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10_hf[] UCG_FONT_SECTION("ucg_font_ncenB10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10_hr[] UCG_FONT_SECTION("ucg_font_ncenB10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10_tf[] UCG_FONT_SECTION("ucg_font_ncenB10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB10_tr[] UCG_FONT_SECTION("ucg_font_ncenB10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12_hf[] UCG_FONT_SECTION("ucg_font_ncenB12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12_hr[] UCG_FONT_SECTION("ucg_font_ncenB12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12_tf[] UCG_FONT_SECTION("ucg_font_ncenB12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB12_tr[] UCG_FONT_SECTION("ucg_font_ncenB12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14_hf[] UCG_FONT_SECTION("ucg_font_ncenB14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14_hr[] UCG_FONT_SECTION("ucg_font_ncenB14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14_tf[] UCG_FONT_SECTION("ucg_font_ncenB14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB14_tr[] UCG_FONT_SECTION("ucg_font_ncenB14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18_hf[] UCG_FONT_SECTION("ucg_font_ncenB18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18_hr[] UCG_FONT_SECTION("ucg_font_ncenB18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18_tf[] UCG_FONT_SECTION("ucg_font_ncenB18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB18_tr[] UCG_FONT_SECTION("ucg_font_ncenB18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_hf[] UCG_FONT_SECTION("ucg_font_ncenB24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_hn[] UCG_FONT_SECTION("ucg_font_ncenB24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_hr[] UCG_FONT_SECTION("ucg_font_ncenB24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_tf[] UCG_FONT_SECTION("ucg_font_ncenB24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_tn[] UCG_FONT_SECTION("ucg_font_ncenB24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_ncenB24_tr[] UCG_FONT_SECTION("ucg_font_ncenB24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08_hf[] UCG_FONT_SECTION("ucg_font_ncenR08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08_hr[] UCG_FONT_SECTION("ucg_font_ncenR08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08_tf[] UCG_FONT_SECTION("ucg_font_ncenR08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR08_tr[] UCG_FONT_SECTION("ucg_font_ncenR08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10_hf[] UCG_FONT_SECTION("ucg_font_ncenR10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10_hr[] UCG_FONT_SECTION("ucg_font_ncenR10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10_tf[] UCG_FONT_SECTION("ucg_font_ncenR10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR10_tr[] UCG_FONT_SECTION("ucg_font_ncenR10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12_hf[] UCG_FONT_SECTION("ucg_font_ncenR12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12_hr[] UCG_FONT_SECTION("ucg_font_ncenR12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12_tf[] UCG_FONT_SECTION("ucg_font_ncenR12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR12_tr[] UCG_FONT_SECTION("ucg_font_ncenR12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14_hf[] UCG_FONT_SECTION("ucg_font_ncenR14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14_hr[] UCG_FONT_SECTION("ucg_font_ncenR14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14_tf[] UCG_FONT_SECTION("ucg_font_ncenR14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR14_tr[] UCG_FONT_SECTION("ucg_font_ncenR14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18_hf[] UCG_FONT_SECTION("ucg_font_ncenR18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18_hr[] UCG_FONT_SECTION("ucg_font_ncenR18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18_tf[] UCG_FONT_SECTION("ucg_font_ncenR18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR18_tr[] UCG_FONT_SECTION("ucg_font_ncenR18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_hf[] UCG_FONT_SECTION("ucg_font_ncenR24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_hn[] UCG_FONT_SECTION("ucg_font_ncenR24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_hr[] UCG_FONT_SECTION("ucg_font_ncenR24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_tf[] UCG_FONT_SECTION("ucg_font_ncenR24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_tn[] UCG_FONT_SECTION("ucg_font_ncenR24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_ncenR24_tr[] UCG_FONT_SECTION("ucg_font_ncenR24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_hf[] UCG_FONT_SECTION("ucg_font_orgv01_hf");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_hn[] UCG_FONT_SECTION("ucg_font_orgv01_hn");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_hr[] UCG_FONT_SECTION("ucg_font_orgv01_hr");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_tf[] UCG_FONT_SECTION("ucg_font_orgv01_tf");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_tn[] UCG_FONT_SECTION("ucg_font_orgv01_tn");
extern const ucg_fntpgm_uint8_t ucg_font_orgv01_tr[] UCG_FONT_SECTION("ucg_font_orgv01_tr");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_hf[] UCG_FONT_SECTION("ucg_font_p01type_hf");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_hn[] UCG_FONT_SECTION("ucg_font_p01type_hn");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_hr[] UCG_FONT_SECTION("ucg_font_p01type_hr");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_tf[] UCG_FONT_SECTION("ucg_font_p01type_tf");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_tn[] UCG_FONT_SECTION("ucg_font_p01type_tn");
extern const ucg_fntpgm_uint8_t ucg_font_p01type_tr[] UCG_FONT_SECTION("ucg_font_p01type_tr");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_hf[] UCG_FONT_SECTION("ucg_font_pixelle_micro_hf");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_hn[] UCG_FONT_SECTION("ucg_font_pixelle_micro_hn");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_hr[] UCG_FONT_SECTION("ucg_font_pixelle_micro_hr");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_tf[] UCG_FONT_SECTION("ucg_font_pixelle_micro_tf");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_tn[] UCG_FONT_SECTION("ucg_font_pixelle_micro_tn");
extern const ucg_fntpgm_uint8_t ucg_font_pixelle_micro_tr[] UCG_FONT_SECTION("ucg_font_pixelle_micro_tr");
extern const ucg_fntpgm_uint8_t ucg_font_profont10_8f[] UCG_FONT_SECTION("ucg_font_profont10_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont10_8r[] UCG_FONT_SECTION("ucg_font_profont10_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont10_mf[] UCG_FONT_SECTION("ucg_font_profont10_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont10_mr[] UCG_FONT_SECTION("ucg_font_profont10_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont11_8f[] UCG_FONT_SECTION("ucg_font_profont11_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont11_8r[] UCG_FONT_SECTION("ucg_font_profont11_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont11_mf[] UCG_FONT_SECTION("ucg_font_profont11_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont11_mr[] UCG_FONT_SECTION("ucg_font_profont11_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont12_8f[] UCG_FONT_SECTION("ucg_font_profont12_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont12_8r[] UCG_FONT_SECTION("ucg_font_profont12_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont12_mf[] UCG_FONT_SECTION("ucg_font_profont12_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont12_mr[] UCG_FONT_SECTION("ucg_font_profont12_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont15_8f[] UCG_FONT_SECTION("ucg_font_profont15_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont15_8r[] UCG_FONT_SECTION("ucg_font_profont15_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont15_mf[] UCG_FONT_SECTION("ucg_font_profont15_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont15_mr[] UCG_FONT_SECTION("ucg_font_profont15_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont17_8f[] UCG_FONT_SECTION("ucg_font_profont17_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont17_8r[] UCG_FONT_SECTION("ucg_font_profont17_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont17_mf[] UCG_FONT_SECTION("ucg_font_profont17_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont17_mr[] UCG_FONT_SECTION("ucg_font_profont17_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_8f[] UCG_FONT_SECTION("ucg_font_profont22_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_8n[] UCG_FONT_SECTION("ucg_font_profont22_8n");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_8r[] UCG_FONT_SECTION("ucg_font_profont22_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_mf[] UCG_FONT_SECTION("ucg_font_profont22_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_mn[] UCG_FONT_SECTION("ucg_font_profont22_mn");
extern const ucg_fntpgm_uint8_t ucg_font_profont22_mr[] UCG_FONT_SECTION("ucg_font_profont22_mr");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_8f[] UCG_FONT_SECTION("ucg_font_profont29_8f");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_8n[] UCG_FONT_SECTION("ucg_font_profont29_8n");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_8r[] UCG_FONT_SECTION("ucg_font_profont29_8r");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_mf[] UCG_FONT_SECTION("ucg_font_profont29_mf");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_mn[] UCG_FONT_SECTION("ucg_font_profont29_mn");
extern const ucg_fntpgm_uint8_t ucg_font_profont29_mr[] UCG_FONT_SECTION("ucg_font_profont29_mr");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_hf[] UCG_FONT_SECTION("ucg_font_robot_de_niro_hf");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_hn[] UCG_FONT_SECTION("ucg_font_robot_de_niro_hn");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_hr[] UCG_FONT_SECTION("ucg_font_robot_de_niro_hr");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_tf[] UCG_FONT_SECTION("ucg_font_robot_de_niro_tf");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_tn[] UCG_FONT_SECTION("ucg_font_robot_de_niro_tn");
extern const ucg_fntpgm_uint8_t ucg_font_robot_de_niro_tr[] UCG_FONT_SECTION("ucg_font_robot_de_niro_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb08_tf[] UCG_FONT_SECTION("ucg_font_symb08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb08_tr[] UCG_FONT_SECTION("ucg_font_symb08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb10_tf[] UCG_FONT_SECTION("ucg_font_symb10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb10_tr[] UCG_FONT_SECTION("ucg_font_symb10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb12_tf[] UCG_FONT_SECTION("ucg_font_symb12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb12_tr[] UCG_FONT_SECTION("ucg_font_symb12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb14_tf[] UCG_FONT_SECTION("ucg_font_symb14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb14_tr[] UCG_FONT_SECTION("ucg_font_symb14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb18_tf[] UCG_FONT_SECTION("ucg_font_symb18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb18_tr[] UCG_FONT_SECTION("ucg_font_symb18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_symb24_tf[] UCG_FONT_SECTION("ucg_font_symb24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_symb24_tr[] UCG_FONT_SECTION("ucg_font_symb24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB08_hf[] UCG_FONT_SECTION("ucg_font_timB08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB08_hr[] UCG_FONT_SECTION("ucg_font_timB08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB08_tf[] UCG_FONT_SECTION("ucg_font_timB08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB08_tr[] UCG_FONT_SECTION("ucg_font_timB08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB10_hf[] UCG_FONT_SECTION("ucg_font_timB10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB10_hr[] UCG_FONT_SECTION("ucg_font_timB10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB10_tf[] UCG_FONT_SECTION("ucg_font_timB10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB10_tr[] UCG_FONT_SECTION("ucg_font_timB10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB12_hf[] UCG_FONT_SECTION("ucg_font_timB12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB12_hr[] UCG_FONT_SECTION("ucg_font_timB12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB12_tf[] UCG_FONT_SECTION("ucg_font_timB12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB12_tr[] UCG_FONT_SECTION("ucg_font_timB12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB14_hf[] UCG_FONT_SECTION("ucg_font_timB14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB14_hr[] UCG_FONT_SECTION("ucg_font_timB14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB14_tf[] UCG_FONT_SECTION("ucg_font_timB14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB14_tr[] UCG_FONT_SECTION("ucg_font_timB14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB18_hf[] UCG_FONT_SECTION("ucg_font_timB18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB18_hr[] UCG_FONT_SECTION("ucg_font_timB18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB18_tf[] UCG_FONT_SECTION("ucg_font_timB18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB18_tr[] UCG_FONT_SECTION("ucg_font_timB18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_hf[] UCG_FONT_SECTION("ucg_font_timB24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_hn[] UCG_FONT_SECTION("ucg_font_timB24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_hr[] UCG_FONT_SECTION("ucg_font_timB24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_tf[] UCG_FONT_SECTION("ucg_font_timB24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_tn[] UCG_FONT_SECTION("ucg_font_timB24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_timB24_tr[] UCG_FONT_SECTION("ucg_font_timB24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR08_hf[] UCG_FONT_SECTION("ucg_font_timR08_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR08_hr[] UCG_FONT_SECTION("ucg_font_timR08_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR08_tf[] UCG_FONT_SECTION("ucg_font_timR08_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR08_tr[] UCG_FONT_SECTION("ucg_font_timR08_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR10_hf[] UCG_FONT_SECTION("ucg_font_timR10_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR10_hr[] UCG_FONT_SECTION("ucg_font_timR10_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR10_tf[] UCG_FONT_SECTION("ucg_font_timR10_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR10_tr[] UCG_FONT_SECTION("ucg_font_timR10_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR12_hf[] UCG_FONT_SECTION("ucg_font_timR12_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR12_hr[] UCG_FONT_SECTION("ucg_font_timR12_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR12_tf[] UCG_FONT_SECTION("ucg_font_timR12_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR12_tr[] UCG_FONT_SECTION("ucg_font_timR12_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR14_hf[] UCG_FONT_SECTION("ucg_font_timR14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR14_hr[] UCG_FONT_SECTION("ucg_font_timR14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR14_tf[] UCG_FONT_SECTION("ucg_font_timR14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR14_tr[] UCG_FONT_SECTION("ucg_font_timR14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR18_hf[] UCG_FONT_SECTION("ucg_font_timR18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR18_hr[] UCG_FONT_SECTION("ucg_font_timR18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR18_tf[] UCG_FONT_SECTION("ucg_font_timR18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR18_tr[] UCG_FONT_SECTION("ucg_font_timR18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_hf[] UCG_FONT_SECTION("ucg_font_timR24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_hn[] UCG_FONT_SECTION("ucg_font_timR24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_hr[] UCG_FONT_SECTION("ucg_font_timR24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_tf[] UCG_FONT_SECTION("ucg_font_timR24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_tn[] UCG_FONT_SECTION("ucg_font_timR24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_timR24_tr[] UCG_FONT_SECTION("ucg_font_timR24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_hf[] UCG_FONT_SECTION("ucg_font_tpssb_hf");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_hn[] UCG_FONT_SECTION("ucg_font_tpssb_hn");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_hr[] UCG_FONT_SECTION("ucg_font_tpssb_hr");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_tf[] UCG_FONT_SECTION("ucg_font_tpssb_tf");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_tn[] UCG_FONT_SECTION("ucg_font_tpssb_tn");
extern const ucg_fntpgm_uint8_t ucg_font_tpssb_tr[] UCG_FONT_SECTION("ucg_font_tpssb_tr");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_hf[] UCG_FONT_SECTION("ucg_font_tpss_hf");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_hn[] UCG_FONT_SECTION("ucg_font_tpss_hn");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_hr[] UCG_FONT_SECTION("ucg_font_tpss_hr");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_tf[] UCG_FONT_SECTION("ucg_font_tpss_tf");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_tn[] UCG_FONT_SECTION("ucg_font_tpss_tn");
extern const ucg_fntpgm_uint8_t ucg_font_tpss_tr[] UCG_FONT_SECTION("ucg_font_tpss_tr");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_hf[] UCG_FONT_SECTION("ucg_font_trixel_square_hf");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_hn[] UCG_FONT_SECTION("ucg_font_trixel_square_hn");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_hr[] UCG_FONT_SECTION("ucg_font_trixel_square_hr");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_tf[] UCG_FONT_SECTION("ucg_font_trixel_square_tf");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_tn[] UCG_FONT_SECTION("ucg_font_trixel_square_tn");
extern const ucg_fntpgm_uint8_t ucg_font_trixel_square_tr[] UCG_FONT_SECTION("ucg_font_trixel_square_tr");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4_hf[] UCG_FONT_SECTION("ucg_font_u8glib_4_hf");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4_hr[] UCG_FONT_SECTION("ucg_font_u8glib_4_hr");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4_tf[] UCG_FONT_SECTION("ucg_font_u8glib_4_tf");
extern const ucg_fntpgm_uint8_t ucg_font_u8glib_4_tr[] UCG_FONT_SECTION("ucg_font_u8glib_4_tr");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_0_8[] UCG_FONT_SECTION("ucg_font_unifont_0_8");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_12_13[] UCG_FONT_SECTION("ucg_font_unifont_12_13");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_18_19[] UCG_FONT_SECTION("ucg_font_unifont_18_19");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_2_3[] UCG_FONT_SECTION("ucg_font_unifont_2_3");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_4_5[] UCG_FONT_SECTION("ucg_font_unifont_4_5");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_67_75[] UCG_FONT_SECTION("ucg_font_unifont_67_75");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_72_73[] UCG_FONT_SECTION("ucg_font_unifont_72_73");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_78_79[] UCG_FONT_SECTION("ucg_font_unifont_78_79");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_8_9[] UCG_FONT_SECTION("ucg_font_unifont_8_9");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_mf[] UCG_FONT_SECTION("ucg_font_unifont_mf");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_mr[] UCG_FONT_SECTION("ucg_font_unifont_mr");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_tf[] UCG_FONT_SECTION("ucg_font_unifont_tf");
extern const ucg_fntpgm_uint8_t ucg_font_unifont_tr[] UCG_FONT_SECTION("ucg_font_unifont_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_hf[] UCG_FONT_SECTION("ucg_font_fub11_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_hn[] UCG_FONT_SECTION("ucg_font_fub11_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_hr[] UCG_FONT_SECTION("ucg_font_fub11_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_tf[] UCG_FONT_SECTION("ucg_font_fub11_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_tn[] UCG_FONT_SECTION("ucg_font_fub11_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub11_tr[] UCG_FONT_SECTION("ucg_font_fub11_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_hf[] UCG_FONT_SECTION("ucg_font_fub14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_hn[] UCG_FONT_SECTION("ucg_font_fub14_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_hr[] UCG_FONT_SECTION("ucg_font_fub14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_tf[] UCG_FONT_SECTION("ucg_font_fub14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_tn[] UCG_FONT_SECTION("ucg_font_fub14_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub14_tr[] UCG_FONT_SECTION("ucg_font_fub14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_hf[] UCG_FONT_SECTION("ucg_font_fub17_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_hn[] UCG_FONT_SECTION("ucg_font_fub17_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_hr[] UCG_FONT_SECTION("ucg_font_fub17_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_tf[] UCG_FONT_SECTION("ucg_font_fub17_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_tn[] UCG_FONT_SECTION("ucg_font_fub17_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub17_tr[] UCG_FONT_SECTION("ucg_font_fub17_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_hf[] UCG_FONT_SECTION("ucg_font_fub20_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_hn[] UCG_FONT_SECTION("ucg_font_fub20_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_hr[] UCG_FONT_SECTION("ucg_font_fub20_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_tf[] UCG_FONT_SECTION("ucg_font_fub20_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_tn[] UCG_FONT_SECTION("ucg_font_fub20_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub20_tr[] UCG_FONT_SECTION("ucg_font_fub20_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_hf[] UCG_FONT_SECTION("ucg_font_fub25_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_hn[] UCG_FONT_SECTION("ucg_font_fub25_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_hr[] UCG_FONT_SECTION("ucg_font_fub25_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_tf[] UCG_FONT_SECTION("ucg_font_fub25_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_tn[] UCG_FONT_SECTION("ucg_font_fub25_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub25_tr[] UCG_FONT_SECTION("ucg_font_fub25_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_hf[] UCG_FONT_SECTION("ucg_font_fub30_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_hn[] UCG_FONT_SECTION("ucg_font_fub30_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_hr[] UCG_FONT_SECTION("ucg_font_fub30_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_tf[] UCG_FONT_SECTION("ucg_font_fub30_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_tn[] UCG_FONT_SECTION("ucg_font_fub30_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub30_tr[] UCG_FONT_SECTION("ucg_font_fub30_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_hf[] UCG_FONT_SECTION("ucg_font_fub35_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_hn[] UCG_FONT_SECTION("ucg_font_fub35_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_hr[] UCG_FONT_SECTION("ucg_font_fub35_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_tf[] UCG_FONT_SECTION("ucg_font_fub35_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_tn[] UCG_FONT_SECTION("ucg_font_fub35_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub35_tr[] UCG_FONT_SECTION("ucg_font_fub35_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_hf[] UCG_FONT_SECTION("ucg_font_fub42_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_hn[] UCG_FONT_SECTION("ucg_font_fub42_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_hr[] UCG_FONT_SECTION("ucg_font_fub42_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_tf[] UCG_FONT_SECTION("ucg_font_fub42_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_tn[] UCG_FONT_SECTION("ucg_font_fub42_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fub42_tr[] UCG_FONT_SECTION("ucg_font_fub42_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fub49_hn[] UCG_FONT_SECTION("ucg_font_fub49_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fub49_tn[] UCG_FONT_SECTION("ucg_font_fub49_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_hf[] UCG_FONT_SECTION("ucg_font_fur11_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_hn[] UCG_FONT_SECTION("ucg_font_fur11_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_hr[] UCG_FONT_SECTION("ucg_font_fur11_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_tf[] UCG_FONT_SECTION("ucg_font_fur11_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_tn[] UCG_FONT_SECTION("ucg_font_fur11_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur11_tr[] UCG_FONT_SECTION("ucg_font_fur11_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_hf[] UCG_FONT_SECTION("ucg_font_fur14_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_hn[] UCG_FONT_SECTION("ucg_font_fur14_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_hr[] UCG_FONT_SECTION("ucg_font_fur14_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_tf[] UCG_FONT_SECTION("ucg_font_fur14_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_tn[] UCG_FONT_SECTION("ucg_font_fur14_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur14_tr[] UCG_FONT_SECTION("ucg_font_fur14_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_hf[] UCG_FONT_SECTION("ucg_font_fur17_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_hn[] UCG_FONT_SECTION("ucg_font_fur17_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_hr[] UCG_FONT_SECTION("ucg_font_fur17_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_tf[] UCG_FONT_SECTION("ucg_font_fur17_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_tn[] UCG_FONT_SECTION("ucg_font_fur17_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur17_tr[] UCG_FONT_SECTION("ucg_font_fur17_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_hf[] UCG_FONT_SECTION("ucg_font_fur20_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_hn[] UCG_FONT_SECTION("ucg_font_fur20_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_hr[] UCG_FONT_SECTION("ucg_font_fur20_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_tf[] UCG_FONT_SECTION("ucg_font_fur20_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_tn[] UCG_FONT_SECTION("ucg_font_fur20_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur20_tr[] UCG_FONT_SECTION("ucg_font_fur20_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_hf[] UCG_FONT_SECTION("ucg_font_fur25_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_hn[] UCG_FONT_SECTION("ucg_font_fur25_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_hr[] UCG_FONT_SECTION("ucg_font_fur25_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_tf[] UCG_FONT_SECTION("ucg_font_fur25_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_tn[] UCG_FONT_SECTION("ucg_font_fur25_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur25_tr[] UCG_FONT_SECTION("ucg_font_fur25_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_hf[] UCG_FONT_SECTION("ucg_font_fur30_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_hn[] UCG_FONT_SECTION("ucg_font_fur30_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_hr[] UCG_FONT_SECTION("ucg_font_fur30_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_tf[] UCG_FONT_SECTION("ucg_font_fur30_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_tn[] UCG_FONT_SECTION("ucg_font_fur30_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur30_tr[] UCG_FONT_SECTION("ucg_font_fur30_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_hf[] UCG_FONT_SECTION("ucg_font_fur35_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_hn[] UCG_FONT_SECTION("ucg_font_fur35_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_hr[] UCG_FONT_SECTION("ucg_font_fur35_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_tf[] UCG_FONT_SECTION("ucg_font_fur35_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_tn[] UCG_FONT_SECTION("ucg_font_fur35_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur35_tr[] UCG_FONT_SECTION("ucg_font_fur35_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_hf[] UCG_FONT_SECTION("ucg_font_fur42_hf");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_hn[] UCG_FONT_SECTION("ucg_font_fur42_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_hr[] UCG_FONT_SECTION("ucg_font_fur42_hr");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_tf[] UCG_FONT_SECTION("ucg_font_fur42_tf");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_tn[] UCG_FONT_SECTION("ucg_font_fur42_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur42_tr[] UCG_FONT_SECTION("ucg_font_fur42_tr");
extern const ucg_fntpgm_uint8_t ucg_font_fur49_hn[] UCG_FONT_SECTION("ucg_font_fur49_hn");
extern const ucg_fntpgm_uint8_t ucg_font_fur49_tn[] UCG_FONT_SECTION("ucg_font_fur49_tn");
extern const ucg_fntpgm_uint8_t ucg_font_fur49_tr[] UCG_FONT_SECTION("ucg_font_fur49_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_mf[] UCG_FONT_SECTION("ucg_font_inb16_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_mn[] UCG_FONT_SECTION("ucg_font_inb16_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_mr[] UCG_FONT_SECTION("ucg_font_inb16_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_tf[] UCG_FONT_SECTION("ucg_font_inb16_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_tn[] UCG_FONT_SECTION("ucg_font_inb16_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb16_tr[] UCG_FONT_SECTION("ucg_font_inb16_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_mf[] UCG_FONT_SECTION("ucg_font_inb19_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_mn[] UCG_FONT_SECTION("ucg_font_inb19_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_mr[] UCG_FONT_SECTION("ucg_font_inb19_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_tf[] UCG_FONT_SECTION("ucg_font_inb19_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_tn[] UCG_FONT_SECTION("ucg_font_inb19_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb19_tr[] UCG_FONT_SECTION("ucg_font_inb19_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_mf[] UCG_FONT_SECTION("ucg_font_inb21_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_mn[] UCG_FONT_SECTION("ucg_font_inb21_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_mr[] UCG_FONT_SECTION("ucg_font_inb21_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_tf[] UCG_FONT_SECTION("ucg_font_inb21_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_tn[] UCG_FONT_SECTION("ucg_font_inb21_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb21_tr[] UCG_FONT_SECTION("ucg_font_inb21_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_mf[] UCG_FONT_SECTION("ucg_font_inb24_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_mn[] UCG_FONT_SECTION("ucg_font_inb24_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_mr[] UCG_FONT_SECTION("ucg_font_inb24_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_tf[] UCG_FONT_SECTION("ucg_font_inb24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_tn[] UCG_FONT_SECTION("ucg_font_inb24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb24_tr[] UCG_FONT_SECTION("ucg_font_inb24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_mf[] UCG_FONT_SECTION("ucg_font_inb27_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_mn[] UCG_FONT_SECTION("ucg_font_inb27_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_mr[] UCG_FONT_SECTION("ucg_font_inb27_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_tf[] UCG_FONT_SECTION("ucg_font_inb27_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_tn[] UCG_FONT_SECTION("ucg_font_inb27_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb27_tr[] UCG_FONT_SECTION("ucg_font_inb27_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_mf[] UCG_FONT_SECTION("ucg_font_inb30_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_mn[] UCG_FONT_SECTION("ucg_font_inb30_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_mr[] UCG_FONT_SECTION("ucg_font_inb30_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_tf[] UCG_FONT_SECTION("ucg_font_inb30_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_tn[] UCG_FONT_SECTION("ucg_font_inb30_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb30_tr[] UCG_FONT_SECTION("ucg_font_inb30_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_mf[] UCG_FONT_SECTION("ucg_font_inb33_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_mn[] UCG_FONT_SECTION("ucg_font_inb33_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_mr[] UCG_FONT_SECTION("ucg_font_inb33_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_tf[] UCG_FONT_SECTION("ucg_font_inb33_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_tn[] UCG_FONT_SECTION("ucg_font_inb33_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb33_tr[] UCG_FONT_SECTION("ucg_font_inb33_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_mf[] UCG_FONT_SECTION("ucg_font_inb38_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_mn[] UCG_FONT_SECTION("ucg_font_inb38_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_mr[] UCG_FONT_SECTION("ucg_font_inb38_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_tf[] UCG_FONT_SECTION("ucg_font_inb38_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_tn[] UCG_FONT_SECTION("ucg_font_inb38_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb38_tr[] UCG_FONT_SECTION("ucg_font_inb38_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_mf[] UCG_FONT_SECTION("ucg_font_inb42_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_mn[] UCG_FONT_SECTION("ucg_font_inb42_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_mr[] UCG_FONT_SECTION("ucg_font_inb42_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_tf[] UCG_FONT_SECTION("ucg_font_inb42_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_tn[] UCG_FONT_SECTION("ucg_font_inb42_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb42_tr[] UCG_FONT_SECTION("ucg_font_inb42_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_mf[] UCG_FONT_SECTION("ucg_font_inb46_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_mn[] UCG_FONT_SECTION("ucg_font_inb46_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_mr[] UCG_FONT_SECTION("ucg_font_inb46_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_tf[] UCG_FONT_SECTION("ucg_font_inb46_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_tn[] UCG_FONT_SECTION("ucg_font_inb46_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb46_tr[] UCG_FONT_SECTION("ucg_font_inb46_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_mf[] UCG_FONT_SECTION("ucg_font_inb49_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_mn[] UCG_FONT_SECTION("ucg_font_inb49_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_mr[] UCG_FONT_SECTION("ucg_font_inb49_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_tf[] UCG_FONT_SECTION("ucg_font_inb49_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_tn[] UCG_FONT_SECTION("ucg_font_inb49_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb49_tr[] UCG_FONT_SECTION("ucg_font_inb49_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_mf[] UCG_FONT_SECTION("ucg_font_inb53_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_mn[] UCG_FONT_SECTION("ucg_font_inb53_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_mr[] UCG_FONT_SECTION("ucg_font_inb53_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_tf[] UCG_FONT_SECTION("ucg_font_inb53_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_tn[] UCG_FONT_SECTION("ucg_font_inb53_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb53_tr[] UCG_FONT_SECTION("ucg_font_inb53_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_mf[] UCG_FONT_SECTION("ucg_font_inb57_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_mn[] UCG_FONT_SECTION("ucg_font_inb57_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_mr[] UCG_FONT_SECTION("ucg_font_inb57_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_tf[] UCG_FONT_SECTION("ucg_font_inb57_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_tn[] UCG_FONT_SECTION("ucg_font_inb57_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb57_tr[] UCG_FONT_SECTION("ucg_font_inb57_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inb63_mn[] UCG_FONT_SECTION("ucg_font_inb63_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inb63_tf[] UCG_FONT_SECTION("ucg_font_inb63_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inb63_tn[] UCG_FONT_SECTION("ucg_font_inb63_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inb63_tr[] UCG_FONT_SECTION("ucg_font_inb63_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_mf[] UCG_FONT_SECTION("ucg_font_inr16_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_mn[] UCG_FONT_SECTION("ucg_font_inr16_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_mr[] UCG_FONT_SECTION("ucg_font_inr16_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_tf[] UCG_FONT_SECTION("ucg_font_inr16_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_tn[] UCG_FONT_SECTION("ucg_font_inr16_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr16_tr[] UCG_FONT_SECTION("ucg_font_inr16_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_mf[] UCG_FONT_SECTION("ucg_font_inr19_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_mn[] UCG_FONT_SECTION("ucg_font_inr19_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_mr[] UCG_FONT_SECTION("ucg_font_inr19_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_tf[] UCG_FONT_SECTION("ucg_font_inr19_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_tn[] UCG_FONT_SECTION("ucg_font_inr19_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr19_tr[] UCG_FONT_SECTION("ucg_font_inr19_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_mf[] UCG_FONT_SECTION("ucg_font_inr21_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_mn[] UCG_FONT_SECTION("ucg_font_inr21_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_mr[] UCG_FONT_SECTION("ucg_font_inr21_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_tf[] UCG_FONT_SECTION("ucg_font_inr21_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_tn[] UCG_FONT_SECTION("ucg_font_inr21_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr21_tr[] UCG_FONT_SECTION("ucg_font_inr21_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_mf[] UCG_FONT_SECTION("ucg_font_inr24_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_mn[] UCG_FONT_SECTION("ucg_font_inr24_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_mr[] UCG_FONT_SECTION("ucg_font_inr24_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_tf[] UCG_FONT_SECTION("ucg_font_inr24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_tn[] UCG_FONT_SECTION("ucg_font_inr24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr24_tr[] UCG_FONT_SECTION("ucg_font_inr24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_mf[] UCG_FONT_SECTION("ucg_font_inr27_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_mn[] UCG_FONT_SECTION("ucg_font_inr27_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_mr[] UCG_FONT_SECTION("ucg_font_inr27_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_tf[] UCG_FONT_SECTION("ucg_font_inr27_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_tn[] UCG_FONT_SECTION("ucg_font_inr27_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr27_tr[] UCG_FONT_SECTION("ucg_font_inr27_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_mf[] UCG_FONT_SECTION("ucg_font_inr30_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_mn[] UCG_FONT_SECTION("ucg_font_inr30_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_mr[] UCG_FONT_SECTION("ucg_font_inr30_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_tf[] UCG_FONT_SECTION("ucg_font_inr30_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_tn[] UCG_FONT_SECTION("ucg_font_inr30_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr30_tr[] UCG_FONT_SECTION("ucg_font_inr30_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_mf[] UCG_FONT_SECTION("ucg_font_inr33_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_mn[] UCG_FONT_SECTION("ucg_font_inr33_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_mr[] UCG_FONT_SECTION("ucg_font_inr33_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_tf[] UCG_FONT_SECTION("ucg_font_inr33_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_tn[] UCG_FONT_SECTION("ucg_font_inr33_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr33_tr[] UCG_FONT_SECTION("ucg_font_inr33_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_mf[] UCG_FONT_SECTION("ucg_font_inr38_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_mn[] UCG_FONT_SECTION("ucg_font_inr38_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_mr[] UCG_FONT_SECTION("ucg_font_inr38_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_tf[] UCG_FONT_SECTION("ucg_font_inr38_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_tn[] UCG_FONT_SECTION("ucg_font_inr38_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr38_tr[] UCG_FONT_SECTION("ucg_font_inr38_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_mf[] UCG_FONT_SECTION("ucg_font_inr42_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_mn[] UCG_FONT_SECTION("ucg_font_inr42_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_mr[] UCG_FONT_SECTION("ucg_font_inr42_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_tf[] UCG_FONT_SECTION("ucg_font_inr42_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_tn[] UCG_FONT_SECTION("ucg_font_inr42_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr42_tr[] UCG_FONT_SECTION("ucg_font_inr42_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_mf[] UCG_FONT_SECTION("ucg_font_inr46_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_mn[] UCG_FONT_SECTION("ucg_font_inr46_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_mr[] UCG_FONT_SECTION("ucg_font_inr46_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_tf[] UCG_FONT_SECTION("ucg_font_inr46_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_tn[] UCG_FONT_SECTION("ucg_font_inr46_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr46_tr[] UCG_FONT_SECTION("ucg_font_inr46_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_mf[] UCG_FONT_SECTION("ucg_font_inr49_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_mn[] UCG_FONT_SECTION("ucg_font_inr49_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_mr[] UCG_FONT_SECTION("ucg_font_inr49_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_tf[] UCG_FONT_SECTION("ucg_font_inr49_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_tn[] UCG_FONT_SECTION("ucg_font_inr49_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr49_tr[] UCG_FONT_SECTION("ucg_font_inr49_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_mf[] UCG_FONT_SECTION("ucg_font_inr53_mf");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_mn[] UCG_FONT_SECTION("ucg_font_inr53_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_mr[] UCG_FONT_SECTION("ucg_font_inr53_mr");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_tf[] UCG_FONT_SECTION("ucg_font_inr53_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_tn[] UCG_FONT_SECTION("ucg_font_inr53_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr53_tr[] UCG_FONT_SECTION("ucg_font_inr53_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr57_mn[] UCG_FONT_SECTION("ucg_font_inr57_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr57_tf[] UCG_FONT_SECTION("ucg_font_inr57_tf");
extern const ucg_fntpgm_uint8_t ucg_font_inr57_tn[] UCG_FONT_SECTION("ucg_font_inr57_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr57_tr[] UCG_FONT_SECTION("ucg_font_inr57_tr");
extern const ucg_fntpgm_uint8_t ucg_font_inr62_mn[] UCG_FONT_SECTION("ucg_font_inr62_mn");
extern const ucg_fntpgm_uint8_t ucg_font_inr62_tn[] UCG_FONT_SECTION("ucg_font_inr62_tn");
extern const ucg_fntpgm_uint8_t ucg_font_inr62_tr[] UCG_FONT_SECTION("ucg_font_inr62_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_hf[] UCG_FONT_SECTION("ucg_font_logisoso16_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_hn[] UCG_FONT_SECTION("ucg_font_logisoso16_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_hr[] UCG_FONT_SECTION("ucg_font_logisoso16_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_tf[] UCG_FONT_SECTION("ucg_font_logisoso16_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_tn[] UCG_FONT_SECTION("ucg_font_logisoso16_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso16_tr[] UCG_FONT_SECTION("ucg_font_logisoso16_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_hf[] UCG_FONT_SECTION("ucg_font_logisoso18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_hn[] UCG_FONT_SECTION("ucg_font_logisoso18_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_hr[] UCG_FONT_SECTION("ucg_font_logisoso18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_tf[] UCG_FONT_SECTION("ucg_font_logisoso18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_tn[] UCG_FONT_SECTION("ucg_font_logisoso18_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso18_tr[] UCG_FONT_SECTION("ucg_font_logisoso18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_hf[] UCG_FONT_SECTION("ucg_font_logisoso20_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_hn[] UCG_FONT_SECTION("ucg_font_logisoso20_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_hr[] UCG_FONT_SECTION("ucg_font_logisoso20_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_tf[] UCG_FONT_SECTION("ucg_font_logisoso20_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_tn[] UCG_FONT_SECTION("ucg_font_logisoso20_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso20_tr[] UCG_FONT_SECTION("ucg_font_logisoso20_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_hf[] UCG_FONT_SECTION("ucg_font_logisoso22_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_hn[] UCG_FONT_SECTION("ucg_font_logisoso22_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_hr[] UCG_FONT_SECTION("ucg_font_logisoso22_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_tf[] UCG_FONT_SECTION("ucg_font_logisoso22_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_tn[] UCG_FONT_SECTION("ucg_font_logisoso22_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso22_tr[] UCG_FONT_SECTION("ucg_font_logisoso22_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_hf[] UCG_FONT_SECTION("ucg_font_logisoso24_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_hn[] UCG_FONT_SECTION("ucg_font_logisoso24_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_hr[] UCG_FONT_SECTION("ucg_font_logisoso24_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_tf[] UCG_FONT_SECTION("ucg_font_logisoso24_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_tn[] UCG_FONT_SECTION("ucg_font_logisoso24_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso24_tr[] UCG_FONT_SECTION("ucg_font_logisoso24_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_hf[] UCG_FONT_SECTION("ucg_font_logisoso26_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_hn[] UCG_FONT_SECTION("ucg_font_logisoso26_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_hr[] UCG_FONT_SECTION("ucg_font_logisoso26_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_tf[] UCG_FONT_SECTION("ucg_font_logisoso26_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_tn[] UCG_FONT_SECTION("ucg_font_logisoso26_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso26_tr[] UCG_FONT_SECTION("ucg_font_logisoso26_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_hf[] UCG_FONT_SECTION("ucg_font_logisoso28_hf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_hn[] UCG_FONT_SECTION("ucg_font_logisoso28_hn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_hr[] UCG_FONT_SECTION("ucg_font_logisoso28_hr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_tf[] UCG_FONT_SECTION("ucg_font_logisoso28_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_tn[] UCG_FONT_SECTION("ucg_font_logisoso28_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso28_tr[] UCG_FONT_SECTION("ucg_font_logisoso28_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30_tf[] UCG_FONT_SECTION("ucg_font_logisoso30_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30_tn[] UCG_FONT_SECTION("ucg_font_logisoso30_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso30_tr[] UCG_FONT_SECTION("ucg_font_logisoso30_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32_tf[] UCG_FONT_SECTION("ucg_font_logisoso32_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32_tn[] UCG_FONT_SECTION("ucg_font_logisoso32_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso32_tr[] UCG_FONT_SECTION("ucg_font_logisoso32_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34_tf[] UCG_FONT_SECTION("ucg_font_logisoso34_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34_tn[] UCG_FONT_SECTION("ucg_font_logisoso34_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso34_tr[] UCG_FONT_SECTION("ucg_font_logisoso34_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38_tf[] UCG_FONT_SECTION("ucg_font_logisoso38_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38_tn[] UCG_FONT_SECTION("ucg_font_logisoso38_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso38_tr[] UCG_FONT_SECTION("ucg_font_logisoso38_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42_tf[] UCG_FONT_SECTION("ucg_font_logisoso42_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42_tn[] UCG_FONT_SECTION("ucg_font_logisoso42_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso42_tr[] UCG_FONT_SECTION("ucg_font_logisoso42_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso46_tf[] UCG_FONT_SECTION("ucg_font_logisoso46_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso46_tn[] UCG_FONT_SECTION("ucg_font_logisoso46_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso46_tr[] UCG_FONT_SECTION("ucg_font_logisoso46_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso50_tf[] UCG_FONT_SECTION("ucg_font_logisoso50_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso50_tn[] UCG_FONT_SECTION("ucg_font_logisoso50_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso50_tr[] UCG_FONT_SECTION("ucg_font_logisoso50_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso54_tf[] UCG_FONT_SECTION("ucg_font_logisoso54_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso54_tn[] UCG_FONT_SECTION("ucg_font_logisoso54_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso54_tr[] UCG_FONT_SECTION("ucg_font_logisoso54_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso58_tf[] UCG_FONT_SECTION("ucg_font_logisoso58_tf");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso58_tn[] UCG_FONT_SECTION("ucg_font_logisoso58_tn");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso58_tr[] UCG_FONT_SECTION("ucg_font_logisoso58_tr");
extern const ucg_fntpgm_uint8_t ucg_font_logisoso62_tn[] UCG_FONT_SECTION("ucg_font_logisoso62_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_hf[] UCG_FONT_SECTION("ucg_font_osb18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_hn[] UCG_FONT_SECTION("ucg_font_osb18_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_hr[] UCG_FONT_SECTION("ucg_font_osb18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_tf[] UCG_FONT_SECTION("ucg_font_osb18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_tn[] UCG_FONT_SECTION("ucg_font_osb18_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb18_tr[] UCG_FONT_SECTION("ucg_font_osb18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_hf[] UCG_FONT_SECTION("ucg_font_osb21_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_hn[] UCG_FONT_SECTION("ucg_font_osb21_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_hr[] UCG_FONT_SECTION("ucg_font_osb21_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_tf[] UCG_FONT_SECTION("ucg_font_osb21_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_tn[] UCG_FONT_SECTION("ucg_font_osb21_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb21_tr[] UCG_FONT_SECTION("ucg_font_osb21_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_hf[] UCG_FONT_SECTION("ucg_font_osb26_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_hn[] UCG_FONT_SECTION("ucg_font_osb26_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_hr[] UCG_FONT_SECTION("ucg_font_osb26_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_tf[] UCG_FONT_SECTION("ucg_font_osb26_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_tn[] UCG_FONT_SECTION("ucg_font_osb26_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb26_tr[] UCG_FONT_SECTION("ucg_font_osb26_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_hf[] UCG_FONT_SECTION("ucg_font_osb29_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_hn[] UCG_FONT_SECTION("ucg_font_osb29_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_hr[] UCG_FONT_SECTION("ucg_font_osb29_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_tf[] UCG_FONT_SECTION("ucg_font_osb29_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_tn[] UCG_FONT_SECTION("ucg_font_osb29_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb29_tr[] UCG_FONT_SECTION("ucg_font_osb29_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_hf[] UCG_FONT_SECTION("ucg_font_osb35_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_hn[] UCG_FONT_SECTION("ucg_font_osb35_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_hr[] UCG_FONT_SECTION("ucg_font_osb35_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_tf[] UCG_FONT_SECTION("ucg_font_osb35_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_tn[] UCG_FONT_SECTION("ucg_font_osb35_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb35_tr[] UCG_FONT_SECTION("ucg_font_osb35_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_hf[] UCG_FONT_SECTION("ucg_font_osb41_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_hn[] UCG_FONT_SECTION("ucg_font_osb41_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_hr[] UCG_FONT_SECTION("ucg_font_osb41_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_tf[] UCG_FONT_SECTION("ucg_font_osb41_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_tn[] UCG_FONT_SECTION("ucg_font_osb41_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osb41_tr[] UCG_FONT_SECTION("ucg_font_osb41_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_hf[] UCG_FONT_SECTION("ucg_font_osr18_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_hn[] UCG_FONT_SECTION("ucg_font_osr18_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_hr[] UCG_FONT_SECTION("ucg_font_osr18_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_tf[] UCG_FONT_SECTION("ucg_font_osr18_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_tn[] UCG_FONT_SECTION("ucg_font_osr18_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr18_tr[] UCG_FONT_SECTION("ucg_font_osr18_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_hf[] UCG_FONT_SECTION("ucg_font_osr21_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_hn[] UCG_FONT_SECTION("ucg_font_osr21_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_hr[] UCG_FONT_SECTION("ucg_font_osr21_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_tf[] UCG_FONT_SECTION("ucg_font_osr21_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_tn[] UCG_FONT_SECTION("ucg_font_osr21_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr21_tr[] UCG_FONT_SECTION("ucg_font_osr21_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_hf[] UCG_FONT_SECTION("ucg_font_osr26_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_hn[] UCG_FONT_SECTION("ucg_font_osr26_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_hr[] UCG_FONT_SECTION("ucg_font_osr26_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_tf[] UCG_FONT_SECTION("ucg_font_osr26_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_tn[] UCG_FONT_SECTION("ucg_font_osr26_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr26_tr[] UCG_FONT_SECTION("ucg_font_osr26_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_hf[] UCG_FONT_SECTION("ucg_font_osr29_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_hn[] UCG_FONT_SECTION("ucg_font_osr29_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_hr[] UCG_FONT_SECTION("ucg_font_osr29_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_tf[] UCG_FONT_SECTION("ucg_font_osr29_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_tn[] UCG_FONT_SECTION("ucg_font_osr29_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr29_tr[] UCG_FONT_SECTION("ucg_font_osr29_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_hf[] UCG_FONT_SECTION("ucg_font_osr35_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_hn[] UCG_FONT_SECTION("ucg_font_osr35_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_hr[] UCG_FONT_SECTION("ucg_font_osr35_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_tf[] UCG_FONT_SECTION("ucg_font_osr35_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_tn[] UCG_FONT_SECTION("ucg_font_osr35_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr35_tr[] UCG_FONT_SECTION("ucg_font_osr35_tr");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_hf[] UCG_FONT_SECTION("ucg_font_osr41_hf");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_hn[] UCG_FONT_SECTION("ucg_font_osr41_hn");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_hr[] UCG_FONT_SECTION("ucg_font_osr41_hr");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_tf[] UCG_FONT_SECTION("ucg_font_osr41_tf");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_tn[] UCG_FONT_SECTION("ucg_font_osr41_tn");
extern const ucg_fntpgm_uint8_t ucg_font_osr41_tr[] UCG_FONT_SECTION("ucg_font_osr41_tr");

#endif



#ifdef __cplusplus
}
#endif


#endif /* _UCG_H */
