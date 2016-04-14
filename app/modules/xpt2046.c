// Module for xpt2046
// by Starofall
// used source from:
//  - https://github.com/spapadim/XPT2046/
//  - https://github.com/PaulStoffregen/XPT2046_Touchscreen/

#include "module.h"
#include "lauxlib.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "gpio.h"

// Hardware specific
static const uint16_t CAL_MARGIN = 20;
static const uint8_t CTRL_LO_DFR = 0b0011;
static const uint8_t CTRL_LO_SER = 0b0100;
static const uint8_t CTRL_HI_X = 0b1001  << 4;
static const uint8_t CTRL_HI_Y = 0b1101  << 4;
static const uint16_t ADC_MAX = 0x0fff;  // 12 bits

// Runtime variables
uint16_t _width, _height;
uint8_t _cs_pin, _irq_pin;
int32_t _cal_dx, _cal_dy, _cal_dvi, _cal_dvj;
uint16_t _cal_vi1, _cal_vj1;

// Average pair with least distance between each
static int16_t ICACHE_FLASH_ATTR besttwoavg( int16_t x , int16_t y , int16_t z ) {
  int16_t da, db, dc;
  int16_t reta = 0;

  if ( x > y ) da = x - y; else da = y - x;
  if ( x > z ) db = x - z; else db = z - x;
  if ( z > y ) dc = z - y; else dc = y - z;

  if ( da <= db && da <= dc ) reta = (x + y) >> 1;
  else if ( db <= da && db <= dc ) reta = (x + z) >> 1;
  else reta = (y + z) >> 1;   

  return reta;
}

// Checks if the irq_pin is down
static int ICACHE_FLASH_ATTR isTouching() { 
	return (platform_gpio_read(_irq_pin) == 0); 
}

// transfer 16 bits from the touch display - returns the recived uint16_t
static uint16_t ICACHE_FLASH_ATTR transfer16(uint16_t _data) {
	union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } t;
	t.val = _data;
	t.msb = platform_spi_send_recv(1, 8 , t.msb);
	t.lsb = platform_spi_send_recv(1, 8 , t.lsb);
	return t.val;
}

// reads the value from the touch panel
static uint16_t ICACHE_FLASH_ATTR _readLoop(uint8_t ctrl, uint8_t max_samples) {
  uint16_t prev = 0xffff, cur = 0xffff;
  uint8_t i = 0;
  do {
    prev = cur;
    cur = platform_spi_send_recv(1, 8 , 0);
    cur = (cur << 4) | (platform_spi_send_recv(1, 8 , ctrl) >> 4);  // 16 clocks -> 12-bits (zero-padded at end)
  } while ((prev != cur) && (++i < max_samples));
  return cur;
}

// returns the raw position information
static void ICACHE_FLASH_ATTR getRaw(uint16_t *vi, uint16_t *vj) {
  // Implementation based on TI Technical Note http://www.ti.com/lit/an/sbaa036/sbaa036.pdf

  platform_gpio_write(_cs_pin, 0);
  platform_spi_send_recv(1, 8 , CTRL_HI_X | CTRL_LO_DFR);  // Send first control int
  *vi = _readLoop(CTRL_HI_X | CTRL_LO_DFR, 255);
  *vj = _readLoop(CTRL_HI_Y | CTRL_LO_DFR, 255);

  // Turn off ADC by issuing one more read (throwaway)
  // This needs to be done, because PD=0b11 (needed for MODE_DFR) will disable PENIRQ
  platform_spi_send_recv(1, 8 , 0);  // Maintain 16-clocks/conversion; _readLoop always ends after issuing a control int
  platform_spi_send_recv(1, 8 , CTRL_HI_Y | CTRL_LO_SER);
  transfer16(0);  // Flush last read, just to be sure
  
  platform_gpio_write(_cs_pin, 1);
}

// sets the calibration of the display
static void ICACHE_FLASH_ATTR setCalibration (uint16_t vi1, uint16_t vj1, uint16_t vi2, uint16_t vj2) {
  _cal_dx = _width - 2*CAL_MARGIN;
  _cal_dy = _height - 2*CAL_MARGIN;

  _cal_vi1 = (int32_t)vi1;
  _cal_vj1 = (int32_t)vj1;
  _cal_dvi = (int32_t)vi2 - vi1;
  _cal_dvj = (int32_t)vj2 - vj1;
}

