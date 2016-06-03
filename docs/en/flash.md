Adafruit provides a really nice [firmware flashing tutorial](https://learn.adafruit.com/building-and-running-micropython-on-the-esp8266/flash-firmware). Below you'll find just the basics for the two popular tools esptool and NodeMCU Flasher.

!!! note "Note:"

    Keep in mind that the ESP8266 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

## esptool
> A cute Python utility to communicate with the ROM bootloader in Espressif ESP8266. It is intended to be a simple, platform independent, open source replacement for XTCOM.

Source: [https://github.com/themadinventor/esptool](https://github.com/themadinventor/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

**Running esptool.py**

Run the following command to flash an *aggregated* binary as is produced for example by the [cloud build service](build.md#cloud-build-service) or the [Docker image](build.md#docker-image).

`esptool.py --port <USB-port-with-ESP8266> write_flash -fm <mode> -fs <size> 0x00000 <nodemcu-firmware>.bin`

- `mode` is `qio` for 512&nbsp;kByte modules and `dio` for 4&nbsp;MByte modules (`qio` might work as well, YMMV).
- `size` is given in bits. Specify `4m` for 512&nbsp;kByte and `32m` for 4&nbsp;MByte.

Check the [esptool flash modes documentation](https://github.com/themadinventor/esptool#flash-modes) for details and other options.

## NodeMCU Flasher
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

Also, in some special circumstances, you may need to flash `blank.bin` or `esp_init_data_default.bin` to various addresses on the flash (depending on flash size and type), see [below](#upgrading-from-sdk-09x-firmware).

If upgrading from [SPIFFS](https://github.com/pellepl/spiffs) version 0.3.2 to 0.3.3 or later, or after flashing any new firmware (particularly one with a much different size), you may need to run [`file.format()`](modules/file.md#fileformat) to re-format your flash filesystem. You will know if you need to do this if your flash files disappeared, or if they exist but seem empty, or if data cannot be written to new files.

## Upgrading from SDK 0.9.x Firmware
If you flash a recent NodeMCU firmware for the first time, it's advisable that you get all accompanying files right. A typical case that often fails is when a module is upgraded from a 0.9.x firmware to the latest version built from the [NodeMCU build service](http://nodemcu-build.com). It might look like the brand new firmware is broken, but the reason for the missing Lua prompt is related to the big jump in SDK versions: Espressif changed the `esp_init_data_default.bin` for their devices along the way with the [SDK 1.4.0 release](http://bbs.espressif.com/viewtopic.php?f=46&t=1124). So things break when a NodeMCU firmware with SDK 1.4.0 or above is flashed to a module which contains old init data from a previous SDK.

Download a recent SDK release, e.g. [esp_iot_sdk_v1.4.0_15_09_18.zip](http://bbs.espressif.com/download/file.php?id=838) or later and extract `esp_init_data_default.bin` from there. *Use this file together with the new firmware during flashing*.

**esptool**

For [esptool](https://github.com/themadinventor/esptool) you specify another file to download at the command line.
```
esptool.py write_flash <flash options> 0x00000 <nodemcu-firmware>.bin 0x7c000 esp_init_data_default.bin
```

!!! note "Note:"

    The address for `esp_init_data_default.bin` depends on the size of your module's flash. ESP-01, -03, -07 etc. with 512 kByte flash require `0x7c000`. Init data goes to `0x3fc000` on an ESP-12E with 4 MByte flash.

**NodeMCU Flasher**

The [NodeMCU Flasher](https://github.com/nodemcu/nodemcu-flasher) will download init data using a special path:
```
INTERNAL://DEFAULT
```

Replace the provided (old) `esp_init_data_default.bin` with the one extracted above and use the flasher like you're used to.

**References**

* [2A-ESP8266__IOT_SDK_User_Manual__EN_v1.5.pdf, Chapter 6](http://bbs.espressif.com/viewtopic.php?f=51&t=1024)
* [SPI Flash ROM Layout (without OTA upgrades)](https://github.com/esp8266/esp8266-wiki/wiki/Memory-Map#spi-flash-rom-layout-without-ota-upgrades)
