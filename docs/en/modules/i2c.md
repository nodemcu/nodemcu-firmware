# I²C Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [i2c.c](../../../app/modules/i2c.c)|

## i2c.address()
Setup I²C address and read/write mode for the next transfer.

#### Syntax
`i2c.address(id, device_addr, direction)`

#### Parameters
- `id` always 0
- `device_addr` 7-bit device address, remember that [in I²C `device_addr` represents the upper 7 bits](http://www.nxp.com/documents/user_manual/UM10204.pdf#page=13) followed by a single `direction` bit
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
- `id` always 0
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

####See also
[i2c.write()](#i2cwrite)

## i2c.setup()
Initialize the I²C module.

#### Syntax
`i2c.setup(id, pinSDA, pinSCL, speed)`

####Parameters
- `id` always 0
- `pinSDA` 1~12, IO index
- `pinSCL` 1~12, IO index
- `speed` only `i2c.SLOW` supported

#### Returns
`speed` the selected speed

####See also
[i2c.read()](#i2cread)

## i2c.start()
Send an I²C start condition.

#### Syntax
`i2c.start(id)`

#### Parameters
`id` always 0

#### Returns
`nil`

####See also
[i2c.read()](#i2cread)

## i2c.stop()
Send an I²C stop condition.

#### Syntax
`i2c.stop(id)`

####Parameters
`id` always 0

#### Returns
`nil`

####See also
[i2c.read()](#i2cread)

## i2c.write()
Write data to I²C bus. Data items can be multiple numbers, strings or Lua tables.

####Syntax
`i2c.write(id, data1[, data2[, ..., datan]])`

####Parameters
- `id` always 0
- `data` data can be numbers, string or Lua table.

#### Returns
`number` number of bytes written

#### Example
```lua
i2c.write(0, "hello", "world")
```

#### See also
[i2c.read()](#i2cread)
