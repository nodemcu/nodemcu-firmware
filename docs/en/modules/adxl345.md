# ADXL345 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-04-08 | [Jason Schmidlapp](https://github.com/jschmidlapp) | [Jason Schmidlapp](https://github.com/jschmidlapp) | [adxl345.c](../../../app/modules/adxl345.c)|


This module provides access to the [ADXL345](https://www.sparkfun.com/products/9836) triple axis accelerometer.

## adxl345.read()
Samples the sensor and returns X,Y and Z data from the accelerometer.

#### Syntax
`adxl345.read()`

#### Returns
X,Y,Z data (integers)

#### Example
```lua
adxl345.init(1, 2)
local x,y,z = adxl345.read()
print(string.format("X = %d, Y = %d, Z = %d", x, y, z))
```

## adxl345.init()
Initializes the module and sets the pin configuration.

#### Syntax
`adxl345.init(sda, scl)`

#### Parameters
- `sda` data pin
- `scl` clock pin

#### Returns
`nil`
