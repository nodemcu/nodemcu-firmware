# I2S Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-29 | zelll | [Arnim Läuger](https://github.com/devsaurus) | [i2s.c](../../components/modules/i2s.c)|

The I2S module provides access to the in-built two I2S controllers.

!!! note "DAC mode configuration"
    DACs are only available for DAC built-in mode on I2S peripheral 0.

!!! note "ADC mode configuration"
    Only ADC1 is available for ADC built-in mode.


## i2s.mute()
Mute the I2S channel. The hardware buffer is instantly filled with silence.

#### Syntax
`i2s.mute(i2s_num)`

#### Parameters
- `i2s_num` I2S peripheral 0 or 1

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the i2s driver failed.


## i2s.read()
Read data from I2S receive buffer.

#### Syntax
`i2s.read(i2s_num, size[, wait_ms])`

#### Parameters
- `i2s_num` I2S peripheral 0 or 1
- `size` Bytes to read
- `wait_ms` Millisecond to wait if data is not ready. Optional, defaults to 0 (not to wait) when omitted.

#### Returns
Data read from data-in pin. If data is not ready in `wait_ms` millisecond, less than `size` bytes can be returned.

An error is thrown in case of invalid parameters or if the i2s driver failed.


## i2s.start()
Configuration and start I2S bus.

#### Syntax
```lua
i2s.start(i2s_num, cfg, cb)
```

#### Parameters
- `i2s_num` I2S peripheral 0 or 1
- `cfg` table containing configuration data:
    - `mode` I2S work mode. Optional, defaults to `i2s.MODE_MASTER + i2s.MODE_TX` when omitted.
        - `i2s.MODE_MASTER`
        - `i2s.MODE_SLAVE`
        - `i2s.MODE_TX`
        - `i2s.MODE_RX`
        - `i2s.MODE_DAC_BUILT_IN`
        - `i2s.MODE_ADC_BUILT_IN`
        - `i2s.MODE_PDM`
    - `rate` audio sample rate. Optional, defauls to 44100 when omitted.
    - `bits` bits per sample. Optional, defaults to 16 when omitted.
    - `channel` channel format of I2S stream. Optional, defaults to `i2s.CHANNEL_RIGHT_LEFT` when omitted.
        - `i2s.CHANNEL_RIGHT_LEFT`
        - `i2s.CHANNEL_ALL_LEFT`
        - `i2s.CHANNEL_ONLY_LEFT`
        - `i2s.CHANNEL_ALL_RIGHT`
        - `i2s.CHANNEL_ONLY_RIGHT`
    - `format` communication format. Optional, defaults to `i2s.FORMAT_I2S_STANDARD` when omitted.
        - `i2s.FORMAT_I2S_STANDARD`
        - `i2s.FORMAT_I2S_MSB`
        - `i2s.FORMAT_PCM_SHORT`
        - `i2s.FORMAT_PCM_LONG`
    - `buffer_count` number of dma buffers. Optional, defaults to 2 when omitted.
    - `buffer_len` size of one dma buffer. Optional, defaults to rate/100 when omitted.
    - `bck_pin` clock pin, optional
    - `ws_pin` WS pin, optional
    - `data_out_pin` data output pin, optional
    - `data_in_pin` data input pin, optional
    - `dac_mode` DAC mode configuration. Optional, defaults to `i2s.DAC_CHANNEL_DISABLE` when omitted.
        - `i2s.DAC_CHANNEL_DISABLE`
        - `i2s.DAC_CHANNEL_RIGHT`
        - `i2s.DAC_CHANNEL_LEFT`
        - `i2s.DAC_CHANNEL_BOTH`
    - `adc1_channel` ADC1 channel number 0..7. Optional, defaults to off when omitted.
- `cb` function called when transmit data is requested or received data is available
    - the function is called with parameters `i2s_num` and `dir`
        - `dir` is "tx" for TX data request. Function shall call `i2s.write()`.
        - `dir` is "rx" for RX data available. Function shall call `i2s.read()`.

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the i2s driver failed.


## i2s.stop()
Stop I2S bus.

#### Syntax
`i2s.stop(i2s_num)`

#### Parameters
- `i2s_num` I2S peripheral 0 or 1

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the i2s driver failed.


## i2s.write()
Write to I2S transmit buffer.

#### Syntax
`i2s.write(i2s_num, data)`

#### Parameters
- `i2s_num` I2S peripheral 0 or 1
- `data` string containing I2S stream data

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the channel failed.
