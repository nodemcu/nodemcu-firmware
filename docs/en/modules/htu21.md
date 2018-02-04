# HDC1080 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-02-03 | [Pawel Switalski](https://github.com/s-pw) | [Pawel Switalski](https://github.com/s-pw) | [htu21.c](../../../components/modules/htu21.c)|


This module provides access to the [HTU21D](https://www.sparkfun.com/products/13763) temperature and humidity sensor. The module also works with SHT21.

## htu21.read()
Samples the sensor then returns temperature and humidity value.

#### Syntax
`htu21.read()`

#### Returns
Temperature data in millidegree Celsius and humidity data in per cent mille

#### Example
```lua
local sda, scl = 1, 2
i2c.setup(i2c.SW, sda, scl, i2c.SLOW)  -- call i2c.setup() only once
local temperature, humidity = htu21.read()
print(temperature / 1000 .. '°C')
print(humidity / 1000 .. '%')
```