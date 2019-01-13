# GPIO Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-11-26 | [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [gpio.c](../../components/modules/gpio.c)|


This module provides access to the [GPIO](https://en.wikipedia.org/wiki/General-purpose_input/output) (General Purpose Input/Output) subsystem.

# GPIO Overview
The ESP32 chip features 40 physical GPIO pads. Some GPIO pads cannot be used or do not have the corresponding pin on the chip package (refer to the [ESP32 Datasheet](http://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)).

- GPIO6-11 are usually used for SPI flash.
- GPIO20, GPIO24, and GPIO28-31 are not available as pins.
- GPIO34-39 can only be set as input mode and do not have software pullup or pulldown functions.


## gpio.config()
Configure GPIO mode for one or more pins.

#### Syntax
```lua
gpio.config({
  gpio=x || {x, y, z},
  dir=gpio.IN || gpio.OUT || gpio.IN_OUT,
  opendrain= 0 || 1 -- only applicable to output modes
  pull= gpio.FLOATING || gpio.PULL_UP || gpio.PULL_DOWN || gpio.PULL_UP_DOWN
}, ...)
```

#### Parameters
List of configuration tables:

- `gpio` one or more (given as list) pins, see [GPIO Overview](#gpio-overview)
- `dir` direction, one of
    - `gpio.IN`
    - `gpio.OUT`
    - `gpio.IN_OUT`
- `opendrain` 1 enables opendrain mode, defaults to 0 if omitted
- `pull` enable pull-up and -down resistors, one of
    - `gpio.FLOATING` disables both pull-up and -down
    - `gpio.PULL_UP` enables pull-up and disables pull-down
    - `gpio.PULL_DOWN` enables pull-down and disables pull-up
    - `gpio.PULL_UP_DOWN` enables both pull-up and -down

#### Returns
`nil`

#### Example
```lua
gpio.config( { gpio={19,21}, dir=gpio.OUT }, { gpio=22, dir=gpio.IN, pull=gpio.PULL_UP })
```

## gpio.read()
Read digital GPIO pin value.

#### Syntax
`gpio.read(pin)`

#### Parameters
`pin` pin to read, see [GPIO Overview](#gpio-overview)

#### Returns
0 = low, 1 = high


## gpio.trig()
Establish or clear a callback function to run on interrupt for a GPIO.

#### Syntax
`gpio.trig(pin, type [, callback])`

#### Parameters
- `pin`, see [GPIO Overview](#gpio-overview)
- `type` trigger type, one of
    - `gpio.INTR_UP` for trigger on rising edge
    - `gpio.INTR_DOWN` for trigger on falling edge
    - `gpio.INTR_UP_DOWN` for trigger on both edges
    - `gpio.INTR_LOW` for trigger on low level
    - `gpio.INTR_HIGH` for trigger on high level
- `callback` optional function to be called when trigger fires, trigger is disabled when omitted. Parameters are:
    - `pin`
    - `level`

#### Returns
`nil`

## gpio.wakeup()
Configuring wake-from-sleep-on-GPIO-level.

#### Syntax
`gpio.wakeup(pin, level)`

#### Parameters
- `pin`, see [GPIO Overview](#gpio-overview)
- `level` wake-up level, one of
    - `gpio.INTR_NONE` to disable wake-up
    - `gpio.INTR_LOW` for wake-up on low level
    - `gpio.INTR_HIGH` for wake-up on high level

#### Returns
`nil`


## gpio.write()
Set digital GPIO pin value.

#### Syntax
`gpio.write(pin, level)`

#### Parameters
- `pin` pin to write, see [GPIO Overview](#gpio-overview)
- `level` 1 or 0

#### Returns
`nil`
