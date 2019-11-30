# LiquidCrystal Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-12-01 | [Matsievskiy Sergey](https://github.com/seregaxvm) | [Matsievskiy Sergey](https://github.com/seregaxvm) | [liquidcrystal.lua](../../lua_modules/liquidcrystal/liquidcrystal.lua) |

This Lua module provides access to [Hitachi HD44780](https://www.sparkfun.com/datasheets/LCD/HD44780.pdf) based LCDs. It supports 4 bit and 8 bit GPIO interface, 4 bit [PCF8574](https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf) based I²C interface. Default pinout matches pinout of [popular I²C interface converter used with LED screens](https://i.ebayimg.com/images/i/281810234335-0-1/s-l1000.jpg). It may be changed in file [i2c4bit.lua](../../lua_modules/liquidcrystal/i2c4bit.lua).

!!! note
	This module requires `bit` C module built into firmware. Depending on the interface, `gpio` or `i2c` module is also required. `tmr` is needed if one wishes to enable `insert_delay` option.

## Program example

In this example LED screen is connected using I²C GPIO expander.
Program defines two custom characters and prints text.

```lua
lc = require "liquidcrystal"
lc:init("I2C", -- select I2C backend
	{ -- I2C bus settings
	   address=0x27,
	   id=0,
	   sda=1,
	   scl=2,
	   speed=i2c.FAST
	},
	true, -- four bit interface
	false, -- two line mode
	true, -- eight dots font size
	20, -- screen width
	true -- add delay
)
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
```

### Require
```lua
lc = require("liquidcrystal")
```

### Release
```lua
liquidcrystal = nil
package.loaded["liquidcrystal"] = nil
```

## liquidcrystal.autoscroll
Autoscroll text when printing. When turned off, cursor moves and text stays still, when turned on, vice versa.

#### Syntax
`liquidcrystal.autoscroll(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `on`: `true` to turn on, `false` to turn off.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:autoscroll(true)
```

## liquidcrystal.backlight
Control LCDs backlight.

#### Syntax
`liquidcrystal.backlight(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `on`: `true` to turn on, `false` to turn off.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:backlight(true)
```

## liquidcrystal.blink
Control cursors blink mode.

#### Syntax
`liquidcrystal.blink(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `on`: `true` to turn on, `false` to turn off.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:blink(true)
```

## liquidcrystal.clear
Clear LCD screen.

#### Syntax
`liquidcrystal.clear(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:clear()
```

## liquidcrystal.cursor
Control cursors highlight mode.

#### Syntax
`liquidcrystal.cursor(self, on)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `on`: `true` to turn on, `false` to turn off.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:cursor(true)
```

## liquidcrystal.cursorLeft
Move cursor one character to the left.

#### Syntax
`liquidcrystal.cursorLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:cursorLeft()
```

## liquidcrystal.cursorMove
Move cursor to position. If `row` not specified, move cursor to address `col`. Note that column and row indexes start with 1. However, when omitting `row` parameter, cursor addresses start with 0.

#### Syntax
`liquidcrystal.cursorMove(self, col, row)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `col`: new cursor position column. If `row` not specified, new cursor position address.
- `row`: new cursor position row or `nil`.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:cursorMove(5,1)
liquidcrystal:cursorMove(10,4)
liquidcrystal:cursorMove(21)
```

## liquidcrystal.cursorRight
Move cursor one character to the right.

#### Syntax
`liquidcrystal.cursorRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:cursorRight()
```

## liquidcrystal.customChar
Define new custom char. Up to 8 custom characters with indexes 0 to 7 may be defined. They are accessed via `write` function by index.
Note that upon redefinition of a custom character all its instances will be updated automatically.
This function resets cursor position to home.

!!! note
	There are web services (like [1](https://omerk.github.io/lcdchargen/) and [2](https://www.quinapalus.com/hd44780udg.html)) that help create custom characters.

#### Syntax
`liquidcrystal.customChar(self, index, bytes)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `index`: custom char index in range from 0 to 7.
- `bytes`: array of 8 or 10 bytes that defines new char bitmap line by line.

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
- `self`: `liquidcrystal` instance.
- `on`: `true` to turn on, `false` to turn off.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:display(true)
```

## liquidcrystal.home
Reset cursor and screen position.

#### Syntax
`liquidcrystal.home(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:home()
```

## liquidcrystal.init
Function to setup the NodeMCU communication interface and configure LCD.

#### Syntax
`liquidcrystal.init(self, bus, bus_args, fourbitmode, onelinemode, eightdotsmode, column_width, insert_delay)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `bus`: "GPIO" or "I2C".
- `bus_args`: interface configuration table.
  - for 4 bit `GPIO` interface table elements are:
	- `en`: IO connected to LCDs `EN` pin
	- `rs`: IO connected to LCDs `RS` pin
	- `bl`: IO controlling LCDs backlight. Optional
	- `0`: IO connected to LCDs `D4` pin
	- `1`: IO connected to LCDs `D5` pin
	- `2`: IO connected to LCDs `D6` pin
	- `3`: IO connected to LCDs `D7` pin
  - for 8 bit `GPIO` interface table elements are:
	- `en`: IO connected to LCDs `EN` pin
	- `rs`: IO connected to LCDs `RS` pin
	- `bl`: IO controlling LCDs backlight. Optional
	- `0`: IO connected to LCDs `D0` pin
	- `1`: IO connected to LCDs `D1` pin
	- `2`: IO connected to LCDs `D2` pin
	- `3`: IO connected to LCDs `D3` pin
	- `4`: IO connected to LCDs `D4` pin
	- `5`: IO connected to LCDs `D5` pin
	- `6`: IO connected to LCDs `D6` pin
	- `7`: IO connected to LCDs `D7` pin
  - for `I2C` interface table elements are:
	- `address`: 7 bit address of `PCF8574` chip
	- `id`: `i2c` bus id
	- `speed`: `i2c` communication speed
	- `sda`: IO connected to `PCF8574` `SDA` pin. If set to `nil`, I²C bus initialization step via [`i2c.setup`](https://nodemcu.readthedocs.io/en/master/modules/i2c/#i2csetup) will be skipped
	- `scl`: IO connected to `PCF8574` `SCL` pin
	- `pinout`: pinout table of the I²C GPIO expander. If set to `nil`, default values will be
      used (used by most vendors)
	  - `rs`: `RS` pin bit position (default `0`)
      - `rw`: `RW` pin bit position (default `1`)
      - `en`: `EN` pin bit position (default `2`)
      - `backlight`: backlight controlling pin bit position (default `3`)
      - `d4`: `D4` pin bit position (default `4`)
      - `d5`: `D5` pin bit position (default `5`)
      - `d6`: `D6` pin bit position (default `6`)
      - `d7`: `D7` pin bit position (default `7`)

- `fourbitmode`: `true` to use 4 bit communication interface, `false` to use 8 bit communication. Set to `false` is you wish to use GPIO interface with 8 data wires. Otherwise set to `true`.
- `onelinemode`: `true` to use one line mode, `false` to use two line mode.
- `eightdotsmode`: `true` to use 5x8 dot font, `false` to use 5x10 dot font.
- `column_width`: number of characters in column. Used for offset calculations in function `cursorMove`. If set to `nil`, functionality of `cursorMove` will be limited.
- `insert_delay`: `true` to insert appropriate `tmr.delay` for time consuming operations. It is safe to set this to `false` if you don't redraw screen constantly. Set to `true`, if there are some glitches.

!!! note
	LCDs `RW` pin must be pulled to the ground.

#### Returns
`nil`

#### Example (4 bit GPIO)
```lua
liquidcrystal:init("GPIO", {rs=0,en=1,[0]=2,[1]=3,[2]=4,[3]=5},true, true, false, 20, true)
```

#### Example (8 bit GPIO)
```lua
liquidcrystal:init("GPIO", {rs=0,en=1,[0]=6,[1]=7,[2]=8,[3]=12,[4]=2,[5]=3,[6]=4,[7]=5},false, false, true, nil, true)
```

#### Example (I2C)
```lua
liquidcrystal:init("I2C", {address=0x27,id=0,sda=1,scl=2,speed=i2c.SLOW},true, false, true, 20, true)
```

## liquidcrystal.leftToRight
Print text left to right (default).

#### Syntax
`liquidcrystal.leftToRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:leftToRight()
```

## liquidcrystal.rightToLeft
Print text right to left.

#### Syntax
`liquidcrystal.rightToLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:rightToLeft()
```

## liquidcrystal.scrollLeft
Move text to the left.

#### Syntax
`liquidcrystal.scrollLeft(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:scrollLeft()
```

## liquidcrystal.scrollRight
Move text to the right.

#### Syntax
`liquidcrystal.scrollRight(self)`

#### Parameters
- `self`: `liquidcrystal` instance.

#### Returns
`nil`

#### Example
```lua
liquidcrystal:scrollRight()
```

## liquidcrystal.write
Print text.

#### Syntax
`liquidcrystal.write(self, ...)`

#### Parameters
- `self`: `liquidcrystal` instance.
- `...`: strings or char codes. For the list of available characters refer to [HD44780 datasheet](https://www.sparkfun.com/datasheets/LCD/HD44780.pdf#page=17).

#### Returns
`nil`

#### Example
```lua
liquidcrystal:write("hello world")
liquidcrystal:write("hello yourself", "!!!", 243, 244)
```

