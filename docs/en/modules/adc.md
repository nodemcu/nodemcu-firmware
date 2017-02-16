# ADC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-22 | | | [adc.c](../../../components/modules/bthci.c)|

The ADC module provides access to the in-built ADC1.

On the ESP32 there are two ADC. ADC1 has 8 channels, while ADC2 has 10 channels. Currently, only ADC1 is supported.

## adc.setwidth()

The configuration is in effect for all channels of ADC1

#### Syntax
`adc.setwidth(bits)`

#### Parameters
`bits` One of `9`/`10`/`11`/`12`.

#### Returns
nil


## adc.setup()

Configuration ADC1 capture attenuation of channels

#### Syntax
`adc.setup(channel, atten)`

#### Parameters
`channel` `0` to `7`. 0: GPIO36, 1: GPIO37, 2: GPIO38, 3: GPIO39, 4: GPIO32, 5: GPIO33, 6: GPIO34, 7: GPIO35
`atten` `adc.ATTEN_0db`, `adc.ATTEN_2_5db`, `adc.ATTEN_6db` or `adc.ATTEN_11db`.

#### Returns
nil


## adc.read()

Samples the ADC. You should to call `setwidth()` before `read()`.

#### Syntax
`adc.read(channel)`

#### Parameters
`channel` 0 to 7 on the ESP32

#### Returns
the sampled value (number)

#### Example
```lua
val = adc.read(0)
```

## adc.read_hall_sensor()

Read Hall sensor (GPIO36, GPIO39). We recommend `setwidth(12)`.

#### Syntax
`adc.read(channel)`

#### Parameters
`channel` 0 to 7 on the ESP32

#### Returns
the sampled value (number)

#### Example
```lua
val = adc.read(0)
```

