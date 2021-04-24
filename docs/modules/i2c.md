# I²C Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [i2c.c](../../app/modules/i2c.c)|
| 2018-08-30 | [Natalia Sorokina](https://github.com/sonaux) |  | [i2c_master.c](../../app/driver/i2c_master.c)|

I²C (I2C, IIC) is a serial 2-wire bus for communicating with various devices. Also known as SMBus or TWI, though SMBus have some additions to the I2C protocol.
ESP8266 chip does not have hardware I²C, so module uses software I²C driver.
It can be set up on any GPIO pins including GPIO16 (see below).

This module supports:

- Master mode
- Multiple buses (up to 10) with different speeds on each bus
- Standard(Slow, 100kHz), Fast(400kHz) and FastPlus(1MHz) modes or an arbitrary clock speed
- Clock stretching (slow slave device can tell the master to wait)
- Sharing SDA line over multiple I²C buses to save available pins
- GPIO16 pin can be used as SCL pin, but selected bus will be limited to not more than FAST speed.

HIGH-speed mode (3.5MHz clock) and 10-bit addressing scheme is not supported.

You have to call `i2c.setup` on a given I²C bus at least once before communicating to any device connected to that bus, otherwise you will get an error.

I²C bus designed to work in open-drain mode, so it needs pull-up resistors 1k - 10k on SDA and SCL lines. Though many peripheral modules have pull-up resistors onboard and will work without additional external resistors.

Hint for using many identical devices with same address:
Many devices allow to choose between 2 I²C addresses via pin or soldered 0 Ohm resistor.
If address change is not an option or you need to use more than 2 similar devices, you can use different I²C buses.
Initialize them once by calling `i2c.setup` with different bus numbers and pins, then refer to each device by bus id and device address.
SCL pins should be different, SDA can be shared on one pin.

Note that historically many NodeMCU drivers and modules assumed that only a single I²C bus with id 0 is available, so it is always safer to start with id 0 as first bus in your code.
If your device driver functions do not have I²C bus id as an input parameter and/or not built with Lua OOP principles then most probably device will be accessible through bus id 0 only and must be connected to its pins.

To enable new driver comment line `#define I2C_MASTER_OLD_VERSION` in `user_config.h`

To enable support for GPIO16 (D0) uncomment line `#define I2C_MASTER_GPIO16_ENABLED` in `user_config.h`

GPIO16 does not support open-drain mode and works in push-pull mode. That may lead to communication errors when slave device tries to stretch SCL clock but unable to hold SCL line low. If that happens, try setting lower I²C speed.

!!! caution

    If your module reboots when trying to use GPIO16 pin, then it is wired to RESET pin to support deep sleep mode and you cannot use GPIO16 for I²C bus or other purposes.


## i2c.address()
Setup I²C address and read/write mode for the next transfer.
On I²C bus every device is addressed by 7-bit number. Address for the particular device can be found in its datasheet.

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

-- initialize i2c, set pin 1 as sda, set pin 2 as scl
i2c.setup(id, sda, scl, i2c.FAST)

-- user defined function: read 1 byte of data from device
function read_reg(id, dev_addr, reg_addr)
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
reg = read_reg(id, 0x77, 0xAA)
print(string.byte(reg))
```

#### See also
[i2c.write()](#i2cwrite)

## i2c.setup()
Initialize the I²C bus with the selected bus number, pins and speed.

#### Syntax
`i2c.setup(id, pinSDA, pinSCL, speed)`

#### Parameters
- `id` 0~9, bus number
- `pinSDA` 1~12, IO index
- `pinSCL` 0~12, IO index
- `speed` `i2c.SLOW` (100kHz), `i2c.FAST` (400kHz), `i2c.FASTPLUS` (1MHz) or any clock frequency in range of 25000-1000000 Hz.
FASTPLUS mode results in 600kHz I2C clock speed at default 80MHz CPU frequency. To get 1MHz I2C clock speed change CPU frequency to 160MHz with function `node.setcpufreq(node.CPU160MHZ)`.

#### Returns
`speed` the selected speed, `0` if bus initialization error.

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
node.setcpufreq(node.CPU160MHZ) -- to support FASTPLUS speed
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
id  = 0
sda = 1
scl = 2

-- initialize i2c, set pin 1 as sda, set pin 2 as scl
i2c.setup(id, sda, scl, i2c.FAST)

-- user defined function: write some data to device
-- with address dev_addr starting from reg_addr
function write_reg(id, dev_addr, reg_addr, data)
    i2c.start(id)
    i2c.address(id, dev_addr, i2c.TRANSMITTER)
    i2c.write(id, reg_addr)
    c = i2c.write(id, data)
    i2c.stop(id)
    return c
end
-- set register with address 0x45 of device 0x77 with value 1
count = write_reg(id, 0x77, 0x45, 1)
print(count, " bytes written")

-- write text into i2c EEPROM starting with memory address 0
count = write_reg(id, 0x50, 0, "Sample")
print(count, " bytes written")
```

#### See also
[i2c.read()](#i2cread)
