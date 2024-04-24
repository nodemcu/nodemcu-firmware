# RTC User Memory Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-25 | [DiUS](https://github.com/DiUS), [Jade Mattsson](https://github.com/jmattsson) | [PJSG](https://github.com/pjsg) | [rtcmem.c](../../components/modules/rtcmem.c)|

The rtcmem module provides basic access to the RTC memory. 

This memory is preserved while power is applied, making them highly useful for keeping state across sleep cycles. Some of this memory is reserved for system use, 
and, for compatibility with NodeMCU on the ESP8266, 128 slots (each 32bit wide) of RTC memory are reserved by this module.
This module then provides read and write access to these slots.

This module is 100% compatible with the ESP8266 version, and this means that, there is no mechanism for arbitrating use of particular slots. It is up to the end user to be aware of which memory is used for what, and avoid conflicts. Unlike the ESP8266 version, no other NodeMCU module uses any of these slots.

Note that this memory is not necessary preserved across reflashing the firmware. It is the responsibility of the
developer to deal with getting inconsistent data.

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
