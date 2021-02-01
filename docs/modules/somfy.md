# Somfy module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-09-27 | [vsky279](https://github.com/vsky279) | [vsky279](https://github.com/vsky279) | [somfy.c](../../app/modules/somfy.c)|

This module provides a simple interface to control Somfy blinds via an RF transmitter (433.42 MHz). It is based on [Nickduino Somfy Remote Arduino skecth](https://github.com/Nickduino/Somfy_Remote). It also allows listening to commands transmitted by the Somfy remote control.

The hardware used is the standard 433 MHz RF transmitter. Unfortunately these chips are usually transmitting at he frequency of 433.92MHz so the crystal resonator should be replaced with the resonator on this frequency though some reporting that it is working even with the original crystal.

To understand details of the Somfy protocol please refer to [Somfy RTS protocol](https://pushstack.wordpress.com/somfy-rts-protocol/) and also discussion [here](https://forum.arduino.cc/index.php?topic=208346.0).

The module is using hardware timer so it cannot be used at the same time with other NodeMCU modules using the hardware timer, i.e. `sigma delta`, `pcm`, `perf`, or `pwm` modules.

## somfy.sendcommand()

Builds an frame defined by Somfy protocol and sends it to the RF transmitter.

#### Syntax
`somfy.sendcommand(pin, remote_address, command, rolling_code, repeat_count, callback)`

#### Parameters
- `pin` GPIO pin the RF transmitter is connected to.
- `remote_address` address of the remote control. The device to be controlled is programmed with the addresses of the remote controls it should listen to.
- `command` command to be transmitted. Can be one of `somfy.SOMFY_UP`, `somfy.SOMFY_DOWN`, `somfy.SOMFY_PROG`, `somfy.SOMFY_STOP`
- `rolling_code` The rolling code is increased every time a button is pressed. The receiver only accepts command if the rolling code is above the last received code and is not to far ahead of the last received code. This window is in the order of a 100 big. The rolling code needs to be stored in the EEPROM (i.e. filesystem) to survive the ESP8266 reset.
- `repeat_count` how many times the command is repeated
- `callback` a function to be called after the command is transmitted. Allows chaining commands to set the blinds to a defined position.

My original remote is [TELIS 4 MODULIS RTS](https://www.somfy.co.uk/products/blinds-and-curtains/buy-products/controls). This remote is working with the additional info - additional 56 bits that follow data (shortening the Inter-frame gap). It seems that the scrambling algorithm has not been revealed yet.

When I send the `somfy.DOWN` command, repeating the frame twice (which seems to be the standard for a short button press), i.e. `repeat_count` equal to 2, the blinds go only 1 step down. This corresponds to the movement of the wheel on the original remote. The down button on the original remote sends also `somfy.DOWN` command but the additional info is different and this makes the blinds go full down. Fortunately it seems that repeating the frame 16 times makes the blinds go fully down.

#### Returns
nil

#### Example
To start with controlling your Somfy blinds you need to:

- Choose an arbitrary remote address (different from your existing remote) - `123` in this example
- Choose a starting point for the rolling code. Any unsigned int works, 1 is a good start
- Long-press the program button of your existing remote control until your blind goes up and down slightly
- execute `somfy.sendcommand(4, 123, somfy.PROG, 1, 2)` - the blinds will react and your ESP8266 remote control is now registered
- running `somfy.sendcommand(4, 123, somfy.DOWN, 2, 16)` - fully closes the blinds

For more elaborated example please refer to [`somfy.lua`](../../lua_examples/somfy.lua).


## somfy.listen()

Using RF receiver listens to Somfy commands and triggers a callback when command is identified.

#### Syntax
`somfy.listen(pin, callback)`

#### Parameters
- `pin` GPIO pin the RF receiver is connected to.
- `callback(address, command, rc, frame)` a function called when a Somfy command is identified. Use `nil` to stop listening.
    - `address` of the remote controller sending the command
    - `command` sent by the remote controller. A number between 0 and 0xf. Can be `somfy.SOMFY_UP`, `somfy.SOMFY_DOWN`, `somfy.SOMFY_PROG`, `somfy.SOMFY_STOP`.
    - `rc` rolling code
    - `frame` String of 10 characters with the full captured data frame.

#### Returns
nil

#### Example
```Lua
somfy.listen(4, function(address, command, rc, frame) 
    print(("Address:\t0x%x\nCommand:\t0x%x\nRolling code:\t%d"):format(address, command, rc))
    print(("Frame:\t"..("%02x "):rep(#frame)):format(frame:byte(1, -1)))
end)
```

Use `somfy.listen()` or `somfy.listen(4, nil)` to unhook the GPIO and free the callback.