// returns the position on the screen by also applying the calibration
static void ICACHE_FLASH_ATTR getPosition (uint16_t *x, uint16_t *y) {
  if (isTouching() == 0) {
    *x = *y = 0xffff;
    return;
  }

  uint16_t vi, vj;
  getRaw(&vi, &vj);

  // Map to (un-rotated) display coordinates
  *x = (uint16_t)(_cal_dx * (vj - _cal_vj1) / _cal_dvj + CAL_MARGIN);
  *y = (uint16_t)(_cal_dy * (vi - _cal_vi1) / _cal_dvi + CAL_MARGIN);
}


// Lua: xpt2046.setup(cspin,irqpin,height,width)
static int ICACHE_FLASH_ATTR xpt2046_setup( lua_State* L ) {
  _cs_pin  = luaL_checkinteger( L, 1 );
  _irq_pin = luaL_checkinteger( L, 2 );
  _height  = luaL_checkinteger( L, 3 );
  _width  = luaL_checkinteger( L, 4 );
  // set pins correct
  platform_gpio_mode(_cs_pin  , 1, 0);; //output
  platform_gpio_mode(_irq_pin , 0, 1); //input with pullup
  
  setCalibration(
    /*vi1=*/((int32_t)CAL_MARGIN) * ADC_MAX / _width,
    /*vj1=*/((int32_t)CAL_MARGIN) * ADC_MAX / _height,
    /*vi2=*/((int32_t)_width - CAL_MARGIN) * ADC_MAX / _width,
    /*vj2=*/((int32_t)_height - CAL_MARGIN) * ADC_MAX / _height
  );
  
  // assume spi was inited before with a clockDiv of maximum 16
  // as higher clockDivs produced inaccurate results

  // do first powerdown
  platform_gpio_write(_cs_pin, 0);

  // Issue a throw-away read, with power-down enabled (PD{1,0} == 0b00)
  // Otherwise, ADC is disabled
  platform_spi_send_recv(1, 8, CTRL_HI_Y | CTRL_LO_SER);
  transfer16(0); // Flush, just to be sure

  platform_gpio_write(_cs_pin, 1);  
  return 0;
}

// Lua: xpt2046.isTouched()
static int ICACHE_FLASH_ATTR xpt2046_isTouched( lua_State* L ) {
  lua_pushinteger( L, isTouching());
  return 1;
}

// Lua: xpt2046.setCalibration(a,b,c,d)
static int ICACHE_FLASH_ATTR xpt2046_setCalibration( lua_State* L ) {
  int32_t a = luaL_checkinteger( L, 1 );
  int32_t b = luaL_checkinteger( L, 2 );
  int32_t c = luaL_checkinteger( L, 3 );
  int32_t d = luaL_checkinteger( L, 4 );
  setCalibration(a,b,c,d);
  return 0;
}

// Lua: xpt2046.xpt2046_getRaw()
static int ICACHE_FLASH_ATTR xpt2046_getRaw( lua_State* L ) {
  uint16_t x, y;
  getRaw(&x, &y);
  lua_pushinteger( L, x);
  lua_pushinteger( L, y);
  return 2;
}

// Lua: xpt2046.xpt2046_getPosition()
static int ICACHE_FLASH_ATTR xpt2046_getPosition( lua_State* L ) {
  uint16_t x, y;
  getPosition(&x, &y);
  lua_pushinteger( L, x);
  lua_pushinteger( L, y);
  return 2;
}


// Lua: xpt2046.xpt2046_getPositionMeaned()
static int ICACHE_FLASH_ATTR xpt2046_getPositionMeaned( lua_State* L ) {
  // run three times
  uint16_t x1, y1, x2, y2, x3, y3;
  getPosition(&x1, &y1);
  getPosition(&x2, &y2);
  getPosition(&x3, &y3);

  //avg the results
  int16_t x = besttwoavg(x1,x2,x3);
  int16_t y = besttwoavg(y1,y2,y3);

  lua_pushinteger( L, x);
  lua_pushinteger( L, y);
  return 2;
}

// Module function map
static const LUA_REG_TYPE xpt2046_map[] = {
  { LSTRKEY( "isTouched")  , LFUNCVAL(xpt2046_isTouched)  },
  { LSTRKEY( "getRaw"   )  , LFUNCVAL(xpt2046_getRaw)     },
  { LSTRKEY( "getPosition"), LFUNCVAL(xpt2046_getPosition)},
  { LSTRKEY( "getPositionMeaned"), LFUNCVAL(xpt2046_getPositionMeaned)},
  { LSTRKEY( "setCalibration"), LFUNCVAL(xpt2046_setCalibration)},
  { LSTRKEY( "setup"    )  , LFUNCVAL(xpt2046_setup)      },
  { LNILKEY, LNILVAL }
};


NODEMCU_MODULE(XPT2046, "xpt2046", xpt2046_map, NULL);
