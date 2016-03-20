# BMP085 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-08-03 | [Konrad Beckmann](https://github.com/kbeckmann) | [Konrad Beckmann](https://github.com/kbeckmann) | [bmp085.c](../../../app/modules/bmp085.c)|


This module provides access to the [BMP085](https://www.sparkfun.com/tutorials/253) temperature and pressure sensor. The module also works with BMP180.

## bmp085.init()
Initializes the module and sets the pin configuration.

#### Syntax
`bmp085.init(sda, scl)`

#### Parameters
- `sda` data pin
- `scl` clock pin

#### Returns
`nil`

## bmp085.temperature()
Samples the sensor and returns the temperature in celsius as an integer multiplied with 10.

#### Syntax
`bmp085.temperature()`

#### Returns
temperature multiplied with 10 (integer)

#### Example
```lua
bmp085.init(1, 2)
local t = bmp085.temperature()
print(string.format("Temperature: %s.%s degrees C", t / 10, t % 10))
```

## bmp085.pressure()
Samples the sensor and returns the pressure in pascal as an integer.

The optional `oversampling_setting` parameter determines for how long time the sensor samples data.
The default is `3` which is the longest sampling setting. Possible values are 0, 1, 2, 3.
See the data sheet for more information.

#### Syntax
`bmp085.pressure(oversampling_setting)`

#### Parameters
`oversampling_setting` integer that can be 0, 1, 2 or 3

#### Returns
pressure in pascals (integer)

#### Example
```lua
bmp085.init(1, 2)
local p = bmp085.pressure()
print(string.format("Pressure: %s.%s mbar", p / 100, p % 100))
```

## bmp085.pressure_raw()
Samples the sensor and returns the raw pressure in internal units. Might be useful if you need higher precision.

#### Syntax
`bmp085.pressure_raw(oversampling_setting)`

#### Parameters
`oversampling_setting` integer that can be 0, 1, 2 or 3

#### Returns
raw pressure sampling value (integer)
