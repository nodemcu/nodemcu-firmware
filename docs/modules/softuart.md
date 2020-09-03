# SoftUART Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
|2019-12-27 | [pleningerweb](https://github.com/plieningerweb/), [juancgalvez](https://github.com/juancgalvez/), [crasu](https://github.com/crasu/), [galjonsfigur](https://github.com/galjonsfigur/)| [galjonsfigur](https://github.com/galjonsfigur/) | [softuart.c](../../app/modules/softuart.c) |

The SoftUART module offers bit-banged serial ports over GPIO pins.

!!! warning

    Software implementation of serial port can be unreliable and some reception errors are to be expected.
    
ESP8266 has only 1 full hardware UART port that is used to program the chip and communicate with NodeMCU firmware. The second port is transmit-only. More information can be found in [uart module documentation](uart/). This module provides access to more UART ports and can be used to communicate with devices like GSM or GPS modules. The code is based on [esp8266-software-uart](https://github.com/plieningerweb/esp8266-software-uart) and [Arduino-esp8266-Software-UART](https://github.com/juancgalvez/Arduino-esp8266-Software-UART) projects. Currently doesn't support inverted serial data logic or modes other than 8N1. It's important to notice that this is a software implementation of the serial protocol. There could be some interrupts that make the transmission or reception fail due to invalid timing.

!!! note

    SoftUART cannot be used on D0 pin.

## softuart.setup()

Creates new SoftUART instance. Note that rx pin cannot be shared between instances but tx pin can.

#### Syntax
`softuart.setup(baudrate, txPin, rxPin)`

#### Parameters
- `baudrate`: SoftUART baudrate. Maximum supported is 230400.
- `txPin`: SoftUART tx pin. If set to `nil` `write` method will not be supported.
- `rxPin`: SoftUART rx pin. If set to `nil` `on("data")` method will not be supported.

#### Returns
`softuart` instance.

#### Example
```lua
-- Create new software UART with baudrate of 9600, D2 as Tx pin and D3 as Rx pin
s = softuart.setup(9600, 2, 3)
```

# SoftUART port


## softuart.port:on()
Sets up the callback function to receive data.

#### Syntax
`softuart.port:on(event, trigger, function(data))`

#### Parameters
- `event`: Event name. Currently only `data` is supported.
- `trigger`: Can be a character or a number. If character is set, the callback function will only be run when that character gets received. When a number is set, the callback function will only be run when buffer will have as many characters as number.
- `function(data)`: Callback function. the `data` parameter is software UART receiving buffer.

#### Returns
`nil`

#### Example
```lua
-- Create new software UART with baudrate of 9600, D2 as Tx pin and D3 as Rx pin
s = softuart.setup(9600, 2, 3)
-- Set callback to run when 10 characters show up in the buffer
s:on("data", 10, function(data)
  print("Lua handler called!")
  print(data)
end)
```

## softuart.port:write()
Transmits a byte or sequence of them.

#### Syntax
`softuart.port:write(data)`

#### Parameters
- `data`: Can be a number or string. When a number is passed, only one byte will be sent. When a string is passed, whole sequence will be transmitted.

#### Returns
`nil`

#### Example
```lua
-- Create new software UART with baudrate of 9600, D2 as Tx pin and D3 as Rx pin
s = softuart.setup(9600, 2, 3)
s:write("Hello!")
-- Send character 'a'
s:write(97)
```
