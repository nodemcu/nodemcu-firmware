Below you'll find all necessary information to flash a NodeMCU firmware binary to ESP32. Note that this is a reference documentation and not a tutorial with fancy screen shots. Turn to your favorite search engine for those.

!!! attention

    Keep in mind that the ESP32 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

## Tool overview

### esptool.py
> A Python-based, open source, platform independent, utility to communicate with the ROM bootloader in Espressif ESP8266.

Source: [https://github.com/espressif/esptool](https://github.com/espressif/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

Execute `make flash` to build and flash the firmware. See [Flashing Options](build.md#flashing-options) for the configuration of esptool.py.

### NodeMCU PyFlasher
> Self-contained [NodeMCU](https://github.com/nodemcu/nodemcu-firmware) flasher with GUI based on [esptool.py](https://github.com/espressif/esptool) and [wxPython](https://www.wxpython.org/).

![NodeMCU PyFlasher](https://github.com/marcelstoer/nodemcu-pyflasher/raw/master/images/gui.png "NodeMCU PyFlasher")

Source: [https://github.com/marcelstoer/nodemcu-pyflasher](https://github.com/marcelstoer/nodemcu-pyflasher)

Supported platforms: anything that runs Python, runnable .exe available for Windows and .dmg for macOS

Supports flashing *aggregated binaries* as for example produced by [the Docker build](build.md#docker-image).

Disclaimer: the availability of [NodeMCU PyFlasher was announced on the NodeMCU Facebook page](https://www.facebook.com/NodeMCU/posts/663197460515251) but it is not an official offering of the current NodeMCU firmware team.
