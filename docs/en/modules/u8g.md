# u8g Module
U8glib is a graphics library developed at [olikraus/u8glib](https://github.com/olikraus/u8glib) with support for many different displays. The NodeMCU firmware supports a subset of these.

I2C and SPI mode:

- sh1106_128x64
- ssd1306 - 128x64 and 64x48 variants
- ssd1309_128x64
- ssd1327_96x96_gr
- uc1611 - dogm240 and dogxl240 variants

SPI only:

- ld7032_60x32
- pcd8544_84x48
- pcf8812_96x65
- ssd1322_nhd31oled - bw and gr variants
- ssd1325_nhd27oled - bw and gr variants
- ssd1351_128x128 - gh and hicolor variants
- st7565_64128n - variants 64128n, dogm128/132, lm6059/lm6063, c12832/c12864
- uc1601_c128032
- uc1608 - 240x128 and 240x64 variants
- uc1610_dogxl160 - bw and gr variants
- uc1611 - dogm240 and dogxl240 variants
- uc1701 - dogs102 and mini12864 variants

This integration is based on [v1.18.1](https://github.com/olikraus/U8glib_Arduino/releases/tag/1.18.1).

## Overview
### I2C Connection
Hook up SDA and SCL to any free GPIOs. Eg. [u8g_graphics_test.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_graphics_test.lua) expects SDA=5 (GPIO14) and SCL=6 (GPIO12). They are used to set up nodemcu's I2C driver before accessing the display:
```lua
sda = 5
scl = 6
i2c.setup(0, sda, scl, i2c.SLOW)
```
### SPI connection
The HSPI module is used ([more information](http://d.av.id.au/blog/esp8266-hardware-spi-hspi-general-info-and-pinout/)), so certain pins are fixed:

- HSPI CLK = GPIO14
- HSPI MOSI = GPIO13
- HSPI MISO = GPIO12 (not used)

All other pins can be assigned to any available GPIO:

- CS
- D/C
- RES (optional for some displays)

Also refer to the initialization sequence eg in [u8g_graphics_test.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_graphics_test.lua):
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
```

### Library Usage
The Lua bindings for this library closely follow u8glib's object oriented C++ API. Based on the u8g class, you create an object for your display type.

SSD1306 via I2C:
```lua
sla = 0x3c
disp = u8g.ssd1306_128x64_i2c(sla)
```

SSD1306 via SPI:
```lua
cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = u8g.ssd1306_128x64_hw_spi(cs, dc, res)
```

This object provides all of u8glib's methods to control the display. Again, refer to [u8g_graphics_test.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_graphics_test.lua) to get an impression how this is achieved with Lua code. Visit the [u8glib homepage](https://github.com/olikraus/u8glib) for technical details.

### Displays
I2C and HW SPI based displays with support in u8glib can be enabled. To get access to the respective constructors, add the desired entries to the I2C or SPI display tables in [app/include/u8g_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/u8g_config.h):
```c
#define U8G_DISPLAY_TABLE_I2C                   \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_i2c) \

#define U8G_DISPLAY_TABLE_SPI                      \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_hw_spi) \
    U8G_DISPLAY_TABLE_ENTRY(pcd8544_84x48_hw_spi)  \
    U8G_DISPLAY_TABLE_ENTRY(pcf8812_96x65_hw_spi)  \
```

### Fonts
u8glib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in [app/include/u8g_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/u8g_config.h) and recompile. Simply add the desired fonts to the font table:
```c
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
    U8G_FONT_TABLE_ENTRY(font_chikita)
```

They'll become available as `u8g.<font_name>` in Lua.

### Bitmaps
Bitmaps and XBMs are supplied as strings to `drawBitmap()` and `drawXBM()`. This off-loads all data handling from the u8g module to generic methods for binary files. See [u8g_bitmaps.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_bitmaps.lua).
In contrast to the source code based inclusion of XBMs into u8glib, it's required to provide precompiled binary files. This can be performed online with [Online-Utility's Image Converter](http://www.online-utility.org/image_converter.jsp): Convert from XBM to MONO format and upload the binary result with [nodemcu-uploader.py](https://github.com/kmpm/nodemcu-uploader).

## I2C Display Drivers
Initialize a display via I2C.

- `u8g.sh1106_128x64_i2c()`
- `u8g.ssd1306_128x64_i2c()`
- `u8g.ssd1306_64x48_i2c()`
- `u8g.ssd1309_128x64_i2c()`
- `u8g.ssd1327_96x96_gr_i2c()`
- `u8g.uc1611_dogm240_i2c()`
- `u8g.uc1611_dogxl240_i2c()`

####Syntax
`u8g.ssd1306_128x64_i2c(address)`

####Parameters
`address` I2C slave address of display

####Returns
u8g display object

####Example
```lua
sda = 5
scl = 6
i2c.setup(0, sda, scl, i2c.SLOW)

sla = 0x3c
disp = u8g.ssd1306_128x64_i2c(sla)
```

####See also
[SPI Display Drivers](#spi-display-drivers)

## SPI Display Drivers
Initialize a display via Hardware SPI.

- `u8g.ld7032_60x32_hw_spi()`
- `u8g.pcd8544_84x48_hw_spi()`
- `u8g.pcf8812_96x65_hw_spi()`
- `u8g.sh1106_128x64_hw_spi()`
- `u8g.ssd1306_128x64_hw_spi()`
- `u8g.ssd1306_64x48_hw_spi()`
- `u8g.ssd1309_128x64_hw_spi()`
- `u8g.ssd1322_nhd31oled_bw_hw_spi()`
- `u8g.ssd1322_nhd31oled_gr_hw_spi()`
- `u8g.ssd1325_nhd27oled_bw_hw_spi()`
- `u8g.ssd1325_nhd27oled_gr_hw_spi()`
- `u8g.ssd1327_96x96_gr_hw_spi()`
- `u8g.ssd1351_128x128_332_hw_spi()`
- `u8g.ssd1351_128x128gh_332_hw_spi()`
- `u8g.ssd1351_128x128_hicolor_hw_spi()`
- `u8g.ssd1351_128x128gh_hicolor_hw_spi()`
- `u8g.ssd1353_160x128_332_hw_spi()`
- `u8g.ssd1353_160x128_hicolor_hw_spi()`
- `u8g.st7565_64128n_hw_spi()`
- `u8g.st7565_dogm128_hw_spi()`
- `u8g.st7565_dogm132_hw_spi()`
- `u8g.st7565_lm6059_hw_spi()`
- `u8g.st7565_lm6063_hw_spi()`
- `u8g.st7565_nhd_c12832_hw_spi()`
- `u8g.st7565_nhd_c12864_hw_spi()`
- `u8g.uc1601_c128032_hw_spi()`
- `u8g.uc1608_240x128_hw_spi()`
- `u8g.uc1608_240x64_hw_spi()`
- `u8g.uc1610_dogxl160_bw_hw_spi()`
- `u8g.uc1610_dogxl160_gr_hw_spi()`
- `u8g.uc1611_dogm240_hw_spi()`
- `u8g.uc1611_dogxl240_hw_spi()`
- `u8g.uc1701_dogs102_hw_spi()`
- `u8g.uc1701_mini12864_hw_spi()`

#### Syntax
`u8g.ssd1306_128x64_spi(cs, dc, [res])`

#### Parameters
- `cs` GPIO pin for /CS
- `dc` GPIO pin for DC
- `res` GPIO pin for /RES (optional)

#### Returns
u8g display object

#### Example
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)

cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = u8g.ssd1306_128x64_hw_spi(cs, dc, res)
```

#### See also
[I2C Display Drivers](#i2c-display-drivers)

___

## Constants
`u8g.DRAW_UPPER_RIGHT`, `u8g.DRAW_UPPER_LEFT`, `u8g.DRAW_LOWER_RIGHT`, `u8g.DRAW_LOWER_LEFT`, `u8g.DRAW_ALL`,
`u8g.MODE_BW`, `u8g.MODE_GRAY2BIT`

`u8g.font_6x10`, ...

# u8g.disp Sub-Module

## [u8g.disp:begin()](https://github.com/olikraus/u8glib/wiki/userreference#begin)

## u8g.disp:drawBitmap()
Draw a bitmap at the specified x/y position (upper left corner of the bitmap).
Parts of the bitmap may be outside the display boundaries. The bitmap is specified by the array bitmap. A cleared bit means: Do not draw a pixel. A set bit inside the array means: Write pixel with the current color index. For a monochrome display, the color index 0 will usually clear a pixel and the color index 1 will set a pixel.

#### Syntax
`disp:drawBitmap(x, y, cnt, h, bitmap)`

#### Parameters
- `x` X-position (left position of the bitmap)
- `y` Y-position (upper position of the bitmap)
- `cnt` number of bytes of the bitmap in horizontal direction. The width of the bitmap is cnt*8.
- `h` height of the bitmap
- `bitmap` bitmap data supplied as string

#### Returns
`nil`

#### See also
- [u8glib drawBitmap()](https://github.com/olikraus/u8glib/wiki/userreference#drawbitmap)
- [lua_examples/u8glib/u8g_bitmaps.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_bitmaps.lua)
- [u8g.disp:drawXBM()](#u8gdispdrawxbm)

## [u8g.disp:drawBox()](https://github.com/olikraus/u8glib/wiki/userreference#drawbox)

## [u8g.disp:drawCircle()](https://github.com/olikraus/u8glib/wiki/userreference#drawcircle)

## [u8g.disp:drawDisc()](https://github.com/olikraus/u8glib/wiki/userreference#drawdisc)

## [u8g.disp:drawEllipse()](https://github.com/olikraus/u8glib/wiki/userreference#drawellipse)

## [u8g.disp:drawFilledEllipse()](https://github.com/olikraus/u8glib/wiki/userreference#drawfilledellipse)

## [u8g.disp:drawFrame()](https://github.com/olikraus/u8glib/wiki/userreference#drawframe)

## [u8g.disp:drawHLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawhline)

## [u8g.disp:drawLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawline)

## [u8g.disp:drawPixel()](https://github.com/olikraus/u8glib/wiki/userreference#drawpixel)

## [u8g.disp:drawRBox()](https://github.com/olikraus/u8glib/wiki/userreference#drawrbox)

## [u8g.disp:drawRFrame()](https://github.com/olikraus/u8glib/wiki/userreference#drawrframe)

## [u8g.disp:drawStr()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr)

## [u8g.disp:drawStr90()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr90)

## [u8g.disp:drawStr180()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr180)

## [u8g.disp:drawStr270()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr270)

## [u8g.disp:drawTriangle()](https://github.com/olikraus/u8glib/wiki/userreference#drawtriangle)

## [u8g.disp:drawVLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawvline)

## u8g.disp:drawXBM()
Draw a XBM Bitmap. Position (x,y) is the upper left corner of the bitmap.
XBM contains monochrome, 1-bit bitmaps. This procedure only draws pixel values 1. The current color index is used for drawing (see setColorIndex). Pixel with value 0 are not drawn (transparent).

Bitmaps and XBMs are supplied as strings to `drawBitmap()` and `drawXBM()`. This off-loads all data handling from the u8g module to generic methods for binary files. In contrast to the source code based inclusion of XBMs into u8glib, it's required to provide precompiled binary files. This can be performed online with [Online-Utility's Image Converter](http://www.online-utility.org/image_converter.jsp): Convert from XBM to MONO format and upload the binary result with [nodemcu-uploader.py](https://github.com/kmpm/nodemcu-uploader) or [ESPlorer](http://esp8266.ru/esplorer/).

#### Syntax
`disp:drawXBM(x, y, w, h, bitmap)`

#### Parameters
- `x` X-position (left position of the bitmap)
- `y` Y-position (upper position of the bitmap)
- `w` width of the bitmap
- `h` height of the bitmap
- `bitmap` XBM data supplied as string

#### Returns
`nil`

#### See also
- [u8glib drawXBM()](https://github.com/olikraus/u8glib/wiki/userreference#drawxbm)
- [lua_examples/u8glib/u8g_bitmaps.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_bitmaps.lua)
- [u8g.disp:drawBitmap()](#u8gdispdrawbitmap)

## [u8g.disp:firstPage()](https://github.com/olikraus/u8glib/wiki/userreference#firstpage)
## [u8g.disp:getColorIndex()](https://github.com/olikraus/u8glib/wiki/userreference#getcolorindex)

## [u8g.disp:getFontAscent()](https://github.com/olikraus/u8glib/wiki/userreference#getfontascent)

## [u8g.disp:getFontDescent()](https://github.com/olikraus/u8glib/wiki/userreference#getfontdescent)

## [u8g.disp:getFontLineSpacing()](https://github.com/olikraus/u8glib/wiki/userreference#getfontlinespacing)

## [u8g.disp:getHeight()](https://github.com/olikraus/u8glib/wiki/userreference#getheight)

## [u8g.disp:getMode()](https://github.com/olikraus/u8glib/wiki/userreference#getmode)

## [u8g.disp:getWidth()](https://github.com/olikraus/u8glib/wiki/userreference#getwidth)

## [u8g.disp:getStrWidth()](https://github.com/olikraus/u8glib/wiki/userreference#getstrwidth)

## [u8g.disp:nextPage()](https://github.com/olikraus/u8glib/wiki/userreference#nextpage)

## [u8g.disp:setColorIndex()](https://github.com/olikraus/u8glib/wiki/userreference#setcolortndex)

## [u8g.disp:setDefaultBackgroundColor()](https://github.com/olikraus/u8glib/wiki/userreference#setdefaultbackgroundcolor)

## [u8g.disp:setDefaultForegroundColor()](https://github.com/olikraus/u8glib/wiki/userreference#setdefaultforegroundcolor)

## u8g.disp:setFont()
u8glib comes with a wide range of fonts for small displays.
Since they need to be compiled into the firmware image, you'd need to include them in `app/include/u8g_config.h` and recompile. Simply add the desired fonts to the font table:
```c
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
    U8G_FONT_TABLE_ENTRY(font_chikita)
```
They'll be available as `u8g.<font_name>` in Lua.

#### Syntax
`disp:setFont(font)`

#### Parameters
`font` Constant to indentify pre-compiled font

#### Returns
`nil`

#### Example
```lua
disp:setFont(u8g.font_6x10)
```

#### See also
- [u8glib setFont()](https://github.com/olikraus/u8glib/wiki/userreference#setfont)

## [u8g.disp:setFontLineSpacingFactor()](https://github.com/olikraus/u8glib/wiki/userreference#setfontlinespacingfactor)

## [u8g.disp:setFontPosBaseline()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposbaseline)

## [u8g.disp:setFontPosBottom()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposbottom)

## [u8g.disp:setFontPosCenter()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposcenter)

## [u8g.disp:setFontPosTop()](https://github.com/olikraus/u8glib/wiki/userreference#setfontpostop)

## [u8g.disp:setFontRefHeightAll()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheightall)

## [u8g.disp:setFontRefHeightExtendedText()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheightextendedtext)

## [u8g.disp:setFontRefHeightText()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheighttext)

## [u8g.disp:setRot90()](https://github.com/olikraus/u8glib/wiki/userreference#setrot90)

## [u8g.disp:setRot180()](https://github.com/olikraus/u8glib/wiki/userreference#setrot180)

## [u8g.disp:setRot270()](https://github.com/olikraus/u8glib/wiki/userreference#setrot270)

## [u8g.disp:setScale2x2()](https://github.com/olikraus/u8glib/wiki/userreference#setscale2x2)

## [u8g.disp:sleepOn()](https://github.com/olikraus/u8glib/wiki/userreference#sleepon)

## [u8g.disp:sleepOff()](https://github.com/olikraus/u8glib/wiki/userreference#sleepoff)

## [u8g.disp:undoRotation()](https://github.com/olikraus/u8glib/wiki/userreference#undorotation)

## [u8g.disp:undoScale()](https://github.com/olikraus/u8glib/wiki/userreference#undoscale)

## Unimplemented Functions
- Cursor handling
	- disableCursor()
	- enableCursor()
	- setCursorColor()
	- setCursorFont()
	- setCursorPos()
	- setCursorStyle()
- General functions
	- setContrast()
	- setPrintPos()
	- setHardwareBackup()
	- setRGB()
	- setDefaultMidColor()
