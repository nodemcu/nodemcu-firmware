# u8g Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-30 | [Oli Kraus](https://github.com/olikraus/u8glib), [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [u8glib](../../../app/u8glib/)|

U8glib is a graphics library developed at [olikraus/u8glib](https://github.com/olikraus/u8glib) with support for many different displays. The NodeMCU firmware supports a subset of these.

I²C and SPI mode:

- sh1106_128x64
- ssd1306 - 128x32, 128x64, and 64x48 variants
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

This integration is based on [v1.19.1](https://github.com/olikraus/U8glib_Arduino/releases/tag/1.19.1).

## Overview
### I²C Connection
Hook up SDA and SCL to any free GPIOs. Eg. [u8g_graphics_test.lua](https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/u8glib/u8g_graphics_test.lua) expects SDA=5 (GPIO14) and SCL=6 (GPIO12). They are used to set up nodemcu's I²C driver before accessing the display:
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

SSD1306 via I²C:
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
I²C and HW SPI based displays with support in u8glib can be enabled. To get access to the respective constructors, add the desired entries to the I²C or SPI display tables in [app/include/u8g_config.h](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/include/u8g_config.h):
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

## I²C Display Drivers
Initialize a display via I²C.

The init sequence would insert delays to match the display specs. These can destabilize the overall system if wifi service is blocked for too long. It is therefore advisable to disable such delays unless the specific use case can exclude wifi traffic while initializing the display driver.

- `u8g.sh1106_128x64_i2c()`
- `u8g.ssd1306_128x32_i2c()`
- `u8g.ssd1306_128x64_i2c()`
- `u8g.ssd1306_64x48_i2c()`
- `u8g.ssd1309_128x64_i2c()`
- `u8g.ssd1327_96x96_gr_i2c()`
- `u8g.uc1611_dogm240_i2c()`
- `u8g.uc1611_dogxl240_i2c()`

####Syntax
`u8g.ssd1306_128x64_i2c(address[, use_delay])`

####Parameters
- `address` I²C slave address of display
- `use_delay` '1': use delays in init sequence, '0' if omitted

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

The init sequence would insert delays to match the display specs. These can destabilize the overall system if wifi service is blocked for too long. It is therefore advisable to disable such delays unless the specific use case can exclude wifi traffic while initializing the display driver.

- `u8g.ld7032_60x32_hw_spi()`
- `u8g.pcd8544_84x48_hw_spi()`
- `u8g.pcf8812_96x65_hw_spi()`
- `u8g.sh1106_128x64_hw_spi()`
- `u8g.ssd1306_128x32_hw_spi()`
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
`u8g.ssd1306_128x64_spi(cs, dc[, res[, use_delay]])`

#### Parameters
- `cs` GPIO pin for /CS
- `dc` GPIO pin for DC
- `res` GPIO pin for /RES, none if omitted
- `use_delay` '1': use delays in init sequence, '0' if omitted

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
[I²C Display Drivers](#i2c-display-drivers)

## u8g.fb_rle
Initialize a virtual display that provides run-length encoded framebuffer contents to a Lua callback.

The callback function can be used to process the framebuffer line by line. It's called with either `nil` as parameter to indicate the start of a new frame or with a string containing a line of the framebuffer with run-length encoding. First byte in the string specifies how many pairs of (x, len) follow, while each pair defines the start (leftmost x-coordinate) and length of a sequence of lit pixels. All other pixels in the line are dark.

```lua
n = struct.unpack("B", rle_line)
print(n.." pairs")
for i = 0,n-1 do
  print(string.format("  x: %d len: %d", struct.unpack("BB", rle_line, 1+1 + i*2)))
end
```

#### Syntax
`u8g.fb_rle(cb_fn, width, height)`

#### Parameters
- `cb_fn([rle_line])` callback function. `rle_line` is a string containing a run-length encoded framebuffer line, or `nil` to indicate start of frame.
- `width` of display. Must be a multiple of 8, less than or equal to 248.
- `height` of display. Must be a multiple of 8, less than or equal to 248.

#### Returns
u8g display object

___

## Constants
Constants for various functions.

`u8g.DRAW_UPPER_RIGHT`, `u8g.DRAW_UPPER_LEFT`, `u8g.DRAW_LOWER_RIGHT`, `u8g.DRAW_LOWER_LEFT`, `u8g.DRAW_ALL`,
`u8g.MODE_BW`, `u8g.MODE_GRAY2BIT`

`u8g.font_6x10`, ...

# u8g.disp Sub-Module

## u8g.disp:begin()
See [u8glib begin()](https://github.com/olikraus/u8glib/wiki/userreference#begin).

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

## u8g.disp:drawBox()
See [u8glib drawBox()](https://github.com/olikraus/u8glib/wiki/userreference#drawbox).

## u8g.disp:drawCircle()
See [u8glib drawCircle()](https://github.com/olikraus/u8glib/wiki/userreference#drawcircle).

## u8g.disp:drawDisc()
See [u8glib drawDisc()](https://github.com/olikraus/u8glib/wiki/userreference#drawdisc).

## u8g.disp:drawEllipse()
See [u8glib drawEllipse()](https://github.com/olikraus/u8glib/wiki/userreference#drawellipse).

## u8g.disp:drawFilledEllipse()
See [u8glib drawFilledEllipse](https://github.com/olikraus/u8glib/wiki/userreference#drawfilledellipse).

## u8g.disp:drawFrame()
See [u8glib drawFrame()](https://github.com/olikraus/u8glib/wiki/userreference#drawframe).

## u8g.disp:drawHLine()
See [u8glib drawHLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawhline).

## u8g.disp:drawLine()
See [u8glib drawLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawline).

## u8g.disp:drawPixel()
See [u8glib drawPixel()](https://github.com/olikraus/u8glib/wiki/userreference#drawpixel).

## u8g.disp:drawRBox()
See [u8glib drawRBox()](https://github.com/olikraus/u8glib/wiki/userreference#drawrbox).

## u8g.disp:drawRFrame()
See [u8glib drawRFrame()](https://github.com/olikraus/u8glib/wiki/userreference#drawrframe).

## u8g.disp:drawStr()
See [u8glib drawStr()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr).

## u8g.disp:drawStr90()
See [u8glib drawStr90](https://github.com/olikraus/u8glib/wiki/userreference#drawstr90).

## u8g.disp:drawStr180()
See [u8glib drawStr180()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr180).

## u8g.disp:drawStr270()
See [u8glib drawStr270()](https://github.com/olikraus/u8glib/wiki/userreference#drawstr270).

## u8g.disp:drawTriangle()
See [u8glib drawTriangle()](https://github.com/olikraus/u8glib/wiki/userreference#drawtriangle).

## u8g.disp:drawVLine()
See [u8glib drawVLine()](https://github.com/olikraus/u8glib/wiki/userreference#drawvline).

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

## u8g.disp:firstPage()
See [u8glib firstPage()](https://github.com/olikraus/u8glib/wiki/userreference#firstpage).

## u8g.disp:getColorIndex()
See [u8glib getColorIndex()](https://github.com/olikraus/u8glib/wiki/userreference#getcolorindex).

## u8g.disp:getFontAscent()
See [u8glib getFontAscent()](https://github.com/olikraus/u8glib/wiki/userreference#getfontascent).

## u8g.disp:getFontDescent()
See [u8glib getFontDescent()](https://github.com/olikraus/u8glib/wiki/userreference#getfontdescent).

## u8g.disp:getFontLineSpacing()
See [u8glib getFontLineSpacing()](https://github.com/olikraus/u8glib/wiki/userreference#getfontlinespacing).

## u8g.disp:getHeight()
See [u8glib getHeight()](https://github.com/olikraus/u8glib/wiki/userreference#getheight).

## u8g.disp:getMode()
See [u8glib getMode()](https://github.com/olikraus/u8glib/wiki/userreference#getmode).

## u8g.disp:getWidth()
See [u8glib getWidth()](https://github.com/olikraus/u8glib/wiki/userreference#getwidth).

## u8g.disp:getStrWidth()
See [u8glib getStrWidth](https://github.com/olikraus/u8glib/wiki/userreference#getstrwidth).

## u8g.disp:nextPage()
See [u8glib nextPage()](https://github.com/olikraus/u8glib/wiki/userreference#nextpage).

## u8g.disp:setColorIndex()
See [u8glib setColorIndex()](https://github.com/olikraus/u8glib/wiki/userreference#setcolortndex).

## u8g.disp:setContrast()
See [u8glib setContrast()](https://github.com/olikraus/u8glib/wiki/userreference#setcontrast).

## u8g.disp:setDefaultBackgroundColor()
See [u8glib setDefaultBackgroundColor()](https://github.com/olikraus/u8glib/wiki/userreference#setdefaultbackgroundcolor).

## u8g.disp:setDefaultForegroundColor()
See [u8glib setDefaultForegroundColor()](https://github.com/olikraus/u8glib/wiki/userreference#setdefaultforegroundcolor).

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

## u8g.disp:setFontLineSpacingFactor()
See [u8glib setFontLineSpacingFactor()](https://github.com/olikraus/u8glib/wiki/userreference#setfontlinespacingfactor).

## u8g.disp:setFontPosBaseline()
See [u8glib setFontPosBaseline()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposbaseline).

## u8g.disp:setFontPosBottom()
See [u8glib setFontPosBottom()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposbottom).

## u8g.disp:setFontPosCenter()
See [u8glib setFontPosCenter()](https://github.com/olikraus/u8glib/wiki/userreference#setfontposcenter).

## u8g.disp:setFontPosTop()
See [u8glib setFontPosTop()](https://github.com/olikraus/u8glib/wiki/userreference#setfontpostop).

## u8g.disp:setFontRefHeightAll()
See [u8glib setFontRefHeightAll()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheightall).

## u8g.disp:setFontRefHeightExtendedText()
See [u8glib setFontRefHeightExtendedText()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheightextendedtext).

## u8g.disp:setFontRefHeightText()
See [u8glib setFontRefHeightText()](https://github.com/olikraus/u8glib/wiki/userreference#setfontrefheighttext).

## u8g.disp:setRot90()
See [u8glib setRot90()](https://github.com/olikraus/u8glib/wiki/userreference#setrot90).

## u8g.disp:setRot180()
See [u8glib setRot180()](https://github.com/olikraus/u8glib/wiki/userreference#setrot180).

## u8g.disp:setRot270()
See [u8glib setRot270()](https://github.com/olikraus/u8glib/wiki/userreference#setrot270).

## u8g.disp:setScale2x2()
See [u8glib setScale2x2()](https://github.com/olikraus/u8glib/wiki/userreference#setscale2x2).

## u8g.disp:sleepOn()
See [u8glib sleepOn()](https://github.com/olikraus/u8glib/wiki/userreference#sleepon).

## u8g.disp:sleepOff()
See [u8glib sleepOff()](https://github.com/olikraus/u8glib/wiki/userreference#sleepoff).

## u8g.disp:undoRotation()
See [u8glib undoRotation()](https://github.com/olikraus/u8glib/wiki/userreference#undorotation).

## u8g.disp:undoScale()
See [u8glib undoScale()](https://github.com/olikraus/u8glib/wiki/userreference#undoscale).

## Unimplemented Functions
- Cursor handling
	- disableCursor()
	- enableCursor()
	- setCursorColor()
	- setCursorFont()
	- setCursorPos()
	- setCursorStyle()
- General functions
	- setPrintPos()
	- setHardwareBackup()
	- setRGB()
	- setDefaultMidColor()
