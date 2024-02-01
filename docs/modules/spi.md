# SPI Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-05-04 | [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [spi.c](../../components/modules/spi.c)|

# SPI Bus
The ESP32 contains 4 SPI bus hosts called `SPI`, `SPI1`, `HSPI`, and `VSPI`. `SPI` is locked to flash communication and is not available for the application. `SPI1` is currently also tied to flash support, but might be available in the future. Applications can currently only use the `HSPI` and `VSPI` hosts.

On later models in the ESP32 series, the SPI hosts are simply referred to by number, and are available here as `spi.SPI1`, `spi.SPI2` and (if present) `spi.SPI3`.

The host signals can be mapped to any suitable GPIO pins.

!!! note

    The API on ESP32 differs from the API on ESP8266. For backwards compatibility please refer to [`lua_compat/spi_compat.lua`](../../lua_compat/spi_compat.lua).


## spi.master()
Initializes a bus in master mode and returns a bus master object.

#### Syntax
`spi.master(host[, config[, dma]])`

#### Parameters
- `host` id, one of
    - `spi.SPI1`. not supported yet
    - `spi.HSPI`
    - `spi.VSPI`
- `config` table listing the assigned GPIOs. All signal assignment are optional.
    - `sclk`
    - `mosi`
    - `miso`
    - `quadwp`
    - `quadhd`
- `dma` set DMA channel (1 or 2) or disable DMA (0), defaults to 1 if omitted.
  Enabling DMA allows sending and receiving an unlimited amount of bytes but has restrictions in halfduplex mode (see [`spi.master:device()`](#spimasterdevice)).
  Disabling DMA limits a transaction to 32&nbsp;bytes max.

!!! not

    Omitting the `config` table returns an SPI bus master object without further initialization. This enables sharing the same SPI host with `sdmmc` in SD-SPI mode. First call `sdmmc.init(sdmmc.VSPI, ...)`, then `spi.master(spi.VSPI)`.

#### Returns
SPI bus master object

#### Example
```lua
busmaster_config = {sclk = 19, mosi = 23, miso = 25}
busmaster = spi.master(spi.HSPI, busmaster_config)
```

# Bus Master Object

## spi.master:close()
Close the bus host. This fails if there are still devices registered on this bus.

!!! caution

    The bus is also closed when the bus master object is automatically destroyed during garbage collection. Registered devices inherently prevent garbage collection of the bus master object.

#### Syntax
`busmaster:close()`

#### Parameters
none

#### Returns
`nil`

## spi.master:device()
Adds a device on the given master bus. Up to three devices per bus are supported.

!!! note

    Due to restrictions of the ESP IDF, halfduplex mode does not support DMA with both MOSI and MISO phases. Disable DMA during the call to [`spi.master()`](#spimaster) in this case and ensure that transaction data is not larger than 32&nbsp;bytes.

#### Syntax
`busmaster:device(config)`

#### Parameters
`config` table describing the device parameters:

- `cs` GPIO connected to device's chip-select pin, optional
- `mode` SPI mode used for this device (0-3), mandatory
- `freq` clock frequency used for this device [Hz], mandatory
- `command_bits` amount of bits in command phase (0-16), defaults to 0 if omitted
- `address_bits` amount of bits in address phase (0-64), defaults to 0 if omitted
- `dummy_bits` amount of dummy bits to insert address and data phase, defaults to 0 if omitted
- `cs_ena_pretrans`, optional
- `cs_ena_posttrans`, optional
- `duty_cycle_pos`, optional
- `tx_lsb_first` transmit command/address/data LSB first if `true`, MSB first otherwise (or if omitted)
- `rx_lsb_first` receive data LSB first if `true`, MSB first otherwise (or if omitted)
- `wire3` use spiq for both transmit and receive if `true`, use mosi and miso otherwise (or if omitted)
- `positive_cs` chip-select is active high during a transaction if `true`, cs is active low otherwise (or if omitted)
- `halfduplex` transmit data before receiving data if `true`, transmit and receive simultaneously otherwise (or if omitted)
- `clk_as_cs` output clock on cs line when cs is active if `true`, defaults to `false` if omitted

#### Returns    
SPI device object

#### Example
```lua
device_config = {mode = 0, freq = 1000000}
device_config.cs = 22
dev1 = busmaster:device(device_config)

device_config.cs = 4
dev2 = busmaster:device(device_config)
```

# Device Object

## spi.master:device:remove()
Removes a device from the related bus master.

!!! caution

    The device is also removed when the device object is automatically destroyed during garbage collection.

#### Syntax
`device:remove()`

#### Parameters
none

#### Returns
`nil`

## spi.master:device:transfer()
Initiate an SPI transaction consisting of

1. assertion of cs signal
2. optional sending of command bits
3. optional sending of address bits
4. optional sending of dummy bits
5. sending of tx data
6. concurrent or appended reception of rx data, optional
7. de-assertion of cs signal

The function returns after the transaction is completed.

#### Syntax
```lua
device:transfer(trans)
device:transfer(txdata)
```

#### Parameters
`trans` table containing the elements of the transaction:

- `cmd` data for command phase, amount of bits was defined during device creation, optional
- `addr` data for address phase, amount of bits was defined during device creation, optional
- `txdata` string of data to be sent to the device, optional
- `rxlen` number of bytes to be received, optional
- `mode` optional, one of
    - `sio` transmit in SIO mode, default if omitted
    - `dio` transmit in DIO mode
    - `qio` transmit in QIO mode
- `addr_mode` transmit address also in selected `mode` if `true`, transmit address in SIO otherwise.

`txdata` string of data to be sent to the device

#### Returns
String of `rxlen` length, or `#txdata` length if `rxlen` is omitted.


## spi.slave()
Initializes a bus in slave mode and returns a slave object.
Not yet supported.
