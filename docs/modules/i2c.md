# I²C Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [i2c.c](../../components/modules/i2c.c)|

This module supports different interfaces for communicating via I²C protocol. All interfaces can be assigned to arbitrary GPIOs for SCL and SDA and can be operated concurrently.

- `i2c.SW` software based bitbanging, master mode only, synchronous operation
- `i2c.HW0` ESP32 hardware port 0, master or slave mode, synchronous or asynchronous operation
- `i2c.HW1` ESP32 hardware port 1, master or slave mode, synchronous or asynchronous operation

The hardware master interfaces differ from the SW interface as the commands (start, stop, read, write) are queued up to an internal command list. Actual I²C communication is initiated afterwards using the `i2c.transfer()` function. Commands for the `i2c.SW` interface are immediately effective on the I²C bus and read data is also instantly available.

## i2c.address()
Send (`SW`) or queue (`HWx`) I²C address and read/write mode for the next transfer.

Communication stops when the slave answers with NACK to the address byte. This can be avoided with parameter `ack_check_en` on `false`.

#### Syntax
`i2c.address(id, device_addr, direction[, ack_check_en])`

#### Parameters
- `id` interface id
- `device_addr` device address
- `direction` `i2c.TRANSMITTER` for writing mode , `i2c.RECEIVER` for reading mode
- `ack_check_en` enable check for slave ACK with `true` (default), disable with `false`

#### Returns
`true` if ack received (always for ids `i2c.HW0` and `i2c.HW1`), `false` if no ack received (only possible for `i2c.SW`).

#### See also
[i2c.read()](#i2cread)

## i2c.read()
Read (`SW`) or queue (`HWx`) data for variable number of bytes.

#### Syntax
`i2c.read(id, len)`

#### Parameters
- `id` interface id
- `len` number of data bytes

#### Returns
- `string` of received data for interface `i2c.SW`
- `nil` for ids `i2c.HW0` and `i2c.HW1`

#### Example
```lua
id  = i2c.SW
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
Initialize the I²C interface for master mode.

#### Syntax
`i2c.setup(id, pinSDA, pinSCL, speed)`

####Parameters
- `id` interface id
- `pinSDA` IO index, see [GPIO Overview](gpio.md#gpio-overview)
- `pinSCL` IO index, see [GPIO Overview](gpio.md#gpio-overview)
- `speed` bit rate in Hz, positive integer
    - `i2c.SLOW` for 100000 Hz, max for `i2c.SW`
    - `i2c.FAST` for 400000 Hz
    - `i2c.FASTPLUS` for 1000000 Hz

#### Returns
`speed` the selected speed

####See also
[i2c.read()](#i2cread)

## i2c.start()
Send (`SW`) or queue (`HWx`) an I²C start condition.

#### Syntax
`i2c.start(id)`

#### Parameters
`id` interface id

#### Returns
`nil`

####See also
[i2c.read()](#i2cread)

## i2c.stop()
Send (`SW`) or queue (`HWx`) an I²C stop condition.

#### Syntax
`i2c.stop(id)`

####Parameters
`id` interface id

#### Returns
`nil`

####See also
[i2c.read()](#i2cread)

## i2c.transfer()
Starts a transfer for the specified hardware module. Providing a callback function allows the transfer to be started asynchronously in the background and `i2c.transfer()` finishes immediately. Without a callback function, the transfer is executed synchronously and `i2c.transfer()` comes back when the transfer completed. Data from a read operation is returned from `i2c.transfer()` in this case.

First argument to the callback is a string with data obtained from a read operation during the transfer or `nil`, followed by the ack flag (true = ACK received).

#### Syntax
`i2c.transfer(id[, cb_fn][, to_ms])`

#### Parameters
- `id` interface id, `i2c.SW` not allowed
- `cb_fn(data, ack)` function to be called when transfer finished
- `to_ms` timeout for the transfer in ms, defaults to 0=infinite

#### Returns
- synchronous operation:
    - `data` string of received data (`nil` if no read or NACK)
    - `ack` true if ACK received, false for NACK
- `nil` for asynchronous operation

## i2c.write()
Write (`SW`) or queue (`HWx`) data to I²C bus. Data items can be multiple numbers, strings or lua tables.

Communication stops when the slave answers with NACK to a written byte. This can be avoided with parameter `ack_check_en` on `false`.

####Syntax
`i2c.write(id, data1[, data2[, ..., datan]][, ack_check_en])`

####Parameters
- `id` interface id
- `data` data can be numbers, string or lua table.
- `ack_check_en` enable check for slave ACK with `true` (default), disable with `false`

#### Returns
`number` number of bytes written

#### Example
```lua
i2c.write(0, "hello", "world")
```

#### See also
[i2c.read()](#i2cread)

# I²C slave mode
The I²C slave mode is only available for the hardware interfaces `i2c.HW0` and `i2c.HW1`.

## i2c.slave.on()
Registers or unregisters an event callback handler.

#### Syntax
`i2c.slave.on(id, event[, cb_fn])`

#### Parameters
- `id` interface id, `i2c.HW0` or `i2c.HW1`
- `event` one of
    - "receive" data received from master
- `cb_fn(err, data)` function to be called when data was received from the master. Unregisters previous callback for `event` when omitted.

#### Returns
`nil`

## i2c.slave.setup()
Initialize the I²C interface for slave mode.

#### Syntax
`i2c.slave.setup(id, slave_config)`

#### Parameters
- `id` interface id, `i2c.HW0` or `i2c.HW1`
- `slave_config` table containing slave configuration information
    - `sda` IO index, see [GPIO Overview](gpio.md#gpio-overview)
    - `scl` IO index, see [GPIO Overview](gpio.md#gpio-overview)
    - `addr` slave address (7bit or 10bit)
    - `10bit` enable 10bit addressing with `true`, use 7bit with `false` (optional, defaults to `false` is omitted)
    - `rxbuf_len` length of receive buffer (optional, defaults to 128 if omitted)
    - `txbuf_len` length of transmit buffer (optional, defaults to 128 if omitted)

#### Returns
`nil`

## i2c.slave.send()
Writes send data for the master into the transmit buffer. This function returns immediately if there's enough room left in the buffer. It blocks if the buffer is doesn't provide enough space.

Data items can be multiple numbers, strings or lua tables.

#### Syntax
`i2c.slave.send(id, data1[, data2[, ..., datan]])`

#### Parameters
- `id` interface id, `i2c.HW0` or `i2c.HW1`
- `data` data can be numbers, string or lua table.

#### Returns
`number` number of bytes written
