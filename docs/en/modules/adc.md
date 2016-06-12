# ADC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-24 | [Zeroday](https://github.com/funshine) | [jmattsson](https://github.com/jmattsson) | [adc.c](../../../app/modules/adc.c)|

The ADC module provides access to the in-built ADC.

On the ESP8266 there is only a single-channel, which is multiplexed with the battery voltage. Depending on the setting in the "esp init data" (byte 107) one can either use the ADC to read an external voltage, or to read the system voltage (vdd33), but not both.

Which mode to use the ADC in can be configured via the `adc.force_init_mode()` function. Note that after switching from one to the other a system restart (e.g. power cycle, reset button, [`node.restart()`](node.md#noderestart)) is required before the change takes effect.

## adc.force_init_mode()

Checks and if necessary reconfigures the ADC mode setting in the ESP init data block.

####Syntax
`adc.force_init_mode(mode_value)`

####Parameters
`mode_value` One of `adc.INIT_ADC` or `adc.INIT_VDD33`.

####Returns
True if the function had to change the mode, false if the mode was already configured. On a true return the ESP needs to be restarted for the change to take effect.

####Example
```lua
-- in you init.lua:
if adc.force_init_mode(adc.INIT_VDD33)
then
  node.restart()
  return -- don't bother continuing, the restart is scheduled
end

print("System voltage (mV):", adc.readvdd33(0))
```

####See also
[`node.restart()`](node.md#noderestart)

## adc.read()

Samples the ADC.

####Syntax
`adc.read(channel)`

####Parameters
`channel` always 0 on the ESP8266

####Returns
the sampled value (number)

If the ESP8266 has been configured to use the ADC for reading the system voltage, this function will always return 65535. This is a hardware and/or SDK limitation.

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
