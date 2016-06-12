# AM2320 Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-02-14 | [Henk Vergonet](https://github.com/hvegh) | [Henk Vergonet](https://github.com/hvegh) | [am2320.c](../../../app/modules/am2320.c)|


This module provides access to the [AM2320](https://akizukidenshi.com/download/ds/aosong/AM2320.pdf) humidity and temperature sensor, using the i2c interface.

## am2320.init()
Initializes the module and sets the pin configuration. Returns model, version, serial but is seams these where all zero on my model.

#### Syntax
`model, version, serial = am2320.init(sda, scl)`

#### Parameters
- `sda` data pin
- `scl` clock pin

#### Returns
- `model`  16 bits number of model
- `version`  8 bits version number
- `serial`  32 bits serial number

   Note: I have only observed values of 0 for all of these, maybe other sensors return more sensible readings.

## am2320.read()
Samples the sensor and returns the relative humidity in % and temperature in celsius, as an integer multiplied with 10.

#### Syntax
`am2320.read()`

#### Returns
- `relative humidity` percentage multiplied with 10 (integer)
- `temperature` in celcius multiplied with 10 (integer)

#### Example
```lua
am2320.init(1, 2)
rh, t = am2320.read()
print(string.format("RH: %s%%", rh / 10))
print(string.format("Temperature: %s degrees C", t / 10))
```

