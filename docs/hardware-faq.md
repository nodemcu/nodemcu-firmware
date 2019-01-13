# Hardware FAQ
 
## What is this FAQ for?

This FAQ addresses hardware-specific issues relating to the NodeMcu firmware on
NoceMCU Inc Devkits and other ESP-8266 modules.

## Hardware Specifics

## Why file writes fail all the time on DEVKIT V1.0?

NodeMCU DEVKIT V1.0 uses ESP12-E-DIO(ESP-12-D) module. This module runs the
Flash memory in [Dual IO SPI](#whats-the-different-between-dio-and-qio-mode)
(DIO) mode. This firmware will not be correctly loaded if you use old flashtool
versions, and the filesystem will not work if you used a pre 0.9.6 firmware
version (<0.9.5) or old. The easiest way to resolve this problem s update all
the firmware and flash tool to current version.

-   Use the latest [esptool.py](https://github.com/themadinventor/esptool) with
DIO support and command option to flash firmware, or 

-   Use the latest [NodeMCU flasher](https://github.com/NodeMCU/NodeMCU-flasher) 
with default option. (You must select the `restore to default` option in advanced
menu tab), or 

-   Use the latest Espressif's flash tool -- see [this Espressif forum
topic](http://bbs.espressif.com/viewtopic.php?f=5&t=433) (without auto download
support). Use DIO mode and 32M flash size option, and flash latest firmware to
0x00000. Before flashing firmware, remember to hold FLASH button, and press RST
button once. Note that the new NodeMCU our firmware download tool, when
released, will be capable of flashing firmware automatically without any button
presses.

## What's the different between DIO and QIO mode?
Whether DIO or QIO modes are available depends on the physical connection
between the ESP8266 CPU and its onboard flash chip. QIO connects to the flash
using 5 data pins as compared to DIO's 3. This frees up an extra 2 IO pins for
GPIO use, but this also halves the read/write data-rate to Flash compared to 
QIO modules. 

## How to use DEVKIT V0.9 on Mac OS X?
<TODO>

### How does DEVKIT use DTR and RTS enter download mode?
<TODO>
