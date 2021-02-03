# Pixel Buffer (pixbuf) Module

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-?? | [nwf](https://github.com/nwf) | nwf | [pixbuf.c](../../app/modules/pixbuf.c) |

The pixbuf library offers C-array byte objects and convenient utility functions
for maintaining small frame buffers, usually for use with LED arrays, as
supported by, e.g., ws2812.

## pixbuf.newBuffer()
Allocate a new memory buffer to store LED values.

#### Syntax
`pixbuf.newBuffer(numberOfLeds, numberOfChannels)`

#### Parameters
 - `numberOfLeds` length of the LED strip (in pixels)
 - `numberOfChannels` the channel count (bytes per pixel)

#### Returns
`pixbuf.buffer` object

## pixbuf.buffer:get()
Return the value at the given position, in native strip color order.

#### Syntax
`buffer:get(index)`

#### Parameters
 - `index` position in the buffer (1 for first LED)

#### Returns
`(color)`

#### Example
```lua
buffer = pixbuf.newBuffer(32, 4)
print(buffer:get(1))
0	0	0	0
```

## pixbuf.buffer:set()
Set the value at the given position, in native strip color order.

#### Syntax
`buffer:set(index, color)`

#### Parameters
 - `index` position in the buffer (1 for the first LED)
 - `color` payload of the color

Payload could be:
- `number, number, ...`, passing as many colors as required by the array type
- `table` should contain one value per color required by the array type
- `string` with a natural multiple of the colors required by the array type

`string` inputs may be used to set multiple consecutive pixels!

#### Returns
The buffer

#### Example
```lua
buffer = pixbuf.newBuffer(32, 3)
buffer:set(1, 255, 0, 0) -- set the first LED green for a GRB strip
```

```lua
buffer = pixbuf.newBuffer(32, 4)
buffer:set(1, {255, 0, 0, 255}) -- set the first LED white and red for a RGBW strip
```

```lua
-- set the first LED green for a RGB strip and exploit the return value
buffer = pixbuf.newBuffer(32, 3):set(1, string.char(0, 255, 0))
```

## pixbuf.buffer:size()
Return the size of the buffer in number of LEDs.

#### Syntax
`buffer:size()`

#### Parameters
none

#### Returns
`int`

## pixbuf.buffer:channels()
Return the buffer's channel count.

#### Syntax
`buffer:channels()`

#### Parameters
none

#### Returns
`int`

## pixbuf.buffer:fill()
Fill the buffer with the given color.
The number of given bytes must match the channel count of the buffer.

#### Syntax
`buffer:fill(color)`

#### Parameters
 - `color` bytes for each channel

#### Returns
The buffer

#### Example
```lua
buffer:fill(0, 0, 0) -- fill the buffer with black for a RGB strip
```

## pixbuf.buffer:dump()
Returns the contents of the buffer (the pixel values) as a string. This can then be saved to a file or sent over a network and may be fed back to [`pixbuf.buffer:set()`](#pixbufbufferset).

#### Syntax
`buffer:dump()`

#### Returns
A string containing the pixel values.

#### Example
```lua
local s = buffer:dump()
```

## pixbuf.buffer:replace()
Inserts a string (or a pixbuf) into another buffer with an offset.
The buffer must be of the same type or an error will be thrown.

#### Syntax
`buffer:replace(source[, offset])`

#### Parameters
 - `source` the pixel values to be set into the buffer. This is either a string or a pixbuf.
 - `offset` the offset where the source is to be placed in the buffer. Default is 1. Negative values can be used.

#### Returns
`nil`

#### Example
```lua
buffer:replace(anotherbuffer:dump()) -- copy one buffer into another via a string
buffer:replace(anotherbuffer) -- copy one buffer into another
newbuffer = buffer.sub(1)     -- make a copy of a buffer into a new buffer
```

## pixbuf.buffer:mix()
This is a general method that loads data into a buffer that is a linear combination of data from other buffers. It can be used to copy a buffer or,
more usefully, do a cross fade. The pixel values are computed as integers and then range limited to [0, 255]. This means that negative
factors work as expected, and that the order of combining buffers does not matter.

#### Syntax
`buffer:mix(factor1, buffer1, ...)`

#### Parameters
 - `factor1` This is the factor that the contents of `buffer1` are multiplied by. This factor is scaled by a factor of 256. Thus `factor1` value of 256 is a factor of 1.0.
 - `buffer1` This is the source buffer. It must be of the same shape as the destination buffer.

There can be any number of factor/buffer pairs.

#### Returns
The output buffer.

#### Example
```lua
-- loads buffer with a crossfade between buffer1 and buffer2
buffer:mix(256 - crossmix, buffer1, crossmix, buffer2)

-- multiplies all values in buffer by 0.75
-- This can be used in place of buffer:fade
buffer:mix(192, buffer)
```

## pixbuf.buffer:mix4I5()
Like [`pixbuf.buffer:mix()`](#pixbufbuffermix) but treats the first channel as
a scaling, 5-bit intensity value.  The buffers must all have four channels.
This is mostly useful for APA102 LEDs.

## pixbuf.buffer:power()
Computes the total energy requirement for the buffer. This is merely the total sum of all the pixel values (which assumes that each color in each
pixel consumes the same amount of power). A real WS2812 (or WS2811) has three constant current drivers of 20mA -- one for each of R, G and B. The
pulse width modulation will cause the *average* current to scale linearly with pixel value.

#### Syntax
`buffer:power()`

#### Returns
An integer which is the sum of all the pixel values.

#### Example
```lua
-- Dim the buffer to no more than the PSU can provide
local psu_current_ma = 1000
local led_current_ma = 20
local led_sum = psu_current_ma * 255 / led_current_ma

local p = buffer:power()
if p > led_sum then
  buffer:mix(256 * led_sum / p, buffer) -- power is now limited
end
```

## pixbuf.buffer:powerI()
Like [`pixbuf.buffer:power()`](#pixbufbufferpower) but treats the first channel as
a scaling intensity value.

## pixbuf.buffer:fade()
Fade in or out. Defaults to out. Multiply or divide each byte of each led with/by the given value. Useful for a fading effect.

#### Syntax
`buffer:fade(value [, direction])`

#### Parameters
 - `value` value by which to divide or multiply each byte
 - `direction` pixbuf.FADE\_IN or pixbuf.FADE\_OUT. Defaults to pixbuf.FADE\_OUT

#### Returns
`nil`

#### Example
```lua
buffer:fade(2)
buffer:fade(2, pixbuf.FADE_IN)
```

## pixbuf.buffer:fadeI()
Like [`pixbuf.buffer:fade()`](#pixbufbufferfade) but treats the first channel as
a scaling intensity value.  This is mostly useful for APA102 LEDs.

## pixbuf.buffer:shift()
Shift the content of (a piece of) the buffer in positive or negative direction. This allows simple animation effects. A slice of the buffer can be specified by using the
standard start and end offset Lua notation. Negative values count backwards from the end of the buffer.

#### Syntax
`buffer:shift(value [, mode[, i[, j]]])`

#### Parameters
 - `value` number of pixels by which to rotate the buffer. Positive values rotate forwards, negative values backwards.
 - `mode` is the shift mode to use. Can be one of `pixbuf.SHIFT_LOGICAL` or `pixbuf.SHIFT_CIRCULAR`. In case of SHIFT\_LOGICAL, the freed pixels are set to 0 (off). In case of SHIFT\_CIRCULAR, the buffer is treated like a ring buffer, inserting the pixels falling out on one end again on the other end. Defaults to SHIFT\_LOGICAL.
 - `i` is the first offset in the buffer to be affected. Negative values are permitted and count backwards from the end. Default is 1.
 - `j` is the last offset in the buffer to be affected. Negative values are permitted and count backwards from the end. Default is -1.

#### Returns
`nil`

#### Example
```lua
buffer:shift(3)
```

## pixbuf.buffer:sub()
This implements the extraction function like `string.sub`. The indexes are in leds and all the same rules apply.

#### Syntax
`buffer1:sub(i[, j])`

#### Parameters
 - `i` This is the start of the extracted data. Negative values can be used.
 - `j` this is the end of the extracted data. Negative values can be used. The default is -1.

#### Returns
A buffer containing the extracted piece.

#### Example
```
b = buffer:sub(1,10)
```

## pixbuf.buffer:__concat()
This implements the `..` operator to concatenate two buffers. They must have the same number of colors per led.

#### Syntax
`buffer1 .. buffer2`

#### Parameters
 - `buffer1` this is the start of the resulting buffer
 - `buffer2` this is the end of the resulting buffer

#### Returns
The concatenated buffer.

#### Example
```
ws2812.write(buffer1 .. buffer2)
```
## pixbuf.buffer:map()
Map a function across each pixel of one, or zip a function along two,
pixbuf(s), storing into the buffer on which it is called.

#### Syntax
`buffer0:map(f, [buffer1], [start1], [end1], [buffer2, [start2]])`

#### Parameters
 - `f` This is the mapping function; it is applied for each pixel to all channels of `buffer1` and
   all channels of `buffer2`, if given.  It must return a value for each channel of the output
   buffer, `buffer0`.
 - `buffer1` The first source buffer.  Defaults to `buffer0`.
 - `start1` This is the start of the mapped range of `buffer1`. Negative values can be used and will be interpreted as before the end of `buffer1`.  The default is 1.
 - `end1` this is the end of the mapped range. Negative values can be used. The default is -1 (i.e., the end of `buffer1`).
 - `buffer2` is a second buffer, for zip operations
 - `start2` This is the start of the mapped range within `buffer2`.  Negative values can be used and will be interpreted as before the end of `buffer2`.  The default is 1.

`buffer0` must have sufficient room to recieve all pixels from `start1` to
`end1` (which is true of the defaults, when `buffer1` is `buffer0` and `start1`
is 1 and `end1` is -1).  `buffer2`, if given, must have sufficient pixels after
`start2`.

#### Returns
`buffer0`

#### Examples

Change channel order within a single buffer:
```Lua
buffer:map(function(r,g,b) return g,r,b end)
```

Change channel order for a subset of pixels:
```Lua
buffer:map(function(r,g,b) return g,r,b end, nil, 2, 5)
```

Extract one channel for a subset of pixels:
```Lua
outbuf = pixbuf.create(11, 1)
outbuf:map(function(r,g,b) return b end, inbuf, 10, 20)
```

Concatenate channels per pixel, possibly with different offsets in buffers:
```Lua
outbuf:map(function(...) return ... end, inbuf1, inbuf2)
outbuf:map(function(...) return ... end, inbuf1, 5, 10, inbuf2, 3)
```
