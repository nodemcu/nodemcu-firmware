# GPIO Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-11-26 | [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [gpio.c](../../../components/modules/gpio.c)|


This module provides access to the [GPIO](https://en.wikipedia.org/wiki/General-purpose_input/output) (General Purpose Input/Output) subsystem.


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

- `gpio` one or more (given as list) pins, 0 ~ 33 I/O index
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
`pin` pin to read, 0 ~ 33 I/O index

#### Returns
0 = low, 1 = high


## gpio.trig()
Establish or clear a callback function to run on interrupt for a GPIO.

#### Syntax
`gpio.trig(pin, type [, callback])`

#### Parameters
- `pin` 0 ~ 33 I/O index
- `type` trigger type, one of
    - `INTR_UP` for trigger on rising edge
    - `INTR_DOWN` for trigger on falling edge
    - `INTR_UP_DOWN` for trigger on both edges
    - `INTR_LOW` for trigger on low level
    - `INTR_HIGH` for trigger on high level
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
- `pin` 0 ~ 33 I/O index
- `level` wake-up level, one of
    - `INTR_NONE` to disable wake-up
    - `INTR_LOW` for wake-up on low level
    - `INTR_HIGH` for wake-up on high level

#### Returns
`nil`


## gpio.write()
Set digital GPIO pin value.

#### Syntax
`gpio.write(pin, level)`

#### Parameters
- `pin` pin to write, 0 ~ 33 I/O index
- `level` `gpio.HIGH` or `gpio.LOW`

#### Returns
`nil`
