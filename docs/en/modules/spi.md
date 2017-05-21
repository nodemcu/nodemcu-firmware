# SPI Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-16 | [Ibrahim Abd Elkader](https://github.com/iabdalkader) |              [Arnim Läuger](https://github.com/devsaurus) | [spi.c](../../../app/modules/spi.c)|

All transactions for sending and receiving are most-significant-bit first and least-significant last.
For technical details of the underlying hardware refer to [metalphreak's ESP8266 HSPI articles](http://d.av.id.au/blog/tag/hspi/).

!!! note

	The ESP hardware provides two SPI busses, with IDs 0, and 1, which map to pins generally labelled SPI and HSPI. If you are using any kind of development board which provides flash, then bus ID 0 (SPI) is almost certainly used for communicating with the flash chip. You probably want to choose bus ID 1 (HSPI) for your communication, as you will have uncontended use of it.

HSPI signals are fixed to the following IO indices and GPIO pins:

| Signal    | IO index | ESP8266 pin |
|-----------|----------|-------------|
| HSPI CLK  |    5     | GPIO14      |
| HSPI /CS  |    8     | GPIO15      |
| HSPI MOSI |    7     | GPIO13      |
| HSPI MISO |    6     | GPIO12      |

See also [spi.setup()](#spisetup).

## High Level Functions
The high level functions provide a send & receive API for half- and
full-duplex mode. Sent and received data items are restricted to 1 - 32 bit
length and each data item is surrounded by (H)SPI CS inactive.

## spi.recv()
Receive data from SPI.

#### Syntax
`spi.recv(id, size[, default_data])`

#### Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `size` number of data items to be read
- `default_data` default data being sent on MOSI (all-1 if omitted)

#### Returns
String containing the bytes read from SPI.

####See also
[spi.send()](#spisend)

## spi.send()
Send data via SPI in half-duplex mode. Send & receive data in full-duplex mode.

#### Syntax
HALFDUPLEX:<br />
`wrote = spi.send(id, data1[, data2[, ..., datan]])`

FULLDUPLEX:<br />
`wrote[, rdata1[, ..., rdatan]] = spi.send(id, data1[, data2[, ..., datan]])`

#### Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `data` data can be either a string, a table or an integer number.<br/>Each data item is considered with `databits` number of bits.

#### Returns
- `wrote` number of written bytes
- `rdata` received data when configured with `spi.FULLDUPLEX`<br />Same data type as corresponding data parameter.

#### Example
```lua
=spi.send(1, 0, 255, 255, 255)
4       255     192     32      0
x = {spi.send(1, 0, 255, 255, 255)}
=x[1]
4
=x[2]
255
=x[3]
192
=x[4]
32
=x[5]
0
=x[6]
nil
=#x
5

_, _, x = spi.send(1, 0, {255, 255, 255})
=x[1]
192
=x[2]
32
=x[3]
0
```
#### See also
- [spi.setup()](#spisetup)
- [spi.recv()](#spirecv)

## spi.setup()
Set up the SPI configuration.
Refer to [Serial Peripheral Interface Bus](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus#Clock_polarity_and_phase) for details regarding the clock polarity and phase definition.

Calling `spi.setup()` will route the HSPI signals to the related pins, overriding previous configuration and control by the `gpio` module. It is possible to revert any pin back to gpio control if its HSPI functionality is not needed, just set the desired `gpio.mode()` for it. This is recommended especially for the HSPI /CS pin function in case that SPI slave-select is driven from a different pin by `gpio.write()` - the SPI engine would toggle pin 8 otherwise.

#### Syntax
`spi.setup(id, mode, cpol, cpha, databits, clock_div[, duplex_mode])`

#### Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `mode` select master or slave mode
	- `spi.MASTER`
	- `spi.SLAVE` - **not supported currently**
- `cpol` clock polarity selection
	- `spi.CPOL_LOW` 
	- `spi.CPOL_HIGH`
- `cpha` clock phase selection
	- `spi.CPHA_LOW`
	- `spi.CPHA_HIGH`
- `databits` number of bits per data item 1 - 32
- `clock_div` SPI clock divider, f(SPI) = 80 MHz / `clock_div`, 1 .. n (0 defaults to divider 8)
- `duplex_mode` duplex mode
	- `spi.HALFDUPLEX` (default when omitted)
	- `spi.FULLDUPLEX`

#### Returns
Number: 1

#### Example
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
-- we won't be using the HSPI /CS line, so disable it again
gpio.mode(8, gpio.INPUT, gpio.PULLUP)
```

## Low Level Hardware Functions
The low level functions provide a hardware-centric API for application
scenarios that need to excercise more complex SPI transactions. The
programming model is built up around the HW send and receive buffers and SPI
transactions are initiated with full control over the hardware features.

## spi.get_miso()
Extract data items from MISO buffer after `spi.transaction()`.

#### Syntax
```lua
data1[, data2[, ..., datan]] = spi.get_miso(id, offset, bitlen, num)
string = spi.get_miso(id, num)
```

#### Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `offset` bit offset into MISO buffer for first data item
- `bitlen` bit length of a single data item
- `num` number of data items to retrieve

####Returns
`num` data items or `string`

#### See also
[spi.transaction()](#spitransaction)

## spi.set_mosi()
Insert data items into MOSI buffer for `spi.transaction()`.

#### Syntax
```lua
spi.set_mosi(id, offset, bitlen, data1[, data2[, ..., datan]])
spi.set_mosi(id, string)
```

####Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `offset` bit offset into MOSI buffer for inserting data1 and subsequent items
- `bitlen` bit length of data1, data2, ...
- `data` data items where `bitlen` number of bits are considered for the transaction.
- `string` send data to be copied into MOSI buffer at offset 0, bit length 8

#### Returns
`nil`

#### See also
[spi.transaction()](#spitransaction)

## spi.transaction()
Start an SPI transaction, consisting of up to 5 phases:

1. Command
2. Address
3. MOSI
4. Dummy
5. MISO

#### Syntax
`spi.transaction(id, cmd_bitlen, cmd_data, addr_bitlen, addr_data, mosi_bitlen, dummy_bitlen, miso_bitlen)`

#### Parameters
- `id` SPI ID number: 0 for SPI, 1 for HSPI
- `cmd_bitlen` bit length of the command phase (0 - 16)
- `cmd_data` data for command phase
- `addr_bitlen` bit length for address phase (0 - 32)
- `addr_data` data for command phase
- `mosi_bitlen` bit length of the MOSI phase (0 - 512)
- `dummy_bitlen` bit length of the dummy phase (0 - 256)
- `miso_bitlen` bit length of the MISO phase (0 - 512) for half-duplex.<br />Full-duplex mode is activated with a negative value.

####Returns
`nil`

####See also
- [spi.set_mosi()](#spisetmosi)
- [spi.get_miso()](#spigetmiso)
