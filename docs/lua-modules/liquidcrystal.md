# LiquidCrystal Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-12-01 | [Matsievskiy Sergey](https://github.com/seregaxvm) | [Matsievskiy Sergey](https://github.com/seregaxvm) | [liquidcrystal.lua](../../lua_modules/liquidcrystal/liquidcrystal.lua) [i2c4bit.lua](../../lua_modules/liquidcrystal/lc-i2c4bit.lua) [gpio4bit.lua](../../lua_modules/liquidcrystal/lc-gpio4bit.lua) [gpio8bit.lua](../../lua_modules/liquidcrystal/lc-gpio8bit.lua) |

This Lua module provides access to [Hitachi HD44780](https://www.sparkfun.com/datasheets/LCD/HD44780.pdf) based LCDs. It supports 4 bit and 8 bit GPIO interface, 4 bit [PCF8574](https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf) based I²C interface.

!!! note
	This module requires `bit` C module built into firmware. Depending on the interface, `gpio` or `i2c` module is also required.

## Program example

In this example LED screen is connected using I²C GPIO expander.
Program defines five custom characters and prints text.

```lua
backend_meta = require "lc-i2c4bit"
lc_meta = require "liquidcrystal"

-- create display object
lc = lc_meta(backend_meta{sda=1, scl=2}, false, true, 20)
backend_meta = nil
lc_meta = nil
-- define custom characters
lc:customChar(0, {0,14,31,31,4,4,5,2})
lc:customChar(1, {4,6,5,5,4,12,28,8})
lc:customChar(2, {14,31,17,17,17,17,17,31})
lc:customChar(3, {14,31,17,17,17,17,31,31})
lc:customChar(4, {14,31,17,17,31,31,31,31})
lc:customChar(5, {14,31,31,31,31,31,31,31})
lc:clear() -- clear display
lc:blink(true) -- enable cursor blinking
lc:home() -- reset cursor position
lc:write("hello", " ", "world") -- write string
lc:cursorMove(1, 2) -- move cursor to second line
lc:write("umbrella", 0, 32, "note", 1) -- mix text strings and characters
lc:cursorMove(1, 3)
lc:write("Battery level ", 2, 3, 4, 5)
lc:home()
lc:blink(false)
for i=1,20 do print(lc:read()) end -- read back first line
lc:home()
for _, d in ipairs(lc:readCustom(0)) do print(d) end -- read back umbrella char
for _, d in ipairs(lc:readCustom(1)) do print(d) end -- read back note char
```

### Require
```lua
i2c4bit_meta = require("lc-i2c4bit")
gpio4bit_meta = require("lc-gpio4bit")
gpio8bit_meta = require("lc-gpio8bit")
lc_meta = require("liquidcrystal")
```

### Release
```lua
package.loaded["lc-i2c4bit"] = nil
package.loaded["lc-gpio4bit"] = nil
package.loaded["lc-gpio8bit"] = nil
package.loaded["liquidcrystal"] = nil
```

## Initialization

Liquidcrystal module is initialized using closure, which takes backend object as an argument.

### I²C backend

Loading I²C backend module returns initialization closure.
It configures I²C backend and returns backend object.

#### Syntax
`function({[sda=sda_pin] [, scl=scl_pin] [, busid=id] [, busad=address] [, speed = spd] [, rs = rs_pos] [, rw = rw_pos] [, en = en_pos] [, bl = bl_pos] [, d4 = d4_pos] [, d5 = d5_pos] [, d6 = d6_pos] [, d7 = d7_pos]})`

!!! note
	In most cases only `sda` and `scl` parameters are required

#### Parameters
- `sda`: I²C data pin. If set to `nil`, I²C bus initialization step via [`i2c.setup`](https://nodemcu.readthedocs.io/en/master/modules/i2c/#i2csetup) will be skipped
- `scl`: I²C clock pin. If set to `nil`, I²C bus initialization step via [`i2c.setup`](https://nodemcu.readthedocs.io/en/master/modules/i2c/#i2csetup) will be skipped
- `busid`: I²C bus ID. Defaults to `0`
- `busad`: chip I²C address. Defaults to `0x27` (default PCF8574 address)
- `speed`: I²C speed. Defaults to `i2c.SLOW`
- `rs`: bit position assigned to `RS` pin in I²C word. Defaults to 0
- `rw`: bit position assigned to `RW` pin in I²C word. Defaults to 1
- `en`: bit position assigned to `EN` pin in I²C word. Defaults to 2
- `bl`: bit position assigned to backlight pin in I²C word. Defaults to 3
- `d4`: bit position assigned to `D4` pin in I²C word. Defaults to 4
- `d5`: bit position assigned to `D5` pin in I²C word. Defaults to 5
- `d6`: bit position assigned to `D6` pin in I²C word. Defaults to 6
- `d7`: bit position assigned to `D7` pin in I²C word. Defaults to 7

#### Returns
- backend object

#### Example
```lua
backend_meta = require "lc-i2c4bit"
backend = backend_meta{sda=1, scl=2 ,speed=i2c.FAST}
```

### GPIO 4 bit backend

Loading GPIO 4 bit backend module returns initialization closure.
It configures GPIO 4 bit backend and returns backend object.

#### Syntax
`function({[, rs = rs_pos] [, rw = rw_pos] [, en = en_pos] [, bl = bl_pos] [, d4 = d4_pos] [, d5 = d5_pos] [, d6 = d6_pos] [, d7 = d7_pos]})`

#### Parameters
- `rs`: GPIO pin connected to `RS` pin. Defaults to 0
- `rw`: GPIO pin connected to `RW` pin. If set to `nil` then `busy`, `position` and `readChar` functions will not be available. Note that `RW` pin must be pulled to the ground if not connected to GPIO
- `en`: GPIO pin connected to `EN` pin. Defaults to 1
- `bl`: GPIO pin controlling backlight. It is assumed, that high level turns backlight on, low level turns backlight off. If set to `nil` then backlight function will not be available
- `d4`: GPIO pin connected to `D4` pin. Defaults to 2
- `d5`: GPIO pin connected to `D5` pin. Defaults to 3
- `d6`: GPIO pin connected to `D6` pin. Defaults to 4
- `d7`: GPIO pin connected to `D7` pin. Defaults to 5

#### Returns
- backend object

#### Example
```lua
backend_meta = require "lc-gpio4bit"
backend = backend_meta{rs=0, rw=1, en=4, d4=5, d5=6, d6=7, d7=8}
```

### GPIO 8 bit backend

Loading GPIO 8 bit backend module returns initialization closure.
It configures GPIO 8 bit backend and returns backend object.

#### Syntax
`function({[, rs = rs_pos] [, rw = rw_pos] [, en = en_pos] [, bl = bl_pos] [, d0 = d0_pos] [, d1 = d1_pos] [, d2 = d2_pos] [, d3 = d3_pos] [, d4 = d4_pos] [, d5 = d5_pos] [, d6 = d6_pos] [, d7 = d7_pos]})`

#### Parameters
- `rs`: GPIO pin connected to `RS` pin. Defaults to 0
- `rw`: GPIO pin connected to `RW` pin. If set to `nil` then `busy`, `position` and `readChar` functions will not be available. Note that `RW` pin must be pulled to the ground if not connected to GPIO
- `en`: GPIO pin connected to `EN` pin. Defaults to 1
- `bl`: GPIO pin controlling backlight. It is assumed, that high level turns backlight on, low level turns backlight off. If set to `nil` then backlight function will not be available
- `d0`: GPIO pin connected to `D0` pin. Defaults to 2
- `d1`: GPIO pin connected to `D1` pin. Defaults to 3
- `d2`: GPIO pin connected to `D2` pin. Defaults to 4
- `d3`: GPIO pin connected to `D3` pin. Defaults to 5
- `d4`: GPIO pin connected to `D4` pin. Defaults to 6
- `d5`: GPIO pin connected to `D5` pin. Defaults to 7
- `d6`: GPIO pin connected to `D6` pin. Defaults to 8
- `d7`: GPIO pin connected to `D7` pin. Defaults to 9

#### Returns
- backend object

#### Example
```lua
backend_meta = require "lc-gpio8bit"
backend = backend_meta{rs=15, rw=2, en=5, d0=23, d1=13, d2=33, d3=32, d4=18, d5=19, d6=21, d7=22}
```

### Liquidcrystal initialization

Loading Liquidcrystal module returns initialization closure.
It requires backend object and returns LCD object.

#### Syntax
`function(backend, onelinemode, eightdotsmode, column_width)`

#### Parameters
- `backend`: backend object
- `onelinemode`: `true` to use one line mode, `false` to use two line mode
- `eightdotsmode`: `true` to use 5x8 dot font, `false` to use 5x10 dot font
- `column_width`: number of characters in column. Used for offset calculations in function `cursorMove`. If set to `nil`, functionality of `cursorMove` will be limited. For most displays column width is `20` characters

#### Returns
screen object

#### Example
```lua
lc_meta = require "liquidcrystal"
lc = lc_meta(backend, true, true, 20)
```

## liquidcrystal.autoscroll
Autoscroll text when printing. When turned off, cursor moves and text stays still, when turned on, vice versa.

#### Syntax
`liquidcrystal.autoscroll(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance
- `on`: `true` to turn on, `false` to turn off

#### Returns
- sent data

#### Example
```lua
liquidcrystal:autoscroll(true)
```

## liquidcrystal.backlight
Control LCDs backlight. When using GPIO backend without `bl` argument specification function does nothing.

#### Syntax
`liquidcrystal.backlight(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance
- `on`: `true` to turn on, `false` to turn off

#### Returns
- backlight status

#### Example
```lua
liquidcrystal:backlight(true)
```

## liquidcrystal.blink
Control cursors blink mode.

#### Syntax
`liquidcrystal.blink(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance
- `on`: `true` to turn on, `false` to turn off

#### Returns
- sent data

#### Example
```lua
liquidcrystal:blink(true)
```

## liquidcrystal.busy
Get busy status of the LCD. When using GPIO backend without `rw` argument specification function does nothing.

#### Syntax
`liquidcrystal.busy(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- `true` if device is busy, `false` if device is ready to receive commands

#### Example
```lua
while liquidcrystal:busy() do end
```

## liquidcrystal.clear
Clear LCD screen.

#### Syntax
`liquidcrystal.clear(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:clear()
```

## liquidcrystal.cursorLeft
Move cursor one character to the left.

#### Syntax
`liquidcrystal.cursorLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:cursorLeft()
```

## liquidcrystal.cursorMove
Move cursor to position. If `row` not specified, move cursor to address `col`.

!!! note
	Note that column and row indexes start with 1. However, when omitting `row` parameter, cursor addresses start with 0.

#### Syntax
`liquidcrystal.cursorMove(self, col, row)`

#### Parameters
- `self`: `liquidcrystal` instance
- `col`: new cursor position column. If `row` not specified, new cursor position address
- `row`: new cursor position row or `nil`

#### Returns
- sent data

#### Example
```lua
liquidcrystal:cursorMove(5, 1)
liquidcrystal:cursorMove(10, 4)
liquidcrystal:cursorMove(21)
```

## liquidcrystal.cursor
Control cursors highlight mode.

#### Syntax
`liquidcrystal.cursor(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance
- `on`: `true` to turn on, `false` to turn off

#### Returns
- sent data

#### Example
```lua
liquidcrystal:cursor(true)
```

## liquidcrystal.cursorRight
Move cursor one character to the right.

#### Syntax
`liquidcrystal.cursorRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:cursorRight()
```

## liquidcrystal.customChar
Define new custom char. Up to 8 custom characters with indexes 0 to 7 may be defined in eight dot mode.
They are accessed via `write` function by index.
In ten dot mode only 4 custom characters may be used.
They are numbered from 0 to 7 with half of them being aliases to each other (0 to 1, 2 to 3 etc).

!!! note
	Upon redefinition of a custom character all its instances will be updated automatically.
	
	This function resets cursor position to home if `liquidcrystal.position` function is not available.
	
	There are web services ([1](https://omerk.github.io/lcdchargen/), [2](https://www.quinapalus.com/hd44780udg.html)) and [desktop applications](https://pypi.org/project/lcdchargen/) that help create custom characters.

#### Syntax
`liquidcrystal.customChar(self, index, bytes)`

#### Parameters
- `self`: `liquidcrystal` instance
- `index`: custom char index in range from 0 to 7
- `bytes`: array of 8 bytes in eight bit mode or 11 bytes in ten bit mode (eleventh line is a cursor line that can also be used) that defines new char bitmap line by line

#### Returns
`nil`

#### Example
```lua
liquidcrystal:customChar(5, {14,31,31,31,31,31,31,31})
liquidcrystal:write(5)
```

## liquidcrystal.display
Turn display on and off. Does not affect display backlight. Does not clear the display.

#### Syntax
`liquidcrystal.display(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance
- `on`: `true` to turn on, `false` to turn off

#### Returns
- sent data

#### Example
```lua
liquidcrystal:display(true)
```

## liquidcrystal.home
Reset cursor and screen position.

#### Syntax
`liquidcrystal.home(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:home()
```

## liquidcrystal.leftToRight
Print text left to right (default).

#### Syntax
`liquidcrystal.leftToRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:leftToRight()
```

## liquidcrystal.position
Get current position of the cursor. Position is 0 indexed. When using GPIO backend without `rw` argument specification function does nothing.

#### Syntax
`liquidcrystal.position(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- 0 indexed position of the cursor

#### Example
```lua
local pos = liquidcrystal:position() -- save position
-- some code
liquidcrystal:cursorMove(pos) -- restore position
```

## liquidcrystal.read
Return current character numerical representation.
When using GPIO backend without `rw` argument specification function does nothing.

#### Syntax
`liquidcrystal.read(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- numerical representation of the current character

#### Example
```lua
liquidcrystal:home() -- goto home
local ch = liquidcrystal:read() -- read char
liquidcrystal:cursorMove(1, 2) -- move to the second line
for i=ch,ch+5 do lc:write(i) end -- print 6 chars starting with ch
```

## liquidcrystal.readCustom
Return custom char byte array.
When using GPIO backend without `rw` argument specification function returns zeros.

#### Syntax
`liquidcrystal.readCustom(self, index)`

#### Parameters
- `self`: `liquidcrystal` instance
- `index`: custom char index in range from 0 to 7

#### Returns
- table of size 8 in eight dot mode or 11 in ten dot mode. Each 8 bit number represents a character dot line

#### Example
```lua
lc:customChar(0, {0,14,31,31,4,4,5,2}) -- define custom character
for _, d in ipairs(lc:readCustom(0)) do print(d) end -- read it back
```

## liquidcrystal.rightToLeft
Print text right to left.

#### Syntax
`liquidcrystal.rightToLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:rightToLeft()
```

## liquidcrystal.scrollLeft
Move text to the left.

#### Syntax
`liquidcrystal.scrollLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:scrollLeft()
```

## liquidcrystal.scrollRight
Move text to the right.

#### Syntax
`liquidcrystal.scrollRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance

#### Returns
- sent data

#### Example
```lua
liquidcrystal:scrollRight()
```

## liquidcrystal.write
Print text.

#### Syntax
`liquidcrystal.write(self, ...)`

#### Parameters
- `self`: `liquidcrystal` instance
- `...`: strings or char codes. For the list of available characters refer to [HD44780 datasheet](https://www.sparkfun.com/datasheets/LCD/HD44780.pdf#page=17)

#### Returns
`nil`

#### Example
```lua
liquidcrystal:write("hello world")
liquidcrystal:write("hello yourself", "!!!", 243, 244)
```


