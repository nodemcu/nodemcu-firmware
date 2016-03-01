As with [flashing](flash.md) there are several ways to upload code from your computer to the device.

Note that the NodeMCU serial interface uses 115200bps at boot time. To change the speed after booting, issue `uart.setup(0,9600,8,0,1,1)`. ESPlorer will do this automatically when changing the speed in the dropdown list. If the device panics and resets at any time, errors will be written to the serial interface at 115200 bps.

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

# Compiling Lua on your PC for Uploading

If you install lua on your development PC or Laptop then you can use the standard Lua
compiler to syntax check any Lua source before downloading it to the ESP8266 module.  However,
the nodemcu compiler output uses different data types (e.g. it supports ROMtables) so the
compiled output cannot run on the ESP8266.  

Compiling source on one platform for use on another (e.g. Intel x38 Window to ESP8266) is 
known as _cross-compilation_ and the nodemcu firmware supports the compilation of `luac.cross` 
on \*nix patforms which have Lua 5.1, the Lua filesystem module (lfs), and the essential
GCC tools.  Simply change directory to the firmware root directoy and run the command:

    lua tools/cross-lua.lua
    
This will generate a `luac.cross` executable in your root directory which can be used to
compile and to syntax-check Lua source on the Development machine for execution under 
nodemcu lua on the ESP8266. 
 
