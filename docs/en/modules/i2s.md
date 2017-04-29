# I2S Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-29 | zelll | | [i2s.c](../../../components/modules/i2s.c)|

The I2S module provides access to the in-built two I2S controllers.

  
## i2s.start()
Configuration and start I2S bus.

#### Syntax
```lua
i2s.start(i2s_num, cfg = {
	mode = can.MODE_MASTER + can.MODE_TX,
	rate = 44100,
	bits = 16,
	channel = i2s.CHANNEL_RIGHT_LEFT,
	format = i2s.FORMAT_I2S + i2s.FORMAT_I2S_MSB,
	buffer_count = 2,
	buffer_len = 441,
	bck_pin = 16,
	ws_pin = 17,
	data_out_pin = 18,
	data_in_pin = -1
}, callback)
```

#### Parameters
- `i2s_num` `1` or `2`
- `cfg` configuration
  - `mode` combine of following constants `i2s.MODE_MASTER`, `i2s.MODE_SLAVE`, `i2s.MODE_TX`, `i2s.MODE_RX`, `i2s.MODE_DAC_BUILT_IN`. Default: `i2s.MODE_MASTER + i2s.MODE_TX`
  - `rate` Audio sample rate, such as `44100`, `48000`, `16000`, `800`. Default: `44100`
  - `bits` `32`, `24`, `16` or `8`. Default: `16`
  - `channel` One of following constants. Default: `i2s.CHANNEL_RIGHT_LEFT`
    - `i2s.CHANNEL_RIGHT_LEFT`
	- `i2s.CHANNEL_ALL_LEFT`
	- `i2s.CHANNEL_ONLY_LEFT`
	- `i2s.CHANNEL_ALL_RIGHT`
	- `i2s.CHANNEL_ONLY_RIGHT`
  - `format` combine of following constants `i2s.FORMAT_I2S`, `i2s.FORMAT_I2S_MSB`, `i2s.FORMAT_I2S_LSB`, `i2s.FORMAT_PCM`, `i2s.FORMAT_PCM_SHORT`, `i2s.FORMAT_PCM_LONG`.
      Default: `i2s.FORMAT_I2S + i2s.FORMAT_I2S_MSB`
  - `buffer_count` Buffer count. Default: 2
  - `buffer_len` Size of one buffer. Default: rate/100
  - `bck_pin` Clock pin. Default: `-1`
  - `ws_pin` WS pin. Default: `-1`
  - `data_out_pin` Pin for data output. Default: `-1`
  - `data_in_pin` Pin for data input. Default: `-1`
- `callback` function called when i2s event occurs.
  - `event` Event type, one of `sent`, `data`, `error`
  - `size` bytes

#### Returns
nil


## i2s.stop()
Stop I2S bus

#### Syntax
`i2s.stop(i2s_num)`

#### Parameters
- `i2s_num` `1` or `2`

#### Returns
nil


## i2s.read()
Read data from data-in

#### Syntax
`i2s.read( i2s_num, size[, wait_ms] )`

#### Parameters
- `adc_number` Only `adc.ADC1` now
- `size` Bytes to read
- `wait_ms` Millisecond to wait if data is not ready. Default: `0` (not to wait)

#### Returns
Data read from data-in pin. If data is not ready in `wait_ms` millisecond, less than `size` bytes can be returned.


## i2s.write()
Write to I2S bus

#### Syntax
`i2s.write( i2s_num, size[, wait_ms] )`

#### Parameters
- `adc_number` Only `adc.ADC1` now
- `size` Bytes to send
- `wait_ms` Millisecond to wait if DMA buffer is full. Default: `0` (not to wait)

#### Returns
Integer, bytes wrote to buffer.
