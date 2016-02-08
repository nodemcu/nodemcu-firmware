# GPIOSer Module

This module can be used for reading bits from wide range of sensors (bit banging).

## gpioser.read()
Reads data from the GPIO sensors using specified intervals. 

#### Syntax
`gpioser.read(pin_clk, pin_out, num_bits, after_ticks, delay_us, initial_val, ready_state)`

#### Parameters
`pin_clk` pin number of the sensor (the CLOCK channel), type is number
`pin_out` pin number of the sensor (the DATA channel), type is number
`num_bits` how many bits read, type is number
`after_ticks` how many times to pulse the sensor after all bits were read, type is number
`delay_us` delay in microseconds between writing and reading, type is number
`initial_val` start by writing this state into the CLK channel, type is number (1, 0)
`ready_state` when this value is detected, the reading of bits will start, type is number (1, 0)

#### Returns
- `reading` the C part returns `long` type, which in Lua will be `Number`

Returns `-1` when `pin_data` did not change its state into `ready_state` (may signify that the
sensor/convertor is not connected; the module will try to read the state 1000 times and then
give up).

!!! note "Note:"

    You are supposed to do the appropriate bitmasking in your Lua code. Read the sensor manual
    to discover the appropriate bit mask.
    

#### Example

Read 24 bits, from pin 6 (pin 5 is CLOCK). Wait 1us between each write/read cycle. The initial value
written to pin 5 is LOW (0), and ready state (ready to read data) is when the pin 6 changes to LOW (0)


```lua
val = gpioser.read(5, 6, 24, 1, 1, 0, 0)
v = bit.bxor(val, 0x800000)
print ("HX711 weight:";v)
```

#### See also
- [`hx711.read()`](hx711#hx711read)


## gpioser.info()
Returns information about the state of the library. 

#### Syntax
`gpioser.info()`

#### Returns
- `msg` type is a string


#### Example


```lua
val = gpioser.read(5, 6, 24, 1, 1, 0, 0)
print (gpioser.info()) -- prints "pin clock:5, pin output:6, state:0"
```
