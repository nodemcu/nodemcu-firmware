# WS2801 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-07-12 | [Espressif example](https://github.com/CHERTS/esp8266-devkit/blob/master/Espressif/examples/EspLightNode/user/ws2801.c), [Konrad Beckmann](https://github.com/kbeckmann) | [Konrad Beckmann](https://github.com/kbeckmann) | [ws2801.c](../../../app/modules/ws2801.c)|


## ws2801.init()
Initializes the module and sets the pin configuration.

#### Syntax
`ws2801.init(pin_clk, pin_data)`

#### Parameters
- `pin_clk` pin for the clock. Supported are GPIO 0, 2, 4, 5.
- `pin_data` pin for the data. Supported are GPIO 0, 2, 4, 5.

#### Returns
`nil`

## ws2801.write()
Sends a string of RGB Data in 24 bits to WS2801. Don't forget to call `ws2801.init()` before.

#### Syntax
`ws2801.write(string)`

####Parameters
- `string` payload to be sent to one or more WS2801.
  It should be composed from an RGB triplet per element.
    - `R1` the first pixel's red channel value (0-255)
    - `G1` the first pixel's green channel value (0-255)
    - `B1` the first pixel's blue channel value (0-255)<br />
    ... You can connect a lot of WS2801...
    - `R2`, `G2`, `B2` are the next WS2801's Red, Green, and Blue channel values

#### Returns
`nil`

#### Example
```lua
ws2801.write(string.char(255,0,0, 0,255,0, 0,0,255))
```
