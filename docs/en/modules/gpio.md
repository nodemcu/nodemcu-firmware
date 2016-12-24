# GPIO Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [gpio.c](../../../app/modules/gpio.c)|


This module provides access to the [GPIO](https://en.wikipedia.org/wiki/General-purpose_input/output) (General Purpose Input/Output) subsystem.

All access is based on the I/O index number on the NodeMCU dev kits, not the internal GPIO pin. For example, the D0 pin on the dev kit is mapped to the internal GPIO pin 16.

If not using a NodeMCU dev kit, please refer to the below GPIO pin maps for the index↔gpio mapping.

| IO index | ESP8266 pin | IO index | ESP8266 pin |
|---------:|:------------|---------:|:------------|
|    0 [*] | GPIO16      |        7 | GPIO13      |
|        1 | GPIO5       |        8 | GPIO15      |
|        2 | GPIO4       |        9 | GPIO3       |
|        3 | GPIO0       |       10 | GPIO1       |
|        4 | GPIO2       |       11 | GPIO9       |
|        5 | GPIO14      |       12 | GPIO10      |
|        6 | GPIO12      |          |             |

** [*] D0(GPIO16) can only be used as gpio read/write. No support for open-drain/interrupt/pwm/i2c/ow. **


## gpio.mode()

Initialize pin to GPIO mode, set the pin in/out direction, and optional internal weak pull-up.

#### Syntax
`gpio.mode(pin, mode [, pullup])`

#### Parameters
- `pin` pin to configure, IO index
- `mode` one of gpio.OUTPUT, gpio.OPENDRAIN, gpio.INPUT, or gpio.INT (interrupt mode)
- `pullup` gpio.PULLUP enables the weak pull-up resistor; default is gpio.FLOAT

#### Returns
`nil`

#### Example
```lua
gpio.mode(0, gpio.OUTPUT)
```
#### See also
- [`gpio.read()`](#gpioread)
- [`gpio.write()`](#gpiowrite)

## gpio.read()

Read digital GPIO pin value.

#### Syntax
`gpio.read(pin)`

#### Parameters
`pin` pin to read, IO index

#### Returns
a number, 0 = low, 1 = high

#### Example
```lua
-- read value of gpio 0.
gpio.read(0)
```
#### See also
[`gpio.mode()`](#gpiomode)

## gpio.serout()

Serialize output based on a sequence of delay-times in µs. After each delay, the pin is toggled. After the last cycle and last delay the pin is not toggled.

The function works in two modes: 
* synchronous - for sub-50 µs resolution, restricted to max. overall duration,
* asynchrounous - synchronous operation with less granularity but virtually unrestricted duration.

Whether the asynchronous mode is chosen is defined by presence of the `callback` parameter. If present and is of function type the function goes asynchronous and the callback function is invoked when sequence finishes. If the parameter is numeric the function still goes asynchronous but no callback is invoked when done.

For the asynchronous version, the minimum delay time should not be shorter than 50 μs and maximum delay time is 0x7fffff μs (~8.3 seconds).
In this mode the function does not block the stack and returns immediately before the output sequence is finalized. HW timer `FRC1_SOURCE` mode is used to change the states. As there is only a single hardware timer, there
are restrictions on which modules can be used at the same time. An error will be raised if the timer is already in use.

Note that the synchronous variant (no or nil `callback` parameter) function blocks the stack and as such any use of it must adhere to the SDK guidelines (also explained [here](../extn-developer-faq/#extension-developer-faq)). Failure to do so may lead to WiFi issues or outright to crashes/reboots. In short it means that the sum of all delay times multiplied by the number of cycles should not exceed 15 ms.

#### Syntax
`gpio.serout(pin, start_level, delay_times [, cycle_num[, callback]])`

#### Parameters
- `pin`  pin to use, IO index
- `start_level` level to start on, either `gpio.HIGH` or `gpio.LOW`
- `delay_times` an array of delay times in µs between each toggle of the gpio pin. 
- `cycle_num` an optional number of times to run through the sequence. (default is 1)
- `callback` an optional callback function or number, if present the function returns immediately and goes asynchronous.


#### Returns
`nil`

#### Example
```lua
gpio.mode(1,gpio.OUTPUT,gpio.PULLUP)
gpio.serout(1,gpio.HIGH,{30,30,60,60,30,30})  -- serial one byte, b10110010
gpio.serout(1,gpio.HIGH,{30,70},8)  -- serial 30% pwm 10k, lasts 8 cycles
gpio.serout(1,gpio.HIGH,{3,7},8)  -- serial 30% pwm 100k, lasts 8 cycles
gpio.serout(1,gpio.HIGH,{0,0},8)  -- serial 50% pwm as fast as possible, lasts 8 cycles
gpio.serout(1,gpio.LOW,{20,10,10,20,10,10,10,100}) -- sim uart one byte 0x5A at about 100kbps
gpio.serout(1,gpio.HIGH,{8,18},8) -- serial 30% pwm 38k, lasts 8 cycles

gpio.serout(1,gpio.HIGH,{5000,995000},100, function() print("done") end) -- asynchronous 100 flashes 5 ms long every second with a callback function when done
gpio.serout(1,gpio.HIGH,{5000,995000},100, 1) -- asynchronous 100 flashes 5 ms long, no callback
```

## gpio.trig()

Establish or clear a callback function to run on interrupt for a pin.

This function is not available if GPIO_INTERRUPT_ENABLE was undefined at compile time.

#### Syntax
`gpio.trig(pin, [type [, callback_function]])`

#### Parameters
- `pin` **1-12**, pin to trigger on, IO index. Note that pin 0 does not support interrupts.
- `type` "up", "down", "both", "low", "high", which represent *rising edge*, *falling edge*, *both 
edges*, *low level*, and *high level* trigger modes respectivey. If the type is "none" or omitted 
then the callback function is removed and the interrupt is disabled.
- `callback_function(level, when)` callback function when trigger occurs. The level of the specified pin 
at the interrupt passed as the first parameter to the callback. The timestamp of the event is passed
as the second parameter. This is in microseconds and has the same base as for `tmr.now()`. This timestamp
is grabbed at interrupt level and is more consistent than getting the time in the callback function.
The previous callback function will be used if the function is omitted.

#### Returns
`nil`

#### Example

```lua
do
  -- use pin 1 as the input pulse width counter
  local pin, pulse1, du, now, trig = 1, 0, 0, tmr.now, gpio.trig
  gpio.mode(pin,gpio.INT)
  local function pin1cb(level, pulse2)
    print( level, pulse2 - pulse1 )
    pulse1 = pulse2
    trig(pin, level == gpio.HIGH  and "down" or "up")
  end
  trig(pin, "down", pin1cb)
end
```

#### See also
[`gpio.mode()`](#gpiomode)

## gpio.write()

Set digital GPIO pin value.

#### Syntax
`gpio.write(pin, level)`

#### Parameters
- `pin` pin to write, IO index
- `level` `gpio.HIGH` or `gpio.LOW`

#### Returns
`nil`

#### Example
```lua
-- set pin index 1 to GPIO mode, and set the pin to high.
pin=1
gpio.mode(pin, gpio.OUTPUT)
gpio.write(pin, gpio.HIGH)
```
#### See also
- [`gpio.mode()`](#gpiomode)
- [`gpio.read()`](#gpioread)
