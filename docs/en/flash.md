Adafruit provides a really nice [firmware flashing tutorial](https://learn.adafruit.com/building-and-running-micropython-on-the-esp8266/flash-firmware).  Below you'll find just the basics for the two popular tools esptool and NodeMCU Flasher.

!!! important

    If you have trouble getting NodeMCU to run, ensure that you read the sections on [Upgrading Firmware](#upgrading-firmware) and [SDK Init Data](#sdk-init-data) below.

!!! attention

    Keep in mind that the ESP8266 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

## Tool overview

**esptool.py**
> A cute Python utility to communicate with the ROM bootloader in Espressif ESP8266. It is intended to be a simple, platform independent, open source replacement for XTCOM.

Source: [https://github.com/themadinventor/esptool](https://github.com/themadinventor/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

**Running esptool.py**

Run the following command to flash an *aggregated* binary as is produced for example by the [cloud build service](build.md#cloud-build-service) or the [Docker image](build.md#docker-image).

`esptool.py --port <serial-port-of-ESP8266> write_flash -fm <mode> -fs <size> 0x00000 <nodemcu-firmware>.bin <sdk_init_data...>`

- `mode` is `qio` for 512&nbsp;kByte modules and `dio` for 4&nbsp;MByte modules (`qio` might work as well, YMMV).
- `size` is given in bits. Specify `4m` for 512&nbsp;kByte and `32m` for 4&nbsp;MByte.
- `sdk_init_data` may be necessary.  See [SDK Init Data](#sdk-init-data) for details.

Check the [esptool flash modes documentation](https://github.com/themadinventor/esptool#flash-modes) for details and other options.

**NodeMCU Flasher**
> A firmware Flash tool for NodeMCU...We are working on next version and will use QT framework. It will be cross platform and open-source.

Source: [https://github.com/nodemcu/nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher)

Supported platforms: Windows

## Putting Device Into Flash Mode

To enable ESP8266 firmware flashing GPIO0 pin must be pulled low before the device is reset. Conversely, for a normal boot, GPIO0 must be pulled high or floating.

If you have a [NodeMCU dev kit](https://github.com/nodemcu/nodemcu-devkit-v1.0) then you don't need to do anything, as the USB connection can pull GPIO0 low by asserting DTR and reset your board by asserting RTS.

If you have an ESP-01 or other device without built-in USB, you will need to enable flashing yourself by pulling GPIO0 low or pressing a "flash" switch.

## Which Files To Flash

If you build your firmware with the [cloud builder or the Docker image](build.md), or any other method that produces a *combined binary*, then you can flash that file directly to address 0x00000.

Otherwise, if you built your own firmware from source code:

- `bin/0x00000.bin` to 0x00000
- `bin/0x10000.bin` to 0x10000

In both cases, you may also need to flash SDK init data; see [below](#sdk-init-data) for details.

## Upgrading Firmware

There are three potential issues that arise from upgrading (or downgrading!) firmware from one NodeMCU version to another:

* Lua scripts written for one NodeMCU version (like 0.9.x) may not work error-free on a more recent firmware.  Most notably, Espressif changed the `socket:send` operation to be asynchronous i.e. non-blocking. See [API documentation](modules/net.md#netsocketsend) for details.

* The Espressif SDK Init Data changes between each NodeMCU firmware version and may need to be reflashed.  See [SDK Init Data](#sdk-init-data) for details.

* The NodeMCU flash filesystem may need to be reformatted, particularly if its address has changed because the new firmware has a much different size than the old firmware.  If it is not automatically formatted, then it should be valid and have the same contents as before the flash operation. You can still run [`file.format()`](modules/file.md#fileformat) to re-format your flash filesystem. You will know if you need to do this if your flash files exist but seem empty, or if data cannot be written to new files. However, this should be an exceptional case.

## SDK Init Data

NodeMCU versions are compiled against specific versions of the Espressif SDK.  The SDK reserves space in flash that is used to store calibration and other data.  This data changes between SDK versions, and if it is invalid or not present, the code may not boot at all.  Symptoms include messages like `rf_cal[0] !=0x05,is 0xFF`, or endless reboot loops.

Some earlier SDK versions would automatically populate this space if booted on an erased chip.  This is no longer the case for 1.5.4.1, where it must be flashed manually.  The data that needs to be written is provided as part of the Espressif SDK, in the file `esp_init_data_default.bin`.  For 1.5.4.1, the file can be obtained by downloading [SDK patch 1.5.4.1](http://bbs.espressif.com/download/file.php?id=1572) and extracting `esp_init_data_default.bin` from there.

This file must be written to the 4th-last sector in flash.  Sectors are 4&nbsp;kB and the location will depend on the size of your flash memory.  For example, the correct address is:

- `0x7c000` for 512 kB, modules like ESP-01, -03, -07 etc.
- `0xfc000` for 1 MB, modules like ESP8285, PSF-A85
- `0x1fc000` for 2 MB
- `0x3fc000` for 4 MB, modules like ESP-12E, NodeMCU devkit 1.0, WeMos D1 mini

If you're not 100% sure what flash size you have, try other addresses.  Sometimes module manufacturers install a larger flash chip than they advertise.

**Flashing init data with esptool.py**

For [esptool.py](https://github.com/themadinventor/esptool) you specify the init data file as an additional file for the `write_flash` command.

```
esptool.py --port <serial-port-of-ESP8266> erase_flash
esptool.py --port <serial-port-of-ESP8266> write_flash <flash options> 0x00000 <nodemcu-firmware>.bin <init-data-address> esp_init_data_default.bin
```

**Flashing init data with the NodeMCU Flasher**

The [NodeMCU Flasher](https://github.com/nodemcu/nodemcu-flasher) will download init data using a special path:
```
INTERNAL://DEFAULT
```

Replace the provided (old) `esp_init_data_default.bin` with the one extracted above and use the flasher like you're used to.


**Technical details**

Espressif refers to this area as the "System Param" and reserves the last 4 sectors of flash, as described in Chapter 4 of the [ESP8266 Getting Started Guide](https://espressif.com/en/support/explore/get-started/esp8266/getting-started-guide).  Since SDK 1.5.4.1, a 5th sector is reserved for RF calibration (and its placement is controlled by NodeMCU), as described by the [patch notice](http://bbs.espressif.com/viewtopic.php?f=46&t=2407).  At a minimum, the 4th sector from the end needs to be flashed with `esp_init_data_default.bin`, and the 2nd sector from the end should be blank.

