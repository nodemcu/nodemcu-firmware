# L3G4200D Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-04-09 | [Jason Schmidlapp](https://github.com/jschmidlapp) | [Jason Schmidlapp](https://github.com/jschmidlapp) | [l3g4200d.c](../../../app/modules/l3g4200d.c)|


This module provides access to the [L3G4200D](https://www.sparkfun.com/products/10612) three axis digital gyroscope.

## l3g4200d.read()
Samples the sensor and returns the gyroscope output.

#### Syntax
`l3g4200d.read()`

#### Returns
X,Y,Z gyroscope output

#### Example
```lua
l3g4200d.init(1, 2)
local x,y,z = l3g4200d.read()
print(string.format("X = %d, Y = %d, Z = %d", x, y, z)
```

## l3g4200d.init()
Initializes the module and sets the pin configuration.

#### Syntax
`l3g4200d.init(sda, scl)`

#### Parameters
- `sda` data pin
- `scl` clock pin

#### Returns
`nil`
