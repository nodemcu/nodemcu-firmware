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

# ChiliPeppr ESP32 Web IDE

> This is a new method via the browser of editing/uploading your Lua code to your ESP32 devices. The workspace has a Serial Port JSON Server that you run locally or remotely on your computer to let your browser talk direct to your serial ports. The Ace editor is embedded which does code highlighting and intellisense for Lua. There is a file browser for your ESP32, a serial port console, restart button, file saving, run, upload/run, and upload all. A pre-compiled binary is available with all modules compiled in so you don't have to compile the firmware on your own. There is also sample code including pure Lua libraries for websockets, joystick control, etc. There is also an XBM image creator for OLED displays that let you cut/paste PNG, JPEG, or GIF images or even drag/drop of animated GIFs to create Lua code.

![ChiliPeppr ESP32 Workspace](https://github.com/chilipeppr/workspace-esp32-lua/raw/master/screenshot.png "ChiliPeppr ESP32 Workspace")

Location: [http://chilipeppr.com/esp32](http://chilipeppr.com)

Source: [https://github.com/chilipeppr/workspace-esp32-lua](https://github.com/chilipeppr/workspace-esp32-lua)

Supported platforms: OS X, Linux, Windows

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
 
