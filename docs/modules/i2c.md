# I²C Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [i2c.c](../../../app/modules/i2c.c)|
| 2018-08-30 | [Natalia Sorokina](https://github.com/sonaux) |  | [i2c_master.c](../../../app/driver/i2c_master.c)|

I²C (I2C, IIC) is a serial 2-wire bus for communicating with various devices.
ESP8266 chip does not have hardware I²C, so module uses software I²C driver.
It can be set up on any GPIO pins including GPIO16.

This module supports:
 - Master mode
 - Multiple buses (up to 10) with different speeds on each bus
 - Standard(Slow, 100kHz), Fast(400kHz) and FastPlus(1MHz) modes or an arbitrary clock speed
 - Clock stretching (slow slave device can tell the master to wait)
 - Sharing SDA line over multiple I²C buses to save available pins
 - GPIO16 pin can be used as SCL pin, but it does not support clock stretching and selected bus will be limited to FAST speed.

You have to call `i2c.setup` on a given I²C bus at least once before communicating to any device connected to that bus, otherwise you will get an error.

I²C bus designed to work in open-drain mode, so it needs pull-up resistors 1k - 10k on SDA and SCL lines. Though most devices have internal pull-up resistors and they will work without external resistors.

Hint for using many identical devices with same address: initialize many I²C buses calling `i2c.setup` with different bus numbers and pins. SCL pins should be different, SDA can be shared on one pin.

Note that historically many drivers and modules assumed that only a single I²C bus with id 0 is available, so is always safer to start with id 0 as first bus in your code.
If your device driver functions do not have I²C bus id as an input parameter and/or not built with Lua OOP principles then most probably device will only be accessible through bus id 0 and must be connected to its pins.

## i2c.address()
Setup I²C address and read/write mode for the next transfer.

#### Syntax
`i2c.address(id, device_addr, direction)`

#### Parameters
- `id` bus number
- `device_addr` 7-bit device address. Remember that [in I²C `device_addr` represents the upper 7 bits](http://www.nxp.com/documents/user_manual/UM10204.pdf#page=13) followed by a single `direction` bit. Sometimes device address is advertised as 8-bit value, then you should divide it by 2 to get 7-bit value.
- `direction` `i2c.TRANSMITTER` for writing mode , `i2c. RECEIVER` for reading mode

#### Returns
`true` if ack received, `false` if no ack received.

#### See also
[i2c.read()](#i2cread)

## i2c.read()
Read data for variable number of bytes.

#### Syntax
`i2c.read(id, len)`

#### Parameters
- `id` bus number
- `len` number of data bytes

#### Returns
`string` of received data

#### Example
```lua
id  = 0
sda = 1
scl = 2

-- initialize i2c, set pin1 as sda, set pin2 as scl
i2c.setup(id, sda, scl, i2c.SLOW)

-- user defined function: read from reg_addr content of dev_addr
function read_reg(dev_addr, reg_addr)
    i2c.start(id)
    i2c.address(id, dev_addr, i2c.TRANSMITTER)
    i2c.write(id, reg_addr)
    i2c.stop(id)
    i2c.start(id)
    i2c.address(id, dev_addr, i2c.RECEIVER)
    c = i2c.read(id, 1)
    i2c.stop(id)
    return c
end

-- get content of register 0xAA of device 0x77
reg = read_reg(0x77, 0xAA)
print(string.byte(reg))
```

#### See also
[i2c.write()](#i2cwrite)

## i2c.setup()
Initialize the I²C module with the selected bus number, pins and speed.

#### Syntax
`i2c.setup(id, pinSDA, pinSCL, speed)`

#### Parameters
- `id` 0~9, bus number
- `pinSDA` 1~12, IO index
- `pinSCL` 1~12, IO index
- `speed` `i2c.SLOW`, `i2c.FAST`, `i2c.FASTPLUS` or any clock frequency in range of 25000-1000000 Hz.
FASTPLUS mode requires setting CPU frequency to 160MHz with function node.setcpufreq(node.CPU160MHZ), otherwise speed won't be much faster than in FAST mode.

#### Returns
`speed` the selected speed

#### Example
```lua
i2c0 = {
  id  = 0,
  sda = 1,
  scl = 0,
  speed = i2c.FAST
}
i2c1 = {
  id  = 1,
  sda = 1,
  scl = 2,
  speed = i2c.FASTPLUS
}
-- initialize i2c bus 0
i2c0.speed = i2c.setup(i2c0.id, i2c0.sda, i2c0.scl, i2c0.speed)
-- initialize i2c bus 1 with shared SDA on pin 1
i2c1.speed = i2c.setup(i2c1.id, i2c1.sda, i2c1.scl, i2c1.speed)
print("i2c bus 0 speed: ", i2c0.speed, "i2c bus 1 speed: ", i2c1.speed)
```
#### See also
[i2c.read()](#i2cread)

## i2c.start()
Send an I²C start condition.

#### Syntax
`i2c.start(id)`

#### Parameters
`id` bus number

#### Returns
`nil`

#### See also
[i2c.read()](#i2cread)

## i2c.stop()
Send an I²C stop condition.

#### Syntax
`i2c.stop(id)`

#### Parameters
`id` bus number

#### Returns
`nil`

#### See also
[i2c.read()](#i2cread)

## i2c.write()
Write data to I²C bus. Data items can be multiple numbers, strings or Lua tables.

#### Syntax
`i2c.write(id, data1[, data2[, ..., datan]])`

#### Parameters
- `id` bus number
- `data` data can be numbers, string or Lua table.

#### Returns
`number` number of bytes written

#### Example
```lua
i2c.write(0, "hello", "world")
```

#### See also
[i2c.read()](#i2cread)
