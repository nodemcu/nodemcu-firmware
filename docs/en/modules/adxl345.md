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
local sda, scl = 1, 2
i2c.setup(0, sda, scl, i2c.SLOW)  -- call i2c.setup() only once
adxl345.setup()
local x,y,z = adxl345.read()
print(string.format("X = %d, Y = %d, Z = %d", x, y, z))
```

## adxl345.setup()
Initializes the module.

#### Syntax
`adxl345.setup()`

#### Parameters
None

#### Returns
`nil`
