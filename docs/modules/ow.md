# 1-Wire Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [ow.c](../../app/modules/ow.c)|

This module provides functions to work with the [1-Wire](https://en.wikipedia.org/wiki/1-Wire) device communications bus system.

## ow.check_crc16()
Computes the 1-Wire CRC16 and compare it against the received CRC.

#### Syntax
`ow.check_crc16(buf, inverted_crc0, inverted_crc1[, crc])`

#### Parameters
  - `buf` string value, data to be calculated check sum in string
  - `inverted_crc0` LSB of received CRC
  - `inverted_crc1` MSB of received CRC
  - `crc` CRC starting value (optional)

#### Returns
true if the CRC matches, false otherwise

## ow.crc16()
Computes a Dallas Semiconductor 16 bit CRC.  This is required to check the integrity of data received from many 1-Wire devices.  Note that the CRC computed here is **not** what you'll get from the 1-Wire network, for two reasons:

1. The CRC is transmitted bitwise inverted.
2. Depending on the endian-ness of your processor, the binary representation of the two-byte return value may have a different byte order than the two bytes you get from 1-Wire.

#### Syntax
`ow.crc16(buf[, crc])`

#### Parameters
- `buf` string value, data to be calculated check sum in string
- `crc` CRC starting value (optional)

#### Returns
the CRC16 as defined by Dallas Semiconductor

## ow.crc8()
Computes a Dallas Semiconductor 8 bit CRC, these are used in the ROM and scratchpad registers.

#### Syntax
`ow.crc8(buf)`

#### Parameters
`buf` string value, data to be calculated check sum in string

#### Returns
CRC result as byte

## ow.depower()
Stops forcing power onto the bus. You only need to do this if you used the 'power' flag to `ow.write()` or used a `ow.write_bytes()` and aren't about to do another read or write.

#### Syntax
`ow.depower(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
`nil`

#### See also
- [ow.write()](#owwrite)
- [ow.write_bytes()](#owwrite_bytes)

## ow.read()
Reads a byte.

#### Syntax
`ow.read(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
byte read from slave device

## ow.read_bytes()
Reads multi bytes.

#### Syntax
`ow.read_bytes(pin, size)`

#### Parameters
- `pin` 1~12, I/O index
- `size` number of bytes to be read from slave device (up to 256)

#### Returns
`string` bytes read from slave device

## ow.reset()
Performs a 1-Wire reset cycle.

#### Syntax
`ow.reset(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
- `1` if a device responds with a presence pulse
- `0` if there is no device or the bus is shorted or otherwise held low for more than 250 µS

## ow.reset_search()
Clears the search state so that it will start from the beginning again.

#### Syntax
`ow.reset_search(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
`nil`

## ow.search()
Looks for the next device.

#### Syntax
`ow.search(pin, [alarm_search])`

#### Parameters
- `pin` 1~12, I/O index
- `alarm_search` 1 / 0, if 0 a regular 0xF0 search is performed (default if parameter is absent), if 1 a 0xEC ALARM SEARCH is performed.

#### Returns
`rom_code` string with length of 8 upon success. It contains the rom code of slave device. Returns `nil` if search was unsuccessful.

#### See also
[ow.target_search()](#owtargetsearch)

## ow.select()
Issues a 1-Wire rom select command. Make sure you do the `ow.reset(pin)` first.

#### Syntax
`ow.select(pin, rom)`

#### Parameters
- `pin` 1~12, I/O index
- `rom` string value, len 8, rom code of the slave device

#### Returns
`nil`

#### Example
```lua
-- 18b20 Example
-- 18b20 Example
pin = 3
ow.setup(pin)
addr = ow.reset_search(pin)
addr = ow.search(pin)

if addr == nil then
  print("No device detected.")
else
  print(addr:byte(1,8))
  local crc = ow.crc8(string.sub(addr,1,7))
  if crc == addr:byte(8) then
    if (addr:byte(1) == 0x10) or (addr:byte(1) == 0x28) then
      print("Device is a DS18S20 family device.")
      tmr.create():alarm(1000, tmr.ALARM_AUTO, function()
          ow.reset(pin)
          ow.select(pin, addr)
          ow.write(pin, 0x44, 1) -- convert T command
          tmr.create():alarm(750, tmr.ALARM_SINGLE, function()
              ow.reset(pin)
              ow.select(pin, addr)
              ow.write(pin,0xBE,1) -- read scratchpad command
              local data = ow.read_bytes(pin, 9)
              print(data:byte(1,9))
              local crc = ow.crc8(string.sub(data,1,8))
              print("CRC="..crc)
              if crc == data:byte(9) then
                 local t = (data:byte(1) + data:byte(2) * 256) * 625
                 local sgn = t<0 and -1 or 1
                 local tA = sgn*t
                 local t1 = math.floor(tA / 10000)
                 local t2 = tA % 10000
                 print("Temperature="..(sgn<0 and "-" or "")..t1.."."..t2.." Centigrade")
              end
          end)
      end)
    else
      print("Device family is not recognized.")
    end
  else
    print("CRC is not valid!")
  end
end
```

#### See also
[ow.reset()](#owreset)

## ow.setup()
Sets a pin in onewire mode.

#### Syntax
`ow.setup(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
`nil`

## ow.skip()
Issues a 1-Wire rom skip command, to address all on bus.

#### Syntax
`ow.skip(pin)`

#### Parameters
`pin` 1~12, I/O index

#### Returns
`nil`

## ow.target_search()
Sets up the search to find the device type `family_code`. The search itself has to be initiated with a subsequent call to `ow.search()`.

#### Syntax
`ow.target_search(pin, family_code)`

#### Parameters
- `pin` 1~12, I/O index
- `family_code` byte for family code

#### Returns
`nil`

#### See also
[ow.search()](#owsearch)

## ow.write()
Writes a byte. If `power` is 1 then the wire is held high at the end for parasitically powered devices. You are responsible for eventually depowering it by calling `ow.depower()` or doing another read or write.

#### Syntax
`ow.write(pin, v, power)`

#### Parameters
- `pin` 1~12, I/O index
- `v` byte to be written to slave device
- `power` 1 for wire being held high for parasitically powered devices

#### Returns
`nil`

#### See also
[ow.depower()](#owdepower)

## ow.write_bytes()
Writes multi bytes. If `power` is 1 then the wire is held high at the end for parasitically powered devices. You are responsible for eventually depowering it by calling `ow.depower()` or doing another read or write.

#### Syntax
`ow.write_bytes(pin, buf, power)`

#### Parameters
- `pin` 1~12, IO index
- `buf` string to be written to slave device
- `power` 1 for wire being held high for parasitically powered devices

#### Returns
`nil`

#### See also
[ow.depower()](#owdepower)

## ow.set_timings()
Tweak different bit timing parameters. Some slow custom devices might not work perfectly well with NodeMCU as 1-wire master. Since NodeMCU ow module is bit-banging the 1-wire protocol, it is possible to adjust the timings a bit.

Note that you can break the protocol totally if you tweak some numbers too much. This should never be needed with normal devices.

#### Syntax
`ow.set_timings(reset_tx, reset_wait, reset_rx, w1_low, w1_high, w0_low, w0_high, r_low, r_wait, r_delay)`

#### Parameters
Each parameter specifies number of microseconds to delay at different stages in the 1-wire bit-banging process.
A nil value will leave the value unmodified.

- `reset_tx` pull bus low during reset (default 480)
- `reset_wait` wait for presence pulse after reset (default 70)
- `reset_rx` delay after presence pulse have been checked (default 410)
- `w1_low` pull bus low during write 1 slot (default 5)
- `w1_high` leave bus high during write 1 slot (default 52)
- `w0_low` pull bus low during write 1 slot (default 65)
- `w0_high` leave bus high during write 1 slot (default 5)
- `r_low` pull bus low during read slot (default 5)
- `r_wait` wait before reading bus level during read slot  (default 8)
- `r_delay` delay after reading bus level (default 52)

#### Returns
`nil`

#### Example
```lua
-- Give 300uS longer read/write slots for slow MCU based 1-wire slave
ow.set_timings(
	nil, -- reset_tx
	nil, -- reset_wait
	nil, -- reset_rx
	nil, -- w1_low
	352, -- w1_high
	nil, -- w0_low
	305, -- w0_high
	nil, -- r_low
	nil, -- r_wait
	352  -- r_delay
)
```
