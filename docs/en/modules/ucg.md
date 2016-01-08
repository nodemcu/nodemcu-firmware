# ucg Module
Ucglib is a graphics library developed at [olikraus/ucglib](https://github.com/olikraus/ucglib) with support for color TFT displays. The NodeMCU firmware supports a subset of these:

- ILI9163
- ILI9341
- PCF8833
- SEPS225
- SSD1331
- SSD1351
- ST7735

This integration is based on [v1.3.3](https://github.com/olikraus/Ucglib_Arduino/releases/tag/v1.3.3).

## Overview

### SPI Connection

The HSPI module is used, so certain pins are fixed:

* HSPI CLK  = GPIO14
* HSPI MOSI = GPIO13
* HSPI MISO = GPIO12 (not used)

All other pins can be assigned to any available GPIO:

* CS
* D/C
* RES (optional for some displays)

Also refer to the initialization sequence eg in [GraphicsTest.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/ucglib/GraphicsRest.lua):
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
```

### Library Usage
The Lua bindings for this library closely follow ucglib's object oriented C++ API. Based on the ucg class, you create an object for your display type.

ILI9341 via SPI:
```lua
cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = ucg.ili9341_18x240x320_hw_spi(cs, dc, res)
```

This object provides all of ucglib's methods to control the display.
Again, refer to [GraphicsTest.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/ucglib/GraphicsTest.lua) to get an impression how this is achieved with Lua code. Visit the [ucglib homepage](https://github.com/olikraus/ucglib) for technical details.

### Displays
o get access to the display constructors, add the desired entries to the display table in [app/include/ucg_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/ucg_config.h):
```c
#define UCG_DISPLAY_TABLE                          \
    UCG_DISPLAY_TABLE_ENTRY(ili9341_18x240x320_hw_spi, ucg_dev_ili9341_18x240x320, ucg_ext_ili9341_18) \
    UCG_DISPLAY_TABLE_ENTRY(st7735_18x128x160_hw_spi, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18) \
```

### Fonts
ucglib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in [app/include/ucg_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/ucg_config.h) and recompile. Simply add the desired fonts to the font table:
```c
#define UCG_FONT_TABLE                              \
    UCG_FONT_TABLE_ENTRY(font_7x13B_tr)             \
    UCG_FONT_TABLE_ENTRY(font_helvB12_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB18_hr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR12_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR14_hr)
```
They'll be available as `ucg.<font_name>` in Lua.

## Display Drivers

Initialize a display via Hardware SPI.

- `ili9163_18x128x128_hw_spi()`
- `ili9341_18x240x320_hw_spi()`
- `pcf8833_16x132x132_hw_spi()`
- `seps225_16x128x128_univision_hw_spi()`
- `ssd1351_18x128x128_hw_spi()`
- `ssd1351_18x128x128_ft_hw_spi()`
- `ssd1331_18x96x64_univision_hw_spi()`
- `st7735_18x128x160_hw_spi()`

#### Syntax
`ucg.st7735_18x128x160_hw_spi(cs, dc, res)`

#### Parameters
- `cs`: GPIO pin for /CS.
- `dc`: GPIO pin for DC.
- `res`: GPIO pin for /RES (optional).

#### Returns
ucg display object.

#### Example
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, spi.DATABITS_8, 0)

cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = ucg.st7735_18x128x160_hw_spi(cs, dc, res)
```

## Constants
`ucg.FONT_MODE_TRANSPARENT`, `ucg.FONT_MODE_SOLID`, `ucg.DRAW_UPPER_RIGHT`,
`ucg.DRAW_UPPER_LEFT`, `ucg.DRAW_LOWER_RIGHT`, `ucg.DRAW_LOWER_LEFT`, `ucg.DRAW_ALL`

`ucg.font_7x13B_tr`, ...

# ucg.disp Sub-Module

## [ucg.disp:begin()](https://github.com/olikraus/ucglib/wiki/reference#begin)

## [ucg.disp:clearScreen()](https://github.com/olikraus/ucglib/wiki/reference#clearscreen)

## [ucg.disp:draw90Line()](https://github.com/olikraus/ucglib/wiki/reference#draw90line)

## [ucg.disp:drawBox()](https://github.com/olikraus/ucglib/wiki/reference#drawbox)

## [ucg.disp:drawCircle()](https://github.com/olikraus/ucglib/wiki/reference#drawcircle)

## [ucg.disp:drawDisc()](https://github.com/olikraus/ucglib/wiki/reference#drawdisc)

## [ucg.disp:drawFrame()](https://github.com/olikraus/ucglib/wiki/reference#drawframe)

## [ucg.disp:drawGlyph()](https://github.com/olikraus/ucglib/wiki/reference#drawglyph)

## [ucg.disp:drawGradientBox()](https://github.com/olikraus/ucglib/wiki/reference#drawgradientbox)

## [ucg.disp:drawGradientLine()](https://github.com/olikraus/ucglib/wiki/reference#drawgradientline)

## [ucg.disp:drawHLine()](https://github.com/olikraus/ucglib/wiki/reference#drawhline)

## [ucg.disp:drawLine()](https://github.com/olikraus/ucglib/wiki/reference#drawline)

## [ucg.disp:drawPixel()](https://github.com/olikraus/ucglib/wiki/reference#drawpixel)

## [ucg.disp:drawRBox()](https://github.com/olikraus/ucglib/wiki/reference#drawrbox)

## [ucg.disp:drawRFrame()](https://github.com/olikraus/ucglib/wiki/reference#drawrframe)

## [ucg.disp:drawString()](https://github.com/olikraus/ucglib/wiki/reference#drawstring)

## [ucg.disp:drawTetragon()](https://github.com/olikraus/ucglib/wiki/reference#drawtetragon)

## [ucg.disp:drawTriangle()](https://github.com/olikraus/ucglib/wiki/reference#drawrtiangle)

## [ucg.disp:drawVLine()](https://github.com/olikraus/ucglib/wiki/reference#drawvline)

## [ucg.disp:getFontAscent()](https://github.com/olikraus/ucglib/wiki/reference#getfontascent)

## [ucg.disp:getFontDescent()](https://github.com/olikraus/ucglib/wiki/reference#getfontdescent)

## [ucg.disp:getHeight()](https://github.com/olikraus/ucglib/wiki/reference#getheight)

## [ucg.disp:getStrWidth()](https://github.com/olikraus/ucglib/wiki/reference#getstrwidth)

## [ucg.disp:getWidth()](https://github.com/olikraus/ucglib/wiki/reference#getwidth)

## [ucg.disp:print()](https://github.com/olikraus/ucglib/wiki/reference#print)

## [ucg.disp:setClipRange()](https://github.com/olikraus/ucglib/wiki/reference#setcliprange)

## [ucg.disp:setColor()](https://github.com/olikraus/ucglib/wiki/reference#setcolor)

## ucg.disp:setFont()
ucglib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in [app/include/ucg_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/ucg_config.h) and recompile. Simply add the desired fonts to the font table:
```c
#define UCG_FONT_TABLE                              \
    UCG_FONT_TABLE_ENTRY(font_7x13B_tr)             \
    UCG_FONT_TABLE_ENTRY(font_helvB12_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB18_hr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR12_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR14_hr)
```
They'll be available as `ucg.<font_name>` in Lua.

#### Syntax
`disp:setFont(font)`

#### Parameters
- `font`: Constant to identify pre-compiled font.

#### Returns
`nil`

#### Example
```lua
disp:setFont(ucg.font_7x13B_tr)
```

#### See also
- [ucglib setFont()](https://github.com/olikraus/ucglib/wiki/reference#setfont)

## [ucg.disp:setFontMode()](https://github.com/olikraus/ucglib/wiki/reference#setfontmode)

## [ucg.disp:setFontPosBaseline()](https://github.com/olikraus/ucglib/wiki/reference#setfontposbaseline)

## [ucg.disp:setFontPosBottom()](https://github.com/olikraus/ucglib/wiki/reference#setfontposbottom)

## [ucg.disp:setFontPosCenter()](https://github.com/olikraus/ucglib/wiki/reference#setfontposcenter)

## [ucg.disp:setFontPosTop()](https://github.com/olikraus/ucglib/wiki/reference#setfontpostop)

## [ucg.disp:setMaxClipRange()](https://github.com/olikraus/ucglib/wiki/reference#setmaxcliprange)

## [ucg.disp:setPrintDir()](https://github.com/olikraus/ucglib/wiki/reference#setprintdir)

## [ucg.disp:setPrintPos()](https://github.com/olikraus/ucglib/wiki/reference#setprintpos)

## [ucg.disp:setRotate90()](https://github.com/olikraus/ucglib/wiki/reference#setrotate90)

## [ucg.disp:setRotate180()](https://github.com/olikraus/ucglib/wiki/reference#setrotate180)

## [ucg.disp:setRotate270()](https://github.com/olikraus/ucglib/wiki/reference#setrotate270)

## [ucg.disp:setScale2x2()](https://github.com/olikraus/ucglib/wiki/reference#setscale2x2)

## [ucg.disp:undoClipRange()](https://github.com/olikraus/ucglib/wiki/reference#undocliprange)

## [ucg.disp:undoRotate()](https://github.com/olikraus/ucglib/wiki/reference#undorotate)

## [ucg.disp:undoScale()](https://github.com/olikraus/ucglib/wiki/reference#undoscale)
