Below you'll find all necessary information to flash a NodeMCU firmware binary to ESP32. Note that this is a reference documentation and not a tutorial with fancy screen shots. Turn to your favorite search engine for those.

!!! attention

    Keep in mind that the ESP32 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

## Tool overview

### esptool.py
> A Python-based, open source, platform independent, utility to communicate with the ROM bootloader in Espressif ESP8266.

Source: [https://github.com/espressif/esptool](https://github.com/espressif/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

Execute `make flash` to build and flash the firmware. See [Flashing Options](build.md#flashing-options) for the configuration of esptool.py.
