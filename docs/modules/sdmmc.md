# SDMMC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-17 | [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [sdmmc.c](../../components/modules/sdmmc.c)|

!!! note

    MMC cards are not yet supported on HS1/HS2 interfaces due to missing functionality in the IDF driver. Use the SD SPI mode on HSPI/VSPI instead.

## SD Card connection

!!! caution

    The adapter does not require level shifters since SD and ESP are supposed to be powered with the same voltage. If your specific model contains level shifters then make sure that both sides can be operated at 3V3.

![1:1 micro-sd adapter](../img/micro_sd-small.jpg "1:1 micro-sd adapter")

### SDMMC Mode

If the SD card is operated in SDMMC mode, then the card has to be wired to the ESP pins of the HS1_* or HS2_* interfaces. There are several naming schemes used on different adapters - the following list shows alternative terms:

| SD mode name  | SPI mode name   | ESP32 HS1 I/F                  | ESP32 HS2 I/F             |
| :---          | :---            | :---                           | :---                      |
| `CLK`         | `CK, SCLK`      | `GPIO6, HS1_CLK, SD_CLK`       | `GPIO14, HS2_CLK, MTMS`   |
| `CMD`         | `DI, MOSI`      | `GPIO11, HS1_CMD, SD_CMD`      | `GPIO15, HS2_CMD, MTDO`   |
| `DAT0`        | `DO, MISO`      | `GPIO7, HS1_DATA0, SD_DATA_0`  | `GPIO2, HS2_DATA0`        |
| `DAT1`        | n/a             | `GPIO8, HS1_DATA1, SD_DATA_1`  | `GPIO4, HS2_DATA1`        |
| `DAT2`        | n/a             | `GPIO9, HS1_DATA2, SD_DATA_2`  | `GPIO12, HS2_DATA2, MTDI` |
| `DAT3`        | `CS, SS`        | `GPIO10, HS1_DATA3, SD_DATA_3` | `GPIO13, HS2_DATA3, MTCK` |
| `DAT4` (MMC)  | n/a             | `GPIO16, HS1_DATA4`            | n/a                       |
| `DAT5` (MMC)  | n/a             | `GPIO17, HS1_DATA5`            | n/a                       |
| `DAT6` (MMC)  | n/a             | `GPIO5, HS1_DATA6`             | n/a                       |
| `DAT7` (MMC)  | n/a             | `GPIO18, HS1_DATA7`            | n/a                       |
| `VDD`         | `VCC, VDD`      | 3V3 supply                     | 3V3 supply                |
| `VSS1, VSS2`  | `VSS, GND`      | common ground                  | common ground             |

Connections to `CLK`, `CMD`, and `DAT0` are mandatory and enable basic operation in 1-bit mode. For 4-bit mode `DAT1`, `DAT2`, and `DAT3` are required additionally.

!!! important

    Connecting DAT0 to GPIO2 can block firmware flashing depending on the electrical configuration at this pin. Disconnect GPIO2 from the card adapter during flashing if unsure.

### SD SPI Mode

In SD SPI mode the SD or MMC card is operated by the SPI host interfaces (HSPI or VSPI). Both hosts can be assigned to any GPIO pins.

| SPI mode name   | ESP32 HSPI, VSPI I/F |
| :---            | :---                 |
| `CK, SCLK`      | Any GPIO             |
| `DI, MOSI`      | Any GPIO             |
| `DO, MISO`      | Any GPIO             |
| n/a             | n/a                  |
| n/a             | n/a                  |
| `CS, SS`        | Any GPIO             |
| n/a             | n/a                  |
| n/a             | n/a                  |
| n/a             | n/a                  |
| n/a             | n/a                  |
| `VCC, VDD`      | 3V3 supply           |
| `VSS, GND`      | common ground        |

## sdmmc.init()
Initialize the SDMMC and probe the attached SD card.

#### Syntax
`sdmmc.init(slot[, cfg])`

#### Parameters SDMMC Mode
- `slot` SDMMC slot, one of `sdmmc.HS1` or `sdmmc.HS2`
- `cfg` optional table containing slot configuration:
    - `cd_pin` card detect pin, none if omitted
    - `wp_pin` write-protect pin, none if omitted
    - `fmax` maximum communication frequency, defaults to 20&nbsp; if omitted
    - `width` bis width, defaults to `sdmmc.W1BIT` if omitted, one of:
        - `sdmmc.W1BIT`
        - `sdmmc.W4BIT`
        - `sdmmc.W8BIT`, not supported yet

#### Parameters SD SPI Mode
- `slot` SD SPI slot, one of `sdmmc.SPI2` or `sdmmc.SPI3` (on ESP32 the names`sdmmc.HSPI` or `sdmmc.VSPI` are still available, but deprecated)
- `cfg` mandatory table containing slot configuration:
    - `sck_pin` SPI SCK pin, mandatory
    - `mosi_pin`, SPI MOSI pin, mandatory
    - `miso_pin`, SPI MISO pin, mandatory
    - `cs_pin`, SPI CS pin, mandatory
    - `cd_pin` card detect pin, none if omitted
    - `wp_pin` write-protect pin, none if omitted
    - `fmax` maximum communication frequency, defaults to 20&nbsp; if omitted

#### Returns
Card object.

Error is thrown for invalid parameters or if SDMMC hardware or card cannot be initialized.


## card:get_info()
Retrieve information from the SD card.

#### Syntax
`card:get_info()`

#### Returns
Table containing the card's OCR, CID, CSD, SCR, and RCA with elements:

- `ocr` Operation Conditions Register
- `cid` Card IDentification
    - `date` manufacturing date
    - `mfg_id` manufacturer ID
    - `name` product name
    - `oem_id` OEM/product ID
    - `revision` product revision
    - `serial` product serial number
- `csd` Card-Specific Data
    - `capacity` total number of sectors
    - `card_command_class` card command class for SD
    - `csd_ver` CSD structure format
    - `mmc_ver` MMC version (for CID format)
    - `read_block_len` block length for reads
    - `sector_size` sector size in bytes
    - `tr_speed` maximum transfer speed
- `scr`
    - `sd_spec` SD physical layer specification version, reported by card
    - `bus_width` bus widths supported by card
- `rca` Relative Card Address


## card:mount()
Mount filesystem on SD card.

#### Syntax
`card:mount(ldrv)`

#### Parameters
- `ldrv` name of logical drive, "/SD0", "/SD1", etc.

#### Returns
`true` if successful, `false` otherwise

Error is thrown for invalid parameters or if filesystem is already mounted.


## card:read()
Read one or more sectors.

#### Syntax
`card:read(start_sec, num_sec)`

#### Parameters
- `start_sec` first sector to read from
- `num_sec` number of sectors to read (>= 1)

#### Returns
String containing the sector data.

Error is thrown for invalid parameters or if sector(s) cannot be read.


## card:umount()
Unmount filesystem.

#### Syntax
`card:umount()`

#### Parameters
None

#### Returns
`nil`

Error is thrown if filesystem is not mounted or if it cannot be unmounted.


## card:write()
Write one or more sectors.

#### Syntax
`card:write(start_sec, data)`

#### Parameters
- `start_sec` first sector to write to
- `data` string of data to write, must be multiple of sector size (512&nbsp;bytes)

#### Returns
`nil`

Error is thrown for invalid parameters or if sector(s) cannot be written.
