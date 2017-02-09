# RTC FIFO Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-26 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson), Bernd Meyer <bmeyer@dius.com.au> | [Johny Mattsson](https://github.com/jmattsson) | [rtcfifo.c](../../../app/modules/rtcfifo.c)|

The rtcfifo module implements a first-in,first-out storage intended for sensor readings. As the name suggests, it is backed by the [RTC](https://en.wikipedia.org/wiki/Real-time_clock) user memory and as such survives deep sleep cycles. Conceptually it can be thought of as a cyclic array of `{ timestamp, name, value }` tuples. Internally it uses a space-optimized storage format to allow the greatest number of samples to be kept. This comes with several trade-offs, and as such is not a one-solution-fits-all. Notably:

- Timestamps are stored with second-precision.
- Sample frequency must be at least once every 8.5 minutes. This is a side-effect of delta-compression being used for the time stamps.
- Values are limited to 16 bits of precision, but have a separate field for storing an E<sup>-n</sup> multiplier. This allows for high fidelity even when working with very small values. The effective range is thus 1E<sup>-7</sup> to 65535.
- Sensor names are limited to a maximum of 4 characters.

!!! important

	This module uses two sets of RTC memory slots, 10-20 for its control block, and a variable number of slots for samples and sensor names. By default these span 32-127, but this is configurable. Slots are claimed when [`rtcfifo.prepare()`](#rtcfifoprepare) is called.

This is a companion module to the [rtcmem](rtcmem.md) and [rtctime](rtctime.md) modules.

## rtcfifo.dsleep_until_sample()

When the rtcfifo module is compiled in together with the rtctime module, this convenience function is available. It allows for some measure of separation of concerns, enabling writing of modularized Lua code where a sensor reading abstraction may not need to be aware of the sample frequency (which is largely a policy decision, rather than an intrinsic of the sensor). Use of this function is effectively equivalent to [`rtctime.dsleep_aligned(interval_us, minsleep_us)`](rtctime.md#rtctimedsleep_aligned) where `interval_us` is what was given to [`rtcfifo.prepare()`](#rtcfifoprepare).

####Syntax
`rtcfifo.dsleep_until_sample(minsleep_us)`

####Parameter
`minsleep_us` minimum sleep time, in microseconds

####Example
```lua
-- deep sleep until it's time to take the next sample
rtcfifo.dsleep_until_sample(0)
```

####See also
[`rtctime.dsleep_aligned()`](rtctime.md#rtctimedsleep_aligned)

## rtcfifo.peek()

Reads a sample from the rtcfifo. An offset into the rtcfifo may be specified, but by default it reads the first sample (offset 0).

####Syntax:
`rtcfifo.peek([offset])`

####Parameters
`offset` Peek at sample at position `offset` in the fifo. This is a relative offset, from the current head. Zero-based. Default value is 0.

####Returns
The values returned match the input arguments used to [`rtcfifo.put()`](#rtcfifoput).

- `timestamp` timestamp in seconds
- `value` the value
- `neg_e` scaling factor
- `name` sensor name

If no sample is available (at the specified offset), nothing is returned.

####Example
```lua
local timestamp, value, neg_e, name = rtcfifo.peek()
```

## rtcfifo.pop()

Reads the first sample from the rtcfifo, and removes it from there.

####Syntax:
`rtcfifo.pop()`

####Parameters
none

####Returns
The values returned match the input arguments used to [`rtcfifo.put()`](#rtcfifoput).

- `timestamp` timestamp in seconds
- `value` the value
- `neg_e` scaling factor
- `name` sensor name

####Example
```lua
while rtcfifo.count() > 0 do
  local timestamp, value, neg_e, name = rtcfifo.pop()
  -- do something with the sample, e.g. upload to somewhere
end
```

## rtcfifo.prepare()

Initializes the rtcfifo module for use.

Calling [`rtcfifo.prepare()`](#rtcfifoprepare) unconditionally re-initializes the storage - any samples stored are discarded.

####Syntax
`rtcfifo.prepare([table])`

####Parameters
This function takes an optional configuration table as an argument. The following items may be configured:

- `interval_us` If wanting to make use of the [`rtcfifo.sleep_until_sample()`](#rtcfifosleep_until_sample) function, this field sets the sample interval (in microseconds) to use. It is effectively the first argument of [`rtctime.dsleep_aligned()`](rtctime.md#rtctimedsleep_aligned).
- `sensor_count` Specifies the number of different sensors to allocate name space for. This directly corresponds to a number of slots reserved for names in the variable block. The default value is 5, minimum is 1, and maximum is 16.
- `storage_begin` Specifies the first RTC user memory slot to use for the variable block. Default is 32. Only takes effect if `storage_end` is also specified.
- `storage_end` Specified the end of the RTC user memory slots. This slot number will *not* be touched. Default is 128. Only takes effect if `storage_begin` is also specified.


####Returns
`nil`

####Example
```lua
-- Initialize with default values
rtcfifo.prepare()
-- Use RTC slots 19 and up for variable storage
rtcfifo.prepare({storage_begin=21, storage_end=128})
```

####See also
- [`rtcfifo.ready()`](#rtcfifoready)
- [`rtcfifo.prepare()`](#rtcfifoprepare)

## rtcfifo.put()

Puts a sample into the rtcfifo.

If the rtcfifo has not been prepared, this function does nothing.

####Syntax
`rtcfifo.put(timestamp, value, neg_e, name)`

####Parameters
- `timestamp` Timestamp in seconds. The timestamp would typically come from [`rtctime.get()`](rtctime.md#rtctimeget).
- `value` The value to store.
- `neg_e` The effective value stored is valueE<sup>neg_e</sup>.
- `name` Name of the sensor.  Only the first four (ASCII) characters of `name` are used.

Note that if the timestamp delta is too large compared to the previous sample stored, the rtcfifo evicts all earlier samples to store this one. Likewise, if `name` would mean there are more than the `sensor_count` (as specified to [`rtcfifo.prepare()`](#rtcfifoprepare)) names in use, the rtcfifo evicts all earlier samples.

####Returns
`nil`

####Example
```lua
-- Obtain a sample value from somewhere
local sample = ... 
-- Store sample with no scaling, under the name "foo"
rtcfifo.put(rtctime.get(), sample, 0, "foo")
```

## rtcfifo.ready()

Returns non-zero if the rtcfifo has been prepared and is ready for use, zero if not.

####Syntax:
`rtcfifo.ready()`

####Parameters
none

####Returns
Non-zero if the rtcfifo has been prepared and is ready for use, zero if not.

####Example
```lua
-- Prepare the rtcfifo if not already done
if not rtcfifo.ready() then
  rtcfifo.prepare()
end
```