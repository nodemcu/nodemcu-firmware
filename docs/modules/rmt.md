# Remote Control Driver 
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2022-01-01 | [pjsg](https://github.com/pjsg) | [pjsg](https://github.com/pjsg) | [rmt.c](../../components/modules/rmt.c)|

The RMT module provides a simple interface onto the ESP32 RMT peripheral. This allows the generation of 
arbitrary pulse trains with good timing accuracy. This can be used to generate IR remote control signals.

## rmt.txsetup(gpio, bitrate, options)

This sets up a transmit channel on the specified gpio pin at the specified rate. Various options described below
can be specified in the `options`. The bit time is specified in picoseconds so that integer values can be used.

An error will be thrown if the bit time cannot be approximated.

#### Syntax
`tx = rmt.txsetup(gpio, bittime[, options])`

#### Parameters
- `gpio` The GPIO pin number to use.
- `bittime` The bit time to use in picoseconds. Only certain times can be handled exactly. The actual set time will be returned. The actual range is limited -- probably using 100,000 (0.1&micro;S) or 1,000,000 (1&micro;S).
- `options` A table with the keys as defined below.

##### Returns
- The `rmt_txchannel` object that can be used for sending data
- The actual bit time in picoseconds.

#### Example
```lua
```

#### Options table

This optional table consists of a number of keys that control various aspects of the RMT transmission.

- `invert` if true, then the output is inverted.
- `carrier_hz` specifies that the signal is to module the carrier at the specified frequency. This is useful for IR transmissions.
- `carrier_duty` specifies the duty cycle of the carrier. Defaults to 50%
- `idle_level` specifies what value to send when the transmission completes. 

## tx:send(data)

This is a blocking call that transmits the data using the parameters specified on the `txsetup` call. 

#### Syntax
`tx:send(data)`

#### Parameters
- `data` This is either a string or a list of strings or integers. 

#### Data Encoding

If the `data` supplied is a table, then the elements of the table are concatenated together and sent. 

If the item being sent is an integer, then it is encoded as `(bit count << 1) | value` where bit count is in the range 1 - 32767 and the value is either 0 or 1 which is the value to transmit for that many bit times.

If the item being sent is a string, then it contains 16 bit packed integers. 

Note that there must be an overall even number of these entries.

#### Returns
`nil`


## tx:close()

This shuts down the RMT channel and makes it available for other uses (e.g. ws2812). The channel cannot be used after this call returns.

#### Syntax
`tx:close()`

#### Returns
`nil`

## rmt.rxsetup(gpio, bitrate, options)

This sets up a receive channel on the specified gpio pin at the specified rate. Various options described below
can be specified in the `options`. The bit time is specified in picoseconds so that integer values can be used.

An error will be thrown if the bit time cannot be approximated.

#### Syntax
`rx = rmt.rxsetup(gpio, bittime[, options])`

#### Parameters
- `gpio` The GPIO pin number to use.
- `bittime` The bit time to use in picoseconds. Only certain times can be handled exactly. The actual set time will be returned. The actual range is limited -- probably using 100,000 (0.1&micro;S) or 1,000,000 (1&micro;S).
- `options` A table with the keys as defined below.

##### Returns
- The `rmt_rxchannel` object that can be used for receiving data
- The actual bit time in picoseconds.

#### Example
```lua
```

#### Options table

This optional table consists of a number of keys that control various aspects of the RMT transmission.

- `filter_ticks` If specified, then any pulse shorter than this will be ignored.
- `idle_threshold` If specified, then any pulse longer than this will set the receiver as idle (??).


## rx:on(method, callback)

This is establishes a callback to use when data is received and it also starts the data reception process.

#### Syntax
`rx:on(event, callback)`

#### Parameters
- `event` This must be the string 'data' and it sets the callback that gets invoked when data is received.
- `callback` This is invoked with a single argument that is a string that contains the data received in the format described for transmit above.

#### Returns
`nil`

## rx:close()

This shuts down the RMT channel and makes it available for other uses (e.g. ws2812). The channel cannot be used after this call returns.

#### Syntax
`rx:close()`

#### Returns
`nil`
