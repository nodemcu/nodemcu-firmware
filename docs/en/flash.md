Adafruit provides a really nice [firmware flashing tutorial](https://learn.adafruit.com/building-and-running-micropython-on-the-esp8266/flash-firmware). Below you'll find just the basics for the two popular tools esptool and NodeMCU Flasher.

!!! attention

    Keep in mind that the ESP8266 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

## esptool.py
> A cute Python utility to communicate with the ROM bootloader in Espressif ESP8266. It is intended to be a simple, platform independent, open source replacement for XTCOM.

Source: [https://github.com/themadinventor/esptool](https://github.com/themadinventor/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

**Running esptool.py**

Run the following command to flash an *aggregated* binary as is produced for example by the [cloud build service](build.md#cloud-build-service) or the [Docker image](build.md#docker-image).

`esptool.py --port <serial-port-of-ESP8266> write_flash -fm <mode> -fs <size> 0x00000 <nodemcu-firmware>.bin`

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

## Upgrading Firmware

!!! important

    It goes without saying that you shouldn't expect your NodeMCU 0.9.x Lua scripts to work error-free on a more recent firmware. Most notably Espressif changed the `socket:send` operation to be asynchronous i.e. non-blocking. See [API documentation](modules/net.md#netsocketsend) for details.

Espressif changes the init data block (`esp_init_data_default.bin`) for their devices along the way with the SDK. So things break when a NodeMCU firmware with a certain SDK is flashed to a module which contains init data from a different SDK. Hence, this section applies to upgrading NodeMCU firmware just as well as *downgrading* firmware.

A typical case that often fails is when a module is upgraded from a 0.9.x firmware to a recent version. It might look like the new firmware is broken, but the reason for the missing Lua prompt is related to the big jump in SDK versions.

If there is no init data block found during SDK startup, the SDK will install one itself. If there is a previous (potentially too old) init block, the SDK *probably* doesn't do anything with it but there is no documentation from Espressif on this topic.

Hence, there are two strategies to update the SDK init data:

- Erase flash completely. This will also erase the (Lua) files you uploaded to the device! The SDK will install the init data block during startup.
- Don't erase the flash but replace just the init data with a new file during the flashing procedure. For this you would download [SDK patch 1.5.4.1](http://bbs.espressif.com/download/file.php?id=1572) and extract `esp_init_data_default.bin` from there.

When flashing a new firmware (particularly with a much different size), the flash filesystem may be reformatted as the firmware starts. If it is not automatically reformatted, then it should be valid and have the same contents as before the flash operation. You can still run [`file.format()`](modules/file.md#fileformat) to re-format your flash filesystem. You will know if you need to do this if your flash files exist but seem empty, or if data cannot be written to new files. However, this should be an exceptional case.

**esptool.py**

For [esptool.py](https://github.com/themadinventor/esptool) you specify the init data file as an additional file for the `write_flash` command.

```
esptool.py --port <serial-port-of-ESP8266> erase_flash
esptool.py --port <serial-port-of-ESP8266> write_flash <flash options> 0x00000 <nodemcu-firmware>.bin <init-data-address> esp_init_data_default.bin
```

!!! note "Note:"

    The address for `esp_init_data_default.bin` depends on the size of your module's flash. 
    
    - `0x7c000` for 512 kB, modules like ESP-01, -03, -07 etc.
    - `0xfc000` for 1 MB, modules like ESP8285, PSF-A85
    - `0x1fc000` for 2 MB
    - `0x3fc000` for 4 MB, modules like ESP-12E, NodeMCU devkit 1.0, WeMos D1 mini

**NodeMCU Flasher**

The [NodeMCU Flasher](https://github.com/nodemcu/nodemcu-flasher) will download init data using a special path:
```
INTERNAL://DEFAULT
```

Replace the provided (old) `esp_init_data_default.bin` with the one extracted above and use the flasher like you're used to.

**References**

* [2A-ESP8266__IOT_SDK_User_Manual__EN_v1.5.pdf, Chapter 6](http://bbs.espressif.com/viewtopic.php?f=51&t=1024)
* [SPI Flash ROM Layout (without OTA upgrades)](https://github.com/esp8266/esp8266-wiki/wiki/Memory-Map#spi-flash-rom-layout-without-ota-upgrades)
