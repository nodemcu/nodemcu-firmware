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


## gpio.set_drive()
Set the drive strength of a given GPIO pin. The higher the drive strength, the more current can be sourced/sunk from the pin. The exact maximum depends on the power domain of the pin and how much current other pins in that domain are consuming.

#### Syntax
`gpio.set_drive(pin, strength)`

#### Parameters
- `pin`, a valid GPIO pin number.
- `strength` the drive strength to set, one of
    - `gpio.DRIVE_0` weakest drive strength
    - `gpio.DRIVE_1` stronger drive strength
    - `gpio.DRIVE_2` default drive strength
    - `gpio.DRIVE_DEFAULT` default drive strength (same as `DRIVE_2`)
    - `gpio.DRIVE_3` maximum drive strength

#### Returns
`nil`

#### Example
```lua
gpio.set_drive(4, gpio.DRIVE_3)
```


## gpio.trig()
Establish or clear a callback function to run on interrupt for a GPIO.

#### Syntax
`gpio.trig(pin [, type [, callback]])`

#### Parameters
- `pin`, see [GPIO Overview](#gpio-overview)
- `type` trigger type, one of
    - `gpio.INTR_DISABLE` or `nil` to disable interrupts on this pin (in which case `callback` is ignored and should be `nil` or omitted)
    - `gpio.INTR_UP` for trigger on rising edge
    - `gpio.INTR_DOWN` for trigger on falling edge
    - `gpio.INTR_UP_DOWN` for trigger on both edges
    - `gpio.INTR_LOW` for trigger on low level
    - `gpio.INTR_HIGH` for trigger on high level
- `callback` optional function to be called when trigger fires. If `nil` or omitted (and `type` is not `gpio.INTR_DISABLE`) then any previously-set callback will continue to be used. Parameters are:
    - `pin`
    - `level`

#### Returns
`nil`


## gpio.wakeup()
Configure whether the given pin should trigger wake up from light sleep initiated by [`node.sleep()`](node.md#nodesleep).

Note that the `level` specified here overrides the interrupt type set by `gpio.trig()`, and wakeup only supports the level-triggered options `gpio.INTR_LOW` and `gpio.INTR_HIGH`. Therefore it is not possible to configure an edge-triggered GPIO callback in combination with wake from light sleep, at least not without reconfiguring the pin immediately before and after the call to `node.sleep()`.

The call to `node.sleep()` must additionally include `gpio = true` in the `options` in order for any GPIO to trigger wakeup.

#### Syntax
`gpio.wakeup(pin, level)`

#### Parameters
- `pin`, see [GPIO Overview](#gpio-overview)
- `level` wake-up level, one of
    - `gpio.INTR_NONE` changes to the level of this pin will not trigger wake from light sleep
    - `gpio.INTR_LOW` if this pin is low it should trigger wake from light sleep 
    - `gpio.INTR_HIGH` if this pin is high it should trigger wake from light sleep 

#### Returns
`nil`

#### See also
[`node.sleep()`](node.md#nodesleep)


## gpio.write()
Set digital GPIO pin value.

#### Syntax
`gpio.write(pin, level)`

#### Parameters
- `pin` pin to write, see [GPIO Overview](#gpio-overview)
- `level` 1 or 0

#### Returns
`nil`
