# ADC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-22 | [zelll](https://github.com/zelll) | [zelll](https://github.com/zelll) | [adc.c](../../components/modules/adc.c)|

The ADC module provides access to the in-built ADC1.

On the ESP32 there are two ADC. ADC1 has 8 channels, while ADC2 has 10 channels. Currently, only ADC1 is supported.

## adc.setwidth()

The configuration is in effect for all channels of ADC1

#### Syntax
`adc.setwidth(adc_number, bits)`

#### Parameters
- `adc_number` Only `adc.ADC1` now
- `bits` One of `9`/`10`/`11`/`12`.

#### Returns
`nil`


## adc.setup()

Configuration ADC1 capture attenuation of channels

#### Syntax
`adc.setup(adc_number, channel, atten)`

#### Parameters
- `adc_number` Only `adc.ADC1` now
- `channel`  When using `adc.ADC1`: `0` to `7`. 0: GPIO36, 1: GPIO37, 2: GPIO38, 3: GPIO39, 4: GPIO32, 5: GPIO33, 6: GPIO34, 7: GPIO35
- `atten` One of following constants
	- `adc.ATTEN_0db`    The input voltage of ADC will be reduced to about 1/1    (1.1V when VDD_A=3.3V)
	- `adc.ATTEN_2_5db`  The input voltage of ADC will be reduced to about 1/1.34 (1.5V when VDD_A=3.3V)
	- `adc.ATTEN_6db`    The input voltage of ADC will be reduced to about 1/2    (2.2V when VDD_A=3.3V)
	- `adc.ATTEN_11db`   The input voltage of ADC will be reduced to about 1/3.6  (3.9V when VDD_A=3.3V,  maximum voltage is limited by VDD_A)

#### Returns
`nil`


## adc.read()

Samples the ADC. You should to call `setwidth()` before `read()`.

#### Syntax
`adc.read(adc_number, channel)`

#### Parameters
- `adc_number` Only `adc.ADC1` now
- `channel` 0 to 7 for adc.ADC1

#### Returns
the sampled value (number)

#### Example
```lua
val = adc.read(adc.ADC1, 0)
```
