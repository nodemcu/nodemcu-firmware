# NodeMCU Documentation

NodeMCU is an open source [Lua](https://www.lua.org/) based firmware for the [ESP32](https://www.espressif.com/en/products/socs/esp32) and [ESP8266](https://www.espressif.com/en/products/socs/esp8266) WiFi SOCs from Espressif. It uses an on-module flash-based [SPIFFS](https://github.com/pellepl/spiffs) file system. NodeMCU is implemented in C and the ESP32 version is layered on the [Espressif ESP-IDF](https://github.com/espressif/ESP-IDF).

The firmware was initially developed as is a companion project to the popular ESP8266-based [NodeMCU development modules](https://github.com/nodemcu/nodemcu-devkit-v1.0), but the project is now community-supported, and the firmware can now be run on _any_ ESP module.

!!! important

    The NodeMCU [`release`](https://github.com/nodemcu/nodemcu-firmware/tree/release) and [`dev`](https://github.com/nodemcu/nodemcu-firmware/tree/dev) branches target the ESP8266. The [`dev-esp32`](https://github.com/nodemcu/nodemcu-firmware/tree/dev-esp32) branch targets the ESP32.

## Up-To-Date Documentation
At the moment the only up-to-date documentation maintained by the current NodeMCU team is in English. It is part of the source code repository (`/docs` subfolder) and kept in sync with the code.
