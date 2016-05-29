# RTC User Memory Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-25 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [rtcmem.c](../../../app/modules/rtcmem.c)|

The rtcmem module provides basic access to the [RTC](https://en.wikipedia.org/wiki/Real-time_clock) (Real Time Clock) memory.

The RTC in the ESP8266 contains memory registers which survive a deep sleep, making them highly useful for keeping state across sleep cycles. Some of this memory is reserved for system use, but 128 slots (each 32bit wide) are available for application use. This module provides read and write access to these.

Due to the very limited amount of memory available, there is no mechanism for arbitrating use of particular slots. It is up to the end user to be aware of which memory is used for what, and avoid conflicts. Note that some Lua modules lay claim to certain slots.

This is a companion module to the [rtctime](rtctime.md) and [rtcfifo](rtcfifo.md) modules.

## rtcmem.read32()

Reads one or more 32bit values from RTC user memory.

#### Syntax
`rtcmem.read32(idx [, num])`

#### Parameters
  - `idx` zero-based index to start reading from
  - `num` number of slots to read (default 1)

#### Returns
The value(s) read from RTC user memory.

If `idx` is outside the valid range [0,127] this function returns nothing.

If `num` results in overstepping the end of available memory, the function only returns the data from the valid slots.

#### Example
```lua
val = rtcmem.read32(0) -- Read the value in slot 0
val1, val2 = rtcmem.read32(42, 2) -- Read the values in slots 42 and 43
```
#### See also
[`rtcmem.write32()`](#rtcmemwrite32)

## rtcmem.write32()

Writes one or more values to RTC user memory, starting at index `idx`.

Writing to indices outside the valid range [0,127] has no effect.

#### Syntax
`rtcmem.write32(idx, val [, val2, ...])`

#### Parameters
  - `idx` zero-based index to start writing to. Auto-increments if multiple values are given.
  - `val` value to store (32bit)
  - `val2...` additional values to store (optional)

#### Returns
`nil`

#### Example
```lua
rtcmem.write32(0, 53) -- Store the value 53 in slot 0
rtcmem.write32(42, 2, 5, 7) -- Store the values 2, 5 and 7 into slots 42, 43 and 44, respectively.
```
#### See also
[`rtcmem.read32()`](#rtcmemread32)