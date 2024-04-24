# Remote Control Driver
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2022-01-01 | [pjsg](https://github.com/pjsg) | [pjsg](https://github.com/pjsg) | [rmt.c](../../components/modules/rmt.c)|

The RMT module provides a simple interface onto the ESP32 RMT peripheral. This allows the generation of
arbitrary pulse trains with good timing accuracy. This can be used to generate IR remote control signals, or
servo control pulses, or pretty much any high speed signalling system.  It isn't good for low speed stuff as the maximum
pulse time is under 200ms -- though you can get longer by having multiple large values in a row. See the Data Encoding
below for more details.

## rmt.txsetup(gpio, bitrate, options)

This sets up a transmit channel on the specified gpio pin at the specified rate. Various options described below
can be specified in the `options`. The bit time is specified in picoseconds so that integer values can be used.

An error will be thrown if the bit time cannot be approximated.

#### Syntax
`channel = rmt.txsetup(gpio, bittime[, options])`

#### Parameters
- `gpio` The GPIO pin number to use.
- `bittime` The bit time to use in picoseconds. Only certain times can be handled exactly. The actual set time will be returned. The actual range is limited -- probably using 100,000 (0.1&micro;S) or 1,000,000 (1&micro;S). The actual constraint is that the interval is 1 - 255 cycles of an 80MHz clock.
- `options` A table with the keys as defined below.

##### Returns
- The `rmt.channel` object that can be used for sending data
- The actual bit time in picoseconds.

#### Example
```lua
```

#### Options table

This optional table consists of a number of keys that control various aspects of the RMT transmission.

- `invert` if true, then the output is inverted.
- `carrier_hz` specifies that the signal is to modulate the carrier at the specified frequency. This is useful for IR transmissions.
- `carrier_duty` specifies the duty cycle of the carrier. Defaults to 50%
- `idle_level` specifies what value to send when the transmission completes.
- `slots` If specified, then the number of memory slots used for transmission. 1 slot = 64 pulses (i.e. high and low widths). Total slots in the system are 8 (on the ESP32). Slots cannot be shared.

## rmt.rxsetup(gpio, bitrate, options)

This sets up a receive channel on the specified gpio pin at the specified rate. Various options described below
can be specified in the `options`. The bit time is specified in picoseconds so that integer values can be used.

An error will be thrown if the bit time cannot be approximated.

#### Syntax
`channel = rmt.rxsetup(gpio, bittime[, options])`

#### Parameters
- `gpio` The GPIO pin number to use.
- `bittime` The bit time to use in picoseconds. Only certain times can be handled exactly. The actual set time will be returned. The actual range is limited -- probably using 100,000 (0.1&micro;S) or 1,000,000 (1&micro;S). The actual constraint is that the interval is 1 - 255 cycles of an 80MHz clock.
- `options` A table with the keys as defined below.

##### Returns
- The `rmt.channel` object that can be used for receiving data
- The actual bit time in picoseconds.

#### Example
```lua
```

#### Options table

This optional table consists of a number of keys that control various aspects of the RMT transmission.

- `invert` if true, then the input is inverted.
- `filter_ticks` If specified, then any pulse shorter than this will be ignored. This is in units of the bit time.
- `idle_threshold` If specified, then any level longer than this will set the receiver as idle. The default is 65535 bit times.
- `slots` If specified, then the number of memory slots used for reception. 1 slot = 64 pulses (i.e. high and low widths). Total slots in the system are 8 (on the ESP32). Slots cannot be shared.


## channel:on(event, callback)

This is establishes a callback to use when data is received and it also starts the data reception process. It can only be called once per receive
channel.

#### Syntax
`channel:on(event, callback)`

#### Parameters
- `event` This must be the string 'data' and it sets the callback that gets invoked when data is received.
- `callback` This is invoked with a single argument that is a string that contains the data received in the format described for `send` below. `struct.unpack` is your friend.

#### Returns
`nil`

## channel:send(data, cb)

This is a (default) blocking call that transmits the data using the parameters specified on the `txsetup` call.

#### Syntax
`channel:send(data[, cb])`

#### Parameters
- `data` This is either a string or a table of strings.
- `cb` This is an optional callback when the transmission is actually complete. If specified, then the `send` call is non-blocking, and the callback invoked when the transmission is complete. Otherwise the `send` call is synchronous and does not return until transmission is complete.

#### Data Encoding

If the `data` supplied is a table (really an array), then the elements of the table are concatenated together and sent. The elements of the table must be strings.

If the item being sent is a string, then it contains 16 bit packed integers. The top bit of the integer controls the output level.
`struct.pack("H", value)` generates a suitable value to output a zero bit. `struct.pack("H", 32768 + value)` generates a one bit of the specified width.

The widths are in units of the interval specified when the channel was setup.


#### Returns
`nil`

#### Example

This example sends a single R character at 19200 bps. You wouldn't actually want to do it this way.... In some applications this would be inverted.

```
channel = rmt.txsetup(25, 1000000000 / 19200, {idle_level=1})
one = struct.pack("H", 32768 + 1000)
zero = struct.pack("H", 1000)
-- Send start bit, then R = 0x52 (reversed) then stop bit
channel:send(zero .. zero .. one .. zero .. zero .. one .. zero .. one .. zero .. one)
-- or using the table interface
channel:send({zero, zero, one, zero, zero, one, zero, one, zero, one})
```

## channel:close()

This shuts down the RMT channel and makes it available for other uses (e.g. ws2812). The channel cannot be used after this call returns. The channel
is also released when the garbage collector frees it up. However you should always `close` the channel explicitly as otherwise you can run out of RMT channels
before the garbage collector frees some up.

#### Syntax
`channel:close()`

#### Returns
`nil`
