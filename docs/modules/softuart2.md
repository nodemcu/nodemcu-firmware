# SoftUART2 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
|2021-02-19 | [sjp27](https://github.com/sjp27/) | [sjp27](https://github.com/sjp27/) | [module/softuart2.c](../../app/modules/softuart2.c) [driver/softuart2.c](../../app/driver/softuart2.c)  |

The SoftUART2 module offers a bit bashed serial port over GPIO pins making exclusive use of H/W timer 1 for timing, does not burn processor. It can operate full duplex up to 4800 baud.
    
ESP8266 has only 1 full hardware UART port that is used to program the chip and communicate with NodeMCU firmware. The second port is transmit-only. More information can be found in [uart module documentation](uart/). This module provides an additional UART port and can be used to communicate with devices like GSM or GPS modules. The code is based on [softuart](softuart/). Currently doesn't support inverted serial data logic or modes other than 8N1. It's important to notice that this is a software implementation of the serial protocol. There could be some interrupts that make the transmission or reception fail due to invalid timing.

!!! note

    SoftUART cannot be used on D0 pin.

## softuart2.setup()

Creates new SoftUART2 instance.

#### Syntax
`softuart2.setup(baudrate, txPin, rxPin, clkPin)`

#### Parameters
- `baudrate`: SoftUART2 baudrate. Maximum supported is 4800.
- `txPin`: SoftUART2 tx pin. If set to `nil` `write` method will not be supported.
- `rxPin`: SoftUART2 rx pin. If set to `nil` `on("data")` method will not be supported.
- `clkPin`: SoftUART2 clk pin. If set to `nil` 16x baudrate clock will not be generated. Useful for IrDA.

#### Returns
`softuart2` instance.

#### Example
```lua
-- Create new software UART with baudrate of 4800, D2 as Tx pin and D3 as Rx pin and D1 as Clk pin
s = softuart2.setup(4800, 2, 3, 1)
```

# SoftUART2 port


## softuart2.port:on()
Sets up the callback function to receive data.

#### Syntax
`softuart2.port:on(event, trigger, function(data))`

#### Parameters
- `event`: Event name. Currently only `data` is supported.
- `trigger`: Can be a character or a number. If character is set, the callback function will only be run when that character gets received. When a number is set, the callback function will only be run when buffer will have as many characters as number. If number is -1 characters up to a 2 or more character idle time will be returned.
- `function(data)`: Callback function. the `data` parameter is software UART receiving buffer.

#### Returns
`nil`

#### Example
```lua
-- Create new software UART with baudrate of 4800, D2 as Tx pin and D3 as Rx pin
s = softuart2.setup(4800, 2, 3)
-- Set callback to run when 10 characters show up in the buffer
s:on("data", 10, function(data)
  print("Lua handler called!")
  print(data)
end)
```

## softuart2.port:write()
Transmits a byte or sequence of them.

#### Syntax
`softuart2.port:write(data)`

#### Parameters
- `data`: Can be a number or string. When a number is passed, only one byte will be sent. When a string is passed, whole sequence will be transmitted.

#### Returns
`nil`

#### Example
```lua
-- Create new software UART with baudrate of 4800, D2 as Tx pin and D3 as Rx pin
s = softuart2.setup(4800, 2, 3)
s:write("Hello!")
-- Send character 'a'
s:write(97)
```
