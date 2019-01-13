# DAC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-14 | [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [dac.c](../../components/modules/dac.c)|

The DAC module provides access to the two built-in Digital to Analog Converters.

Each DAC is assigned to a dedicated GPIO:
- DAC channel 1 is attached to GPIO25
- DAC channel 2 is attached to GPIO26

The DACs are 8-bit, thus the output values are restricted to the range from 0 to 255.

## dac.disable()
Disables DAC output on the related GPIO.

#### Syntax
```lua
dac.disable(channel)
```

#### Parameters
- `channel` DAC channel, one of
    - `dac.CHANNEL_1`
    - `dac.CHANNEL_2`

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the dac failed.


## dac.enable()
Enables DAC output on the related GPIO.

#### Syntax
```lua
dac.enable(channel)
```

#### Parameters
- `channel` DAC channel, one of
    - `dac.CHANNEL_1`
    - `dac.CHANNEL_2`

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the dac failed.


## dac.write()
Sets the output value of the DAC.

#### Syntax
```lua
dac.write(channel, value)
```

#### Parameters
- `channel` DAC channel, one of
    - `dac.CHANNEL_1`
    - `dac.CHANNEL_2`
- `value` output value

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the dac failed.
