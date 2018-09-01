# RC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-12 | [Mike Wen](https://github.com/mikewen) | - | [rc.c](../../../app/modules/rc.c)|

Module to generate series of impulses for remote control via 433/315Mhz radio transmitter.
Superseded by **[rfswitch](./rfswitch.md)** module which have same functionality, and supports more transmission protocols.

For more detailed description see [rfswitch module documentation](./rfswitch.md).

## rc.send()
Sends series of impulses

#### Syntax
`rc.send(pin, value, length, pulse_length, protocol_id, repeat)`

which is similar to:
`rfswitch.send(protocol_id, pulse_length, repeat, pin, value, length)`


#### Parameters
- `pin` 0~12, I/O index of pin, example 6 is for GPIO12, see [details](../modules/gpio/)
- `value` positive integer value, this is the primary data which will be sent
- `length` bit length of value, if value length is 3 bytes, then length is 24
- `pulse_length` length of one pulse in microseconds, usually from 100 to 650
- `protocol_id` positive integer value, from 1-3
- `repeat` repeat value, usually from 1 to 5. This is a synchronous task. Setting the repeat count to a large value will cause problems.
The recommended limit is about 1-4.

#### Returns
`1` always 1

#### Example
```lua
-- Lua transmit radio code using protocol #1
-- pulse_length 300 microseconds
-- repeat 5 times
-- use pin #7 (GPIO13)
-- value to send is 560777
-- value length is 24 bits (3 bytes)
rc.send(7, 560777, 24, 300, 1, 5)
```
which is similar  to:
```lua
rfswitch.send(1, 300, 5, 7, 560777, 24)
```
