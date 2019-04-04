# ucg Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-08-05 | [Oli Kraus](https://github.com/olikraus/ucglib), [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [ucglib](../../app/ucglib/)|

Ucglib is a graphics library developed at [olikraus/ucglib](https://github.com/olikraus/ucglib) with support for color TFT displays.

!!! note "BSD License for Ucglib Code"
    Universal 8bit Graphics Library (http://code.google.com/p/u8glib/)

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

The NodeMCU firmware supports a subset of these:

- HX8352C
- ILI9163
- ILI9341
- ILI9486
- PCF8833
- SEPS225
- SSD1331
- SSD1351
- ST7735

This integration is based on version [1.5.2](https://github.com/olikraus/Ucglib_Arduino/releases/tag/1.5.2).

## Overview

### SPI Connection

The HSPI module is used ([more information](https://web.archive.org/web/20180425221055/http://d.av.id.au:80/blog/esp8266-hardware-spi-hspi-general-info-and-pinout/)), so certain pins are fixed:

* HSPI CLK  = GPIO14
* HSPI MOSI = GPIO13
* HSPI MISO = GPIO12 (not used)

All other pins can be assigned to any available GPIO:

* CS
* D/C
* RES (optional for some displays)

Also refer to the initialization sequence eg in [GraphicsTest.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/ucglib/GraphicsTest.lua):
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

### Display selection
HW SPI based displays with support in u8g2 can be enabled.

The procedure is different for ESP8266 and ESP32 platforms.

#### ESP8266

Add the desired entries to the display table in [app/include/ucg_config.h](../../app/include/ucg_config.h):
```c
#define UCG_DISPLAY_TABLE                          \
    UCG_DISPLAY_TABLE_ENTRY(ili9341_18x240x320_hw_spi, ucg_dev_ili9341_18x240x320, ucg_ext_ili9341_18) \
    UCG_DISPLAY_TABLE_ENTRY(st7735_18x128x160_hw_spi, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18) \
```

#### ESP32
Enable the desired entries for SPI displays in ucg's sub-menu (run `make menuconfig`).

### Fonts
ucglib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image.

The procedure is different for ESP8266 and ESP32 platforms.

#### ESP8266

Add the desired fonts to the font table in  [app/include/ucg_config.h](../../app/include/ucg_config.h):
```c
#define UCG_FONT_TABLE                              \
    UCG_FONT_TABLE_ENTRY(font_7x13B_tr)             \
    UCG_FONT_TABLE_ENTRY(font_helvB12_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB18_hr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR12_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR14_hr)
```
They will be available as `ucg.<font_name>` in Lua.

#### ESP32
Add the desired fonts to the font selection sub-entry via `make menuconfig`.

## Display Drivers

Initialize a display via Hardware SPI.

- `hx8352c_18x240x400_hw_spi()`
- `ili9163_18x128x128_hw_spi()`
- `ili9341_18x240x320_hw_spi()`
- `ili9486_18x320x480_hw_spi()`
- `pcf8833_16x132x132_hw_spi()`
- `seps225_16x128x128_uvis_hw_spi()`
- `ssd1351_18x128x128_hw_spi()`
- `ssd1351_18x128x128_ft_hw_spi()`
- `ssd1331_18x96x64_uvis_hw_spi()`
- `st7735_18x128x160_hw_spi()`

#### Syntax
`ucg.st7735_18x128x160_hw_spi(bus, cs, dc[, res])`

#### Parameters
- `bus` SPI master bus
- `cs` GPIO pin for /CS
- `dc` GPIO pin for DC
- `res` GPIO pin for /RES, none if omitted

#### Returns
ucg display object

#### Example for ESP8266
```lua
-- Hardware SPI CLK  = GPIO14
-- Hardware SPI MOSI = GPIO13
-- Hardware SPI MISO = GPIO12 (not used)
-- Hardware SPI /CS  = GPIO15 (not used)
cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
bus = 1
spi.setup(bus, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, spi.DATABITS_8, 0)
-- we won't be using the HSPI /CS line, so disable it again
gpio.mode(8, gpio.INPUT, gpio.PULLUP)

disp = ucg.st7735_18x128x160_hw_spi(bus, cs, dc, res)
```

#### Example for ESP32
```lua
sclk = 19
mosi = 23
cs   = 22
dc   = 16
res  = 17
bus = spi.master(spi.HSPI, {sclk=sclk, mosi=mosi})
disp = ucg.st7735_18x128x160_hw_spi(bus, cs, dc, res)
```

## Constants
Constants for various functions.

`ucg.FONT_MODE_TRANSPARENT`, `ucg.FONT_MODE_SOLID`, `ucg.DRAW_UPPER_RIGHT`,
`ucg.DRAW_UPPER_LEFT`, `ucg.DRAW_LOWER_RIGHT`, `ucg.DRAW_LOWER_LEFT`, `ucg.DRAW_ALL`

`ucg.font_7x13B_tr`, ...

# ucg.disp Sub-Module

## ucg.disp:begin()
See [ucglib begin()](https://github.com/olikraus/ucglib/wiki/reference#begin).

## ucg.disp:clearScreen()
See [ucglib clearScreen()](https://github.com/olikraus/ucglib/wiki/reference#clearscreen).

## ucg.disp:draw90Line()
See [ucglib draw90Line()](https://github.com/olikraus/ucglib/wiki/reference#draw90line).

## ucg.disp:drawBox()
See [ucglib drawBox()](https://github.com/olikraus/ucglib/wiki/reference#drawbox).

## ucg.disp:drawCircle()
See [ucglib drawCircle()](https://github.com/olikraus/ucglib/wiki/reference#drawcircle).

## ucg.disp:drawDisc()
See [ucglib drawDisc()](https://github.com/olikraus/ucglib/wiki/reference#drawdisc).

## ucg.disp:drawFrame()
See [ucglib drawFrame()](https://github.com/olikraus/ucglib/wiki/reference#drawframe).

## ucg.disp:drawGlyph()
See [ucglib drawGlyph()](https://github.com/olikraus/ucglib/wiki/reference#drawglyph).

## ucg.disp:drawGradientBox()
See [ucglib drawGradientBox()](https://github.com/olikraus/ucglib/wiki/reference#drawgradientbox).

## ucg.disp:drawGradientLine()
See [ucglib drawGradientLine()](https://github.com/olikraus/ucglib/wiki/reference#drawgradientline).

## ucg.disp:drawHLine()
See [ucglib drawHLine()](https://github.com/olikraus/ucglib/wiki/reference#drawhline).

## ucg.disp:drawLine()
See [ucglib drawLine()](https://github.com/olikraus/ucglib/wiki/reference#drawline).

## ucg.disp:drawPixel()
See [ucglib drawPixel()](https://github.com/olikraus/ucglib/wiki/reference#drawpixel).

## ucg.disp:drawRBox()
See [ucglib drawRBox()](https://github.com/olikraus/ucglib/wiki/reference#drawrbox).

## ucg.disp:drawRFrame()
See [ucglib drawRFrame()](https://github.com/olikraus/ucglib/wiki/reference#drawrframe).

## ucg.disp:drawString()
See [ucglib drawString()](https://github.com/olikraus/ucglib/wiki/reference#drawstring).

## ucg.disp:drawTetragon()
See [ucglib drawTetragon()](https://github.com/olikraus/ucglib/wiki/reference#drawtetragon).

## ucg.disp:drawTriangle()
See [ucglib drawTriangle()](https://github.com/olikraus/ucglib/wiki/reference#drawrtiangle).

## ucg.disp:drawVLine()
See [ucglib drawVline()](https://github.com/olikraus/ucglib/wiki/reference#drawvline).

## ucg.disp:getFontAscent()
See [ucglib getFontAscent()](https://github.com/olikraus/ucglib/wiki/reference#getfontascent).

## ucg.disp:getFontDescent()
See [ucglib getFontDescent()](https://github.com/olikraus/ucglib/wiki/reference#getfontdescent).

## ucg.disp:getHeight()
See [ucglib getHeight()](https://github.com/olikraus/ucglib/wiki/reference#getheight).

## ucg.disp:getStrWidth()
See [ucglib getStrWidth()](https://github.com/olikraus/ucglib/wiki/reference#getstrwidth).

## ucg.disp:getWidth()
See [ucglib getWidth()](https://github.com/olikraus/ucglib/wiki/reference#getwidth).

## ucg.disp:print()
See [ucglib print()](https://github.com/olikraus/ucglib/wiki/reference#print).

## ucg.disp:setClipRange()
See [ucglib setClipRange()](https://github.com/olikraus/ucglib/wiki/reference#setcliprange).

## ucg.disp:setColor()
See [ucglib setColor()](https://github.com/olikraus/ucglib/wiki/reference#setcolor).

## ucg.disp:setFont()
Define a ucg font for the glyph and string drawing functions. They are available as `ucg.<font_name>` in Lua.

#### Syntax
`disp:setFont(font)`

#### Parameters
`font` constant to identify pre-compiled font

#### Returns
`nil`

#### Example
```lua
disp:setFont(ucg.font_7x13B_tr)
```

#### See also
[ucglib setFont()](https://github.com/olikraus/ucglib/wiki/reference#setfont)

## ucg.disp:setFontMode()
See [ucglib setFontMode()](https://github.com/olikraus/ucglib/wiki/reference#setfontmode).

## ucg.disp:setFontPosBaseline()
See [ucglib setFontPosBaseline()](https://github.com/olikraus/ucglib/wiki/reference#setfontposbaseline).

## ucg.disp:setFontPosBottom()
See [ucglib setFontPosBottom()](https://github.com/olikraus/ucglib/wiki/reference#setfontposbottom).

## ucg.disp:setFontPosCenter()
See [ucglib setFontPosCenter()](https://github.com/olikraus/ucglib/wiki/reference#setfontposcenter).

## ucg.disp:setFontPosTop()
See [ucglib setFontPosTop()](https://github.com/olikraus/ucglib/wiki/reference#setfontpostop).

## ucg.disp:setMaxClipRange()
See [ucglib setMaxClipRange()](https://github.com/olikraus/ucglib/wiki/reference#setmaxcliprange).

## ucg.disp:setPrintDir()
See [ucglib setPrintDir()](https://github.com/olikraus/ucglib/wiki/reference#setprintdir).

## ucg.disp:setPrintPos()
See [ucglib setPrintPos()](https://github.com/olikraus/ucglib/wiki/reference#setprintpos).

## ucg.disp:setRotate90()
See [ucglib setRotate90()](https://github.com/olikraus/ucglib/wiki/reference#setrotate90).

## ucg.disp:setRotate180()
See [ucglib setRotate180()](https://github.com/olikraus/ucglib/wiki/reference#setrotate180).

## ucg.disp:setRotate270()
See [ucglib setRotate270()](https://github.com/olikraus/ucglib/wiki/reference#setrotate270).

## ucg.disp:setScale2x2()
See [ucglib setScale2x2()](https://github.com/olikraus/ucglib/wiki/reference#setscale2x2).

## ucg.disp:undoClipRange()
See [ucglib undoClipRange()](https://github.com/olikraus/ucglib/wiki/reference#undocliprange).

## ucg.disp:undoRotate()
See [ucglib undoRotate()](https://github.com/olikraus/ucglib/wiki/reference#undorotate).

## ucg.disp:undoScale()
See [ucglib undoScale()](https://github.com/olikraus/ucglib/wiki/reference#undoscale).
