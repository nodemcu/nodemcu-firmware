# u8g2.disp Sub-Module

## u8g2.disp:clearBuffer()
Clears all pixel in the memory frame buffer.

See [u8g2 clearBuffer()](https://github.com/olikraus/u8g2/wiki/u8g2reference#clearbuffer).

## u8g2.disp:drawBox()
Draw a box (filled frame).

See [u8g2 drawBox()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawbox).

## u8g2.disp:drawCircle()
Draw a circle.

See [u8g2 drawCircle()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawcircle).

Note that parameter `opt` is optional and defaults to `u8g2.DRAW_ALL` if omitted.

## u8g2.disp:drawDisc()
Draw a filled circle.

See [u8g2 drawDisc()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawdisc).

Note that parameter `opt` is optional and defaults to `u8g2.DRAW_ALL` if omitted.

## u8g2.disp:drawEllipse()
Draw an ellipse.

See [u8g2 drawEllipse()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawellipse).

Note that parameter `opt` is optional and defaults to `u8g2.DRAW_ALL` if omitted.

## u8g2.disp:drawFilledEllipse()
Draw a filled ellipse.

See [u8g2 drawFilledEllipse()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawfilledellipse).

Note that parameter `opt` is optional and defaults to `u8g2.DRAW_ALL` if omitted.

## u8g2.disp:drawFrame()
Draw a frame (empty box).

See [u8g2 drawFrame()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawframe).

## u8g2.disp:drawGlyph()
Draw a single character.

See [u8g2 drawGlyph()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawglyph).

## u8g2.disp:drawHLine()
Draw a horizontal line.

See [u8g2 drawHLine()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawhline).

## u8g2.disp:drawLine()
Draw a line between two points.

See [u8g2 drawLine()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawline).

## u8g2.disp:drawPixel()
Draw a pixel.

See [u8g2 drawPixel()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawpixel).

## u8g2.disp:drawRBox()
Draw a box with round edges.

See [u8g2 drawRBox()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawrbox).

## u8g2.disp:drawRFrame()
Draw a frame with round edges.

See [u8g2 drawRFrame()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawrframe).

## u8g2.disp:drawStr()
Draw a string.

See [u8g2 drawStr()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawstr).

## u8g2.disp:drawTriangle()
Draw a triangle (filled polygon).

See [u8g2 drawTriangle()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawtriangle).

## u8g2.disp:drawUTF8()
Draw a string which is encoded as UTF-8.

See [u8g2 drawUTF8()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawutf8).

## u8g2.disp:drawVLine()
Draw a vertical line.

See [u8g2 drawVLine()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawvline).

## u8g2.disp:drawXBM()
Draw a XBM Bitmap.

See [u8g2 drawXBM()](https://github.com/olikraus/u8g2/wiki/u8g2reference#drawxbm).

XBM bitmaps are supplied as strings to `drawXBM()`. This off-loads all data handling from the u8g2 module to generic methods for binary files. See [graphics_test.lua](../../../lua_examples/u8glib/u8g_graphics_test.lua).

In contrast to the source code based inclusion of XBMs in upstream u8g2 library, it's required to provide precompiled binary files. This can be performed online with [Online-Utility's Image Converter](http://www.online-utility.org/image_converter.jsp): Convert from XBM to MONO format and upload the binary result.

## u8g2.disp:getAscent()
Returns the reference height of the glyphs above the baseline (ascent).

See [u8g2 getAscent()](https://github.com/olikraus/u8g2/wiki/u8g2reference#getascent).

## u8g2.disp:getDescent()
Returns the reference height of the glyphs below the baseline (descent).

See [u8g2 getDescent()](https://github.com/olikraus/u8g2/wiki/u8g2reference#getdescent).

## u8g2.disp:getStrWidth()
Return the pixel width of string.

See [u8g2 getStrWidth()](https://github.com/olikraus/u8g2/wiki/u8g2reference#getstrwidth).

## u8g2.disp:getUTF8Width()
Return the pixel width of an UTF-8 encoded string.

See [u8g2 getUTF8Width()](https://github.com/olikraus/u8g2/wiki/u8g2reference#getutf8width).

## u8g2.disp:sendBuffer()
Send the content of the memory frame buffer to the display.

See [u8g2 sendBuffer()](https://github.com/olikraus/u8g2/wiki/u8g2reference#sendbuffer).

## u8g2.disp:setBitmapMode()
Define bitmap background color mode.

See [u8g2 setBitmapMode()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setbitmapmode).

## u8g2.disp:setContrast()
Set the contrast or brightness.

See [u8g2 setContrast()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setconstrast).

## u8g2.disp:setDisplayRotation()
Changes the display rotation.

See [u8g2 setDisplayRotation()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setdisplayrotation).

## u8g2.disp:setDrawColor()
Defines the bit value (color index) for all drawing functions.

See [u8g2 setDrawColor()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setdrawcolor).

## u8g2.disp:setFlipMode()
Set flip (180 degree rotation) mode.

See [u8g2 setFlipMode()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setflipmode).

## u8g2.disp:setFont()
Define a u8g2 font for the glyph and string drawing functions. They can be supplied as strings or compiled into the firmware image. They are available as `u8g2.<font_name>` in Lua.

See [u8g2 setFont()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfont).

## u8g2.disp:setFontDirection()
Set the drawing direction of all strings or glyphs.

See [u8g2 setFontDirection()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontdirection).

## u8g2.disp:setFontMode()
Define font background color mode.

See [u8g2 setFontMode()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontmode).

## u8g2.disp:setFontPosBaseline()
Change the reference position for the glyph and string draw functions to "baseline".

See [u8g2 setFontPosBaseline()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontposbaseline).

## u8g2.disp:setFontPosBottom()
Change the reference position for the glyph and string draw functions to "bottom".

See [u8g2 setFontPosBottom()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontposbottom).

## u8g2.disp:setFontPosTop()
Change the reference position for the glyph and string draw functions to "top".

See [u8g2 setFontPosTop()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontpostop).

## u8g2.disp:setFontPosCenter()
Change the reference position for the glyph and string draw functions to "center".

See [u8g2 setFontPosCenter()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontposcenter).

## u8g2.disp:setFontRefHeightAll()
Set ascent and descent calculation mode to "highest and lowest glyph".

See [u8g2 setFontRefHeightAll()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontrefheightall).

## u8g2.disp:setFontRefHeightExtendedText()
Set ascent and descent calculation mode to "highest of [A1(], lowest of [g(]".

See [u8g2 setFontRefHeightExtendedText()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontrefheightextendedtext).

## u8g2.disp:setFontRefHeightText()
Set ascent and descent calculation mode to "highest of [A1], lowest of [g]".

See [u8g2 setFontRefHeightText()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setfontrefheighttext).

## u8g2.disp:setPowerSave()
Activate or disable power save mode of the display.

See [u8g2 setPowerSave()](https://github.com/olikraus/u8g2/wiki/u8g2reference#setpowersave).
