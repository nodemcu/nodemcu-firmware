As with [flashing](flash.md) there are several ways to upload code from your computer to the device.

Note that the NodeMCU serial interface uses 9600 bps at boot time. To increase the speed after booting, issue `uart.setup(0,115200,8,0,1,1)`. ESPlorer will do this automatically when changing the speed in the dropdown list.
If the device panics and resets at any time, errors will be written to the serial interface at 115200 bps.

# ESPlorer

> The essential multiplatforms tools for any ESP8266 developer from luatool author’s, including Lua for NodeMCU and MicroPython. Also, all AT commands are supported. Requires Java (Standard Edition - SE ver 7 and above) installed.

![ESPlorer](../img/ESPlorer.jpg "ESPlorer")

Source: [https://github.com/4refr0nt/ESPlorer](https://github.com/4refr0nt/ESPlorer)

Supported platforms: OS X, Linux, Windows, anything that runs Java

# nodemcu-uploader.py

> A simple tool for uploading files to the filesystem of an ESP8266 running NodeMCU as well as some other useful commands.

Source: [https://github.com/kmpm/nodemcu-uploader](https://github.com/kmpm/nodemcu-uploader)

Supported platforms: OS X, Linux, Windows, anything that runs Python

# NodeMCU Studio

> THIS TOOL IS IN REALLY REALLY REALLY REALLY EARLY STAGE!!!!!!!!!!!!!!!!!!!!!!!!!!!

Source: [https://github.com/nodemcu/nodemcu-studio-csharp](https://github.com/nodemcu/nodemcu-studio-csharp)

Supported platforms: Windows

# luatool

> Allow easy uploading of any Lua-based script into the ESP8266 flash memory with NodeMcu firmware

Source: [https://github.com/4refr0nt/luatool](https://github.com/4refr0nt/luatool)

Supported platforms: OS X, Linux, Windows, anything that runs Python