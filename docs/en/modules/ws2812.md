# WS2812 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-02-05 | [Till Klocke](https://github.com/dereulenspiegel), [Thomas Soëte](https://github.com/Alkorin) | [Till Klocke](https://github.com/dereulenspiegel) | [ws2812.c](../../../app/modules/ws2812.c)|

ws2812 is a library to handle ws2812-like led strips.
It works at least on WS2812, WS2812b, APA104, SK6812 (RGB or RGBW).

The library uses UART1 routed on GPIO2 (Pin D4 on NodeMCU DEVKIT) to
generate the bitstream. It can use UART0 routed to TXD0 as well to
handle two led strips at the same time.

**WARNING**: In dual mode, you will loose access to the Lua's console
through the serial port (it will be reconfigured to support WS2812-like 
protocol). If you want to keep access to Lua's console, you will have to 
use an other input channel like a TCP server (see [example](https://github.com/nodemcu/nodemcu-firmware/blob/master/examples/telnet.lua))

## ws2812.init(mode)
Initialize UART1 and GPIO2, should be called once and before write().
Initialize UART0 (TXD0) too if `ws2812.MODE_DUAL` is set.

#### Parameters
- `mode` (optional) either `ws2812.MODE_SINGLE` (default if omitted) or `ws2812.MODE_DUAL`.
In `ws2812.MODE_DUAL` mode you will be able to handle two strips in parallel but will lose access
to Lua's serial console as it shares the same UART and PIN.

#### Returns
`nil`

## ws2812.write()
Send data to one or two led strip using its native format which is generally Green,Red,Blue for RGB strips
and Green,Red,Blue,White for RGBW strips.

#### Syntax
`ws2812.write(data1, [data2])`

#### Parameters
- `data1` payload to be sent to one or more WS2812 like leds through GPIO2
- `data2` (optional) payload to be sent to one or more WS2812 like leds through TXD0 (`ws2812.MODE_DUAL` mode required)

Payload type could be:
- `nil` nothing is done
- `string` representing bytes to send
- `ws2812.buffer` see [Buffer module](#buffer-module)

#### Returns
`nil`

#### Example
```lua
ws2812.init()
ws2812.write(string.char(255, 0, 0, 255, 0, 0)) -- turn the two first RGB leds to green
```

```lua
ws2812.init()
ws2812.write(string.char(0, 0, 0, 255, 0, 0, 0, 255)) -- turn the two first RGBW leds to white
```

```lua
ws2812.init(ws2812.MODE_DUAL)
ws2812.write(string.char(255, 0, 0, 255, 0, 0), string.char(0, 255, 0, 0, 255, 0)) -- turn the two first RGB leds to green on the first strip and red on the second strip
```

```lua
ws2812.init(ws2812.MODE_DUAL)
ws2812.write(nil, string.char(0, 255, 0, 0, 255, 0)) -- turn the two first RGB leds to red on the second strip, do nothing on the first
```

# Buffer module
For more advanced animations, it is useful to keep a "framebuffer" of the strip,
interact with it and flush it to the strip.

For this purpose, the ws2812 library offers a read/write buffer.

#### Example
Led chaser with a RGBW strip
```lua
ws2812.init()
local i, buffer = 0, ws2812.newBuffer(300, 4); buffer:fill(0, 0, 0, 0); tmr.alarm(0, 50, 1, function()
        i=i+1
        buffer:fade(2)
        buffer:set(i%buffer:size()+1, 0, 0, 0, 255)
        ws2812.write(buffer)
end)
```

## ws2812.newBuffer()
Allocate a new memory buffer to store led values.

#### Syntax
`ws2812.newBuffer(numberOfLeds, bytesPerLed)`

#### Parameters
 - `numberOfLeds` length of the led strip
 - `bytesPerLed` 3 for RGB strips and 4 for RGBW strips

#### Returns
`ws2812.buffer`

## ws2812.buffer:get()
Return the value at the given position

#### Syntax
`buffer:get(index)`

#### Parameters
 - `index` position in the buffer (1 for first led)

#### Returns
`(color)`

#### Example
```lua
buffer = ws2812.newBuffer(32, 4)
print(buffer:get(1))
0	0	0	0
```

## ws2812.buffer:set()
Set the value at the given position

#### Syntax
`buffer:set(index, color)`

#### Parameters
 - `index` position in the buffer (1 for the first led)
 - `color` payload of the color

Payload could be:
- `number, number, ...` you should pass as many arguments as `bytesPerLed`
- `table` should contains `bytesPerLed` numbers
- `string` should contains `bytesPerLed` bytes

#### Returns
`nil`

#### Example
```lua
buffer = ws2812.newBuffer(32, 3)
buffer:set(1, 255, 0, 0) -- set the first led green for a RGB strip
```

```lua
buffer = ws2812.newBuffer(32, 4)
buffer:set(1, {0, 0, 0, 255}) -- set the first led white for a RGBW strip
```

```lua
buffer = ws2812.newBuffer(32, 3)
buffer:set(1, string.char(255, 0, 0)) -- set the first led green for a RGB strip
```

## ws2812.buffer:size()
Return the size of the buffer in number of leds

#### Syntax
`buffer:size()`

#### Parameters
none

#### Returns
`int`

## ws2812.buffer:fill()
Fill the buffer with the given color.
The number of given bytes must match the number of bytesPerLed of the buffer

#### Syntax
`buffer:fill(color)`

#### Parameters
 - `color` bytes of the color, you should pass as many arguments as `bytesPerLed`

#### Returns
`nil`

#### Example
```lua
buffer:fill(0, 0, 0) -- fill the buffer with black for a RGB strip
```

## ws2812.buffer:fade()
Fade in or out. Defaults to out. Multiply or divide each byte of each led with/by the given value. Useful for a fading effect. 

#### Syntax
`buffer:fade(value [, direction])`

#### Parameters
 - `value` value by which to divide or multiply each byte
 - `direction` ws2812.FADE\_IN or ws2812.FADE\_OUT. Defaults to ws2812.FADE\_OUT

#### Returns
`nil`

#### Example
```lua
buffer:fade(2)
buffer:fade(2, ws2812.FADE_IN)
```
## ws2812.buffer:shift()
Shift the content of the buffer in positive or negative direction. This allows simple animation effects.

#### Syntax
`buffer:shift(value [, mode])`

#### Parameters
 - `value` number of pixels by which to rotate the buffer. Positive values rotate forwards, negative values backwards. 
 - `mode` is the shift mode to use. Can be one of `ws2812.SHIFT_LOGICAL` or `ws2812.SHIFT_CIRCULAR`. In case of SHIFT\_LOGICAL, the freed pixels are set to 0 (off). In case of SHIFT\_CIRCULAR, the buffer is treated like a ring buffer, inserting the pixels falling out on one end again on the other end. Defaults to SHIFT\_LOGICAL. 

#### Returns
`nil`

#### Example
```lua
buffer:shift(3)
```
