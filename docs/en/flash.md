Adafruit provides a really nice [firmware flashing tutorial](https://learn.adafruit.com/building-and-running-micropython-on-the-esp8266/flash-firmware). Below you'll find just the basics for the two popular tools esptool and NodeMCU Flasher.

!!! note "Note:"

    Keep in mind that the ESP8266 needs to be put into flash mode before you can flash a new firmware!

To enable ESP8266 firmware flashing GPIO0 pin must be pulled low before the device is reset. Conversely, for a normal boot, GPIO0 must be pulled high or floating.

If you have a [NodeMCU dev kit](https://github.com/nodemcu/nodemcu-devkit-v1.0) then you don't need to do anything, as the USB connection can pull GPIO0 low by asserting DTR and reset your board by asserting RTS.

If you have an ESP-01 or other device without built-in USB, you will need to enable flashing yourself by pulling GPIO0 low or pressing a "flash" switch.

## esptool
> A cute Python utility to communicate with the ROM bootloader in Espressif ESP8266. It is intended to be a simple, platform independent, open source replacement for XTCOM.

Source: [https://github.com/themadinventor/esptool](https://github.com/themadinventor/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

**Running esptool.py**

Run the following command to flash an *aggregated* binary as is produced for example by the [cloud build service](build.md#cloud-build-service) or the [Docker image](build.md#docker-image).

`esptool.py --port <USB-port-with-ESP8266> write_flash 0x00000 <nodemcu-firmware>.bin`

## NodeMCU Flasher
> A firmware Flash tool for NodeMCU...We are working on next version and will use QT framework. It will be cross platform and open-source.

Source: [https://github.com/nodemcu/nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher)

Supported platforms: Windows