# HMC5883L Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-04-09 | [Jason Schmidlapp](https://github.com/jschmidlapp) | [Jason Schmidlapp](https://github.com/jschmidlapp) | [hmc5883l.c](../../../app/modules/hmc5883l.c)|


This module provides access to the [HMC5883L](https://www.sparkfun.com/products/10530) three axis digital compass.

## hmc5883l.read()
Samples the sensor and returns X,Y and Z data.

#### Syntax
`hmc5883l.read()`

#### Returns
x,y,z measurements (integers)
temperature multiplied with 10 (integer)

#### Example
```lua
hmc58831.init(1, 2)
local x,y,z = hmc5883l.read()
print(string.format("x = %d, y = %d, z = %d", x, y, z))
```

## hmc5883l.init()
Initializes the module and sets the pin configuration.

#### Syntax
`hmc5883l.init(sda, scl)`

#### Parameters
- `sda` data pin
- `scl` clock pin

#### Returns
`nil`
