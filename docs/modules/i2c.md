# I²C Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [i2c.c](../../components/modules/i2c.c)|
| 2020-01-17 | [jmdasnoy](https://github.com/jmdasnoy) | [jmdasnoy](https://github.com/jmdasnoy) | [i2c.c](../../components/modules/i2c.c)|


This module supports 3 different interfaces for I²C communication on a ESP-32.

The interface `id` can be

- `i2c.SW` software based bitbanging, master mode only, immediate execution, synchronous operation, maximum speed 100 KHz (Standard mode)
- `i2c.HW0` ESP32 hardware bus interface 0, master or slave mode, deferred execution, synchronous or asynchronous operation, maximum speed 1 MHz (Fast-mode Plus)
- `i2c.HW1` ESP32 hardware bus interface 1, master or slave mode, deferred execution, synchronous or asynchronous operation, maximum speed 1 MHz (Fast-mode Plus)

All interfaces can be used at the same time.
Interfaces can use arbitrary GPIOs for the clock (SCL) and data (SDA) lines.
The interfaces are not suitable for multi-master configurations.

In master mode, I²C communication is performed by combinations of 5 basic I²C operations, provided by the functions `i2c.start()`,`i2c.stop()`,`i2c.address()`,`i2c.write()`,`i2c.read()`.

The behaviour of these functions is quite different according to the type of interface.

For the software interface, these functions IMMEDIATELY perform the requested I²C operation and return on completion. For slow bus speeds and multi-byte reads or writes, this can tie up the CPU for a significant time.
However, any results returned reflect the effective outcome of the operation.

For the hardware interfaces, these functions do NOT trigger any immediate I²C activity but enqueue (store) a request for later, DEFERRED execution.
These functions return immediately as they do not have to wait for any I²C operation to complete.
As a consequence, any results returned do not reflect the effective outcome of the operation.
The function `i2c.transfer()` MUST be called to effectively execute the stored requests for operations.
Execution of the stored requests is performed by the hardware interface.
If `i2c.transfer()` is provided with a call-back function, it will return before completion of the enqueued I²C operations, freeing the CPU for other tasks.

#### Example using the software interface
```lua
-- check if chip is present and functional by reading a signature/chip id

-- i2c bus setup
sda = 23 -- pins as on Adafruit Huzzah32 silkscreen
scl = 22
id = i2c.SW -- software interface
speed = i2c.SLOW 

-- values for Bosch Sensortech BMP180 chip
bmpAddress = 0x77
bmpIdRegister = 0xD0
bmpChipSignature = 0x55

-- initialize i2c software interface
i2c.setup(id, sda, scl, speed)

-- attempt to read chip id and compare against expected value
function simple_check_chip(dev_address, dev_register, dev_signature)
  i2c.start(id)
  assert(i2c.address(id, dev_address, i2c.TRANSMITTER) , "!!i2c device did not ACK first address operation" )
  i2c.write(id, dev_register)
  i2c.start(id) -- repeated start condition
  assert( i2c.address(id, dev_address, i2c.RECEIVER) , "!!i2c device did not ACK second address operation" )
  if i2c.read(id, 1):byte() == dev_signature then
    print("..chip is operational")
  else
    print("!!The chip does not have the expected signature")
  end
   i2c.stop(id)
end
-- 
print("Chip check, should fail because device address is wrong")
simple_check_chip(bmpAddress+1, bmpIdRegister, bmpChipSignature)

print("Chip check, should succeed if chip is present and functional")
simple_check_chip(bmpAddress, bmpIdRegister, bmpChipSignature)

```

#### Example using the hardware interfaces
```lua
sda = 23 -- pins as on Adafruit Huzzah32 silkscreen
scl = 22
id = i2c.HW1 -- hardware interface
speed = i2c.SLOW

-- values for Bosch Sensortech BMP180 chip
bmpAddress = 0x77
bmpIdRegister = 0xD0
bmpChipSignature = 0x55

-- initialize i2c software interface
i2c.setup(id, sda, scl, speed)

-- read a single byte from the chip
function read_byte(dev_address, dev_register , callback )
  i2c.start(id)
  i2c.address(id, dev_address, i2c.TRANSMITTER)
  i2c.write(id, dev_register)
  i2c.start(id) -- repeated start condition
  i2c.address(id, dev_address, i2c.RECEIVER)
  i2c.read(id, 1)
  i2c.stop(id)
  return i2c.transfer(id, callback)
end

-- check results returned
function check(value, ack)
  if ack then
    if value:byte() == bmpChipSignature then
      print("..chip is operational")
    else
      print("!!The chip does not have the expected signature")
    end
  else
    print("!!chip did not respond")
  end
end
--
print("synchronous use")

print("Chip check, should fail because device address is wrong")
check(read_byte(bmpAddress+1, bmpIdRegister))

print("Chip check, should succeed if chip is present and functional")
check(read_byte(bmpAddress, bmpIdRegister))

print("asynchronous use")

print("Chip check, should fail because device address is wrong")
read_byte(bmpAddress+1, bmpIdRegister, check)

print("Chip check, should succeed if chip is present and functional")
read_byte(bmpAddress, bmpIdRegister, check)
```

## i2c.address()
Perform (`SW`) or enqueue (`HWx`) an I²C address operation, defining data transfer direction for the next operation (read or write).

#### Syntax
`i2c.address(id, device_addr, direction [, ack_check_en])`

#### Parameters
- `id` interface id
- `device_addr` I²C device address
- `direction` `i2c.TRANSMITTER` for write mode , `i2c.RECEIVER` for read mode
- `ack_check_en` enable check for slave ACK with `true` (default), disable check with `false`

This last, optional parameter is only relevant for for hardware interfaces `i2c.HW0` and `i2c.HW1` and defaults to `true`.
The I²C `address` operation is enqueued for later execution and this parameter will be used at that later time.
At that time, if NO slave device produces an ACK to the address operation, the default assumption is that the slave at that address is absent or not functional. Any remaining I²C operations in the queue will be ignored/flushed/discarded and the communication will be stopped.
This default queue flushing behaviour on slave NACK can be overridden by specifying `false`.

#### Returns
for interface `i2c.SW`: returns `true` if ack received, `false` if no ack received. This value should be checked to decide whether to continue communication.

for interfaces `i2c.HW0` and `i2c.HW1`: always returns `true`.


## i2c.read()
Perform (`SW`) or enqueue (`HWx`) a data read operation for a variable number of bytes.

#### Syntax
`i2c.read(id, len)`

#### Parameters
- `id` I²C interface id
- `len` number of data bytes to read

#### Returns
- for software interface `i2c.SW` : returns `string` of received data 
- for hardware interfaces id `i2c.HWx` : no value returned 

For the hardware interfaces, any values read from the bus will be returned by the `i2c.transfer()` function or the associated callback function.
For this reason, a sequence of enqueued operations may only contain one read request.

The value returned by a read operation is a string. Refer to the slave datasheet for multibyte reads/autoincrement capability, endianness and format details.


#### See also
[i2c.write()](#i2cwrite)

## i2c.setup()
Initialize the I²C interface for master mode.

#### Syntax
`i2c.setup(id, pinSDA, pinSCL, speed [,stretchfactor])`

#### Parameters
- `id` interface id
- `pinSDA` IO index, see [GPIO Overview](gpio.md#gpio-overview)
- `pinSCL` IO index, see [GPIO Overview](gpio.md#gpio-overview)
- `speed` bit rate in Hz, integer
    - `i2c.SLOW` for 100000 Hz (100 KHz a.k.a Standard Mode), this is the maximum allowed value for the `i2c.SW` interface
    - `i2c.FAST` for 400000 Hz (400 KHz a.k.a Fast Mode)
    - `i2c.FASTPLUS` for 1000000 Hz (1 MHz, a.k.a. Fast-mode Plus), this is the maximum allowed value for `i2c.HWx`interfaces
- `stretchfactor` integer multiplier for timeout

The pins declared for SDA and SCL are configured with onchip pullup resistors. These are weak pullups.
The data sheets do not specify any specific value for the pullup resistors, but they are reported to be between 10KΩ and 100KΩ with a target value of 50 KΩ.
Additional pullups are recommended especialy for faster speeds, in doubt try 4.7 kΩ.

The `speed` constants are provided for convenience but any other integer value can be used.

The last, optional parameter `stretchfactor` is only relevant for the `HWx`interfaces and defaults to 1.
The hardware interfaces have a built-in timeout, designed to recover from abnormal I²C bus conditions.
The default timeout is defined as 8 I²C SCL clock cycles. With an 80 MHz CPU clock speed and a 100 KHz this means 8*80/0.1 = 6400 CPU clock cycles i.e. 80 µS.

A busy or slow slave device can request a temporary pause of communication by keeping the SCL line line low a.k.a. clock stretching.
This clock stretching may exceed the timeout value and cause (often intermittent) errors.
This can be avoided either by using a slower bit rate or by specifying a multiplier value for the timeout.

Calling setup on an already configured hardware interface will cause panic.

Note to wizards: The I²C specifications defines a High-Speed mode allowing communication up to 3.4 Mbits/s, with the added complexity of variable clock speed and current-source pullups. The Expressif documentation states that the I²C hardware interfaces support up to 5 MHz, constrained by SDA pull-up strength. This module will object to speeds higher than 1 MHz, but your experience and feedback is welcome !

#### Returns
for interface `i2c.SW`: returns `speed` the selected speed.

for interfaces `i2c.HW0` and `i2c.HW1`:  returns `timeout` expressed as CPU clock cycles.

#### See also
[i2c.read()](#i2cread)

## i2c.start()
Perform (`SW`) or enqueue (`HWx`) an I²C start condition.

#### Syntax
`i2c.start(id)`

#### Parameters
`id` interface id

#### Returns
no returned value

#### See also
[i2c.read()](#i2cread)

## i2c.stop()
Perform (`SW`) or enqueue (`HWx`) an I²C stop condition.

#### Syntax
`i2c.stop(id)`

#### Parameters
`id` interface id

#### Returns
no returned value

#### See also
[i2c.read()](#i2cread)

## i2c.transfer()
Starts a transfer for the specified hardware module.

Providing a callback function allows the transfer to be started asynchronously in the background and `i2c.transfer()` finishes immediately, without returning any value.
Results from any data read will be provided to the callback function.

Without a callback function, the transfer is executed synchronously and `i2c.transfer()` returns when the transfer completes.
In this case, this function returns read values and an ACK flag.

#### Syntax
`i2c.transfer(id [, callback] [, to_ms])`

#### Parameters
- `id` hardware interface id only , `i2c.SW` not allowed
- `callback` optional callback function to be called when transfer finishes
- `to_ms` optional timeout for the synchronous transfer in ms, defaults to 0 (infinite)

The optional callback function should be defined to accept 2 arguments i.e. `function( data , ack )` where

- `data` is the string from a read operation during the transfer (`nil` if no read or failed ACK )
- `ack` is a boolean (`true` = ACK received).

The optional timeout parameter defaults to 0 meaning infinite and is only relevant for synchronous mode. This can be used to define an upper bound to the execution time of `i2c.transfer()`.
It specifies the maximum delay in mS before `i2c.transfer()` returns, possibly before the complete I²C set of operations is executed. 
This timeout should not be confused with the timeout specified in `i2c.setup`.


#### Returns
- for synchronous operation i.e. without any callback function, two values are returned
    - `data` string of received data (`nil` if no read or NACK)
    - `ack` true if ACK received, false for NACK
- for asynchronous operation, i.e. with a callback function, no value is returned

## i2c.write()
Perform (`SW`) or enqueue (`HWx`) data write to I²C bus.

#### Syntax
`i2c.write(id, data1 [, data2[, ..., datan]] [, ack_check_en] )`

#### Parameters
- `id` interface id
- `data` data items can be numbers, strings or lua tables.
- `ack_check_en` enable check for slave ACK with `true` (default), disable with `false`

Numbers or table elements must be in range [0,255] i.e. they must fit in a single byte.

By default, communication stops when the slave does not ACK each written byte.
This can be overridden by setting parameter `ack_check_en` to `false`.

#### Returns
`number` number of bytes written

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
