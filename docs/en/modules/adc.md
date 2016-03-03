# ADC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-24 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [adc.c](../../../app/modules/adc.c)|

The ADC module provides access to the in-built ADC.

On the ESP8266 there is only a single-channel, which is multiplexed with the battery voltage. Depending on the setting in the "esp init data" (byte 107) one can either use the ADC to read an external voltage, or to read the system voltage, but not both.

The default setting in the NodeMCU firmware can be controlled via user_config.h at compile time, by defining one of ESP_INIT_DATA_ENABLE_READVDD33, ESP_INIT_DATA_ENABLE_READADC or ESP_INIT_DATA_FIXED_VDD33_VALUE. To change the setting at a later date, use Espressif's flash download tool to create a new init data block.

## adc.read()

Samples the ADC.

####Syntax
`adc.read(channel)`

####Parameters
`channel` always 0 on the ESP8266

####Returns
the sampled value (number)

####Example
```lua
val = adc.read(0)
```

## adc.readvdd33()

Reads the system voltage.

####Syntax
`adc.readvdd33()`

####Parameters
none

####Returns
system voltage in millivolts (number)

If the ESP8266 has been configured to use the ADC for sampling the external pin, this function will always return 65535. This is a hardware and/or SDK limitation.
