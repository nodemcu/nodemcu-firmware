# TM1829 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-05-15 | [Sebastian Haas](https://github.com/sebi2k1) | [Sebastian Haas](https://github.com/sebi2k1) | [tm1829.c](../../../app/modules/tm1829.c)|

tm1829 is a library to handle led strips using Titan Micro tm1829
led controller.

The library uses any GPIO to bitstream the led control commands.

## tm1829.write()
Send data to a led strip using native chip format.

#### Syntax
`tm1829.write(string)`

#### Parameters
- `string` payload to be sent to one or more TM1829 leds.

#### Returns
`nil`

#### Example
```lua
tm1829.write(5, string.char(255,0,0,255,0,0)) -- turn the two first RGB leds to blue using GPIO 5
```

