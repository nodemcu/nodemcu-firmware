# 1-Wire Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [ow.c](../../../app/modules/ow.c)|

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

####See also
- [ow.write()](#owwrite)
- [ow.write_bytes()](#owwrite_bytes)

## ow.read()
Reads a byte.

####Syntax
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
`ow.search(pin)`

#### Parameters
`pin` 1~12, I/O index

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
pin = 9
ow.setup(pin)
count = 0
repeat
  count = count + 1
  addr = ow.reset_search(pin)
  addr = ow.search(pin)
  tmr.wdclr()
until (addr ~= nil) or (count > 100)
if addr == nil then
  print("No more addresses.")
else
  print(addr:byte(1,8))
  crc = ow.crc8(string.sub(addr,1,7))
  if crc == addr:byte(8) then
    if (addr:byte(1) == 0x10) or (addr:byte(1) == 0x28) then
      print("Device is a DS18S20 family device.")
        repeat
          ow.reset(pin)
          ow.select(pin, addr)
          ow.write(pin, 0x44, 1)
          tmr.delay(1000000)
          present = ow.reset(pin)
          ow.select(pin, addr)
          ow.write(pin,0xBE,1)
          print("P="..present)  
          data = nil
          data = string.char(ow.read(pin))
          for i = 1, 8 do
            data = data .. string.char(ow.read(pin))
          end
          print(data:byte(1,9))
          crc = ow.crc8(string.sub(data,1,8))
          print("CRC="..crc)
          if crc == data:byte(9) then
             t = (data:byte(1) + data:byte(2) * 256) * 625
             t1 = t / 10000
             t2 = t % 10000
             print("Temperature="..t1.."."..t2.."Centigrade")
          end                   
          tmr.wdclr()
        until false
    else
      print("Device family is not recognized.")
    end
  else
    print("CRC is not valid!")
  end
end
```

####See also
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

####See also
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

####See also
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

####See also
[ow.depower()](#owdepower)
