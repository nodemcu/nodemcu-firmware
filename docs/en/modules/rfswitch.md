# rfswitch Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-12-01 | [Roman Fedorov](https://github.com/ffedoroff) | [Roman Fedorov](https://github.com/ffedoroff) | [rfswitch.c](../../../app/modules/rfswitch.c)|


Module for operate 433/315Mhz devices like power outlet sockets, relays, etc.
This will most likely work with all popular low cost power outlet sockets
with a SC5262 / SC5272, HX2262 / HX2272, PT2262 / PT2272, EV1527,
RT1527, FP1527 or HS1527 chipset.

This module using some code from original [rc-switch](https://github.com/sui77/rc-switch/) arduino lib
but NodeMCU and Arduino are not fully compatible, and it cause
for full rewrite **rc-switch** into this new **rfswitch** lib for NodeMCU with Lua support.

### Connection of transmitter

| Transmitter  | ESP8266  | comments                        |
| :----------- | :------- | :------------------------------ |
| vin or + | 3V3 | 3.3 - 5 volts on ESP8266 or other power source |
| ground or - | GND | ground should be connected to ESP8266 and to power source |
| data pin | 6 | almost any pin on ESP8266 |

You can read more about connection, [here](https://alexbloggt.com/wp-content/uploads/2015/10/nodemcu_433_transmitter.png) or [here](https://alexbloggt.com/funksteckdosensteuerung-mit-esp8266/).

### Selecting proper protocol
Current library supports **transmitting** using 6 different protocols
and you should use proper one for your needs.
Current lua library rfswitch doesn't support **receiver** functional yet, so you cannot
listen radio air and get protocol details using lua.

The easiest way to get correct protocol is connect radio receiver to your ESP8266 or [Arduino](https://github.com/sui77/rc-switch/wiki/HowTo_Receive),
then run [ReceiveDemo_Advanced.ino](https://github.com/sui77/rc-switch/blob/master/examples/ReceiveDemo_Advanced/ReceiveDemo_Advanced.ino)
and view output in serial console ([example1](http://www.instructables.com/id/Control-CoTech-Remote-Switch-With-Arduino-433Mhz/?ALLSTEPS),
[example2](http://randomnerdtutorials.com/esp8266-remote-controlled-sockets/)).

You should get something like this:
```
Decimal: 11001351 (24Bit)
Binary: 101001111101111000000111
Tri-State: not applicable
PulseLength: 517 microseconds
Protocol: 5

Raw data: 7200,1004,528,504,1048,980,336,1176,356,1176,352,1180,1108,412,356,1172,364,1168,356,1160,1176,1124,412,336,1180,1116,440,328,1188,340,1228,1060,416,1160,380,1160,1108,464,1068,436,328,1232,1060,412,1116,440,1088,428,3024,
```
More detailed about low level protocol specifications could be found [here](https://github.com/sui77/rc-switch/wiki/KnowHow_LineCoding)
You can visualize a telegram copy the raw data by paste it into [http://test.sui.li/oszi/]()

## rfswitch.send()
Transmit value ising radio module.

#### Syntax
`rfswitch.send(protocol_id, pulse_length, repeat, pin, value, length)`

#### Parameters
- `protocol_id` positive integer value, from 1-6
- `pulse_length` length of one pulse in microseconds, usually from 100 till 650
- `repeat` repeat value, usually from 1 till 5. This is synchronous task.
Setting the repeat count to a large value will cause problems.
The recommended limit is about 1-4, if you need more,
then call it asynchronously a few more times (e.g. using [node.task.post](../modules/node/#nodetaskpost))
- `pin` IO index of pin, example 6 is for GPIO12 [see more](../modules/gpio/)
- `value` positive integer value, this is the primary data which will be sent
- `length` bit length of value, if value length is 3 bytes, then length is 24

#### Returns
`nil`

#### Example
```lua
-- lua transmit radio code using protocol #1
-- pulse_length 300 microseconds
-- repeat 5 times
-- use pin #7 (GPIO13)
-- value to send is 560777
-- value length is 24 bits (3 bytes)
rfswitch.send(1, 300, 5, 7, 560777, 24)
```
