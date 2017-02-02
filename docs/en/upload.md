As with [flashing](flash.md) there are several ways to upload code from your computer to the device.

Note that the NodeMCU serial interface uses 115'200bps at boot time. To change the speed after booting, issue `uart.setup(0,9600,8,0,1,1)`. ESPlorer will do this automatically when changing the speed in the dropdown list. If the device panics and resets at any time, errors will be written to the serial interface at 115'200 bps.

# Tools

## ESPlorer

> The essential multiplatforms tools for any ESP8266 developer from luatool author’s, including Lua for NodeMCU and MicroPython. Also, all AT commands are supported. Requires Java (Standard Edition - SE ver 7 and above) installed.

![ESPlorer](../img/ESPlorer.jpg "ESPlorer")

Source: [https://github.com/4refr0nt/ESPlorer](https://github.com/4refr0nt/ESPlorer)

Supported platforms: OS X, Linux, Windows, anything that runs Java

## nodemcu-uploader.py

> A simple tool for uploading files to the filesystem of an ESP8266 running NodeMCU as well as some other useful commands.

Source: [https://github.com/kmpm/nodemcu-uploader](https://github.com/kmpm/nodemcu-uploader)

Supported platforms: OS X, Linux, Windows, anything that runs Python

## NodeMCU Studio

> THIS TOOL IS IN REALLY REALLY REALLY REALLY EARLY STAGE!!!!!!!!!!!!!!!!!!!!!!!!!!!

Source: [https://github.com/nodemcu/nodemcu-studio-csharp](https://github.com/nodemcu/nodemcu-studio-csharp)

Supported platforms: Windows

## luatool

> Allow easy uploading of any Lua-based script into the ESP8266 flash memory with NodeMcu firmware

Source: [https://github.com/4refr0nt/luatool](https://github.com/4refr0nt/luatool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

# init.lua
You will see "lua: cannot open init.lua" printed to the serial console when the device boots after it's been freshly flashed. If NodeMCU finds a `init.lua` in the root of the file system it will execute it as part of the boot sequence (standard Lua feature). Hence, your application is initialized and triggered from `init.lua`. Usually you first set up the WiFi connection and only continue once that has been successful.

Be very careful not to lock yourself out! If there's a bug in your `init.lua` you may be stuck in an infinite reboot loop. It is, therefore, advisable to build a small delay into your startup sequence that would allow you to interrupt the sequence by e.g. deleting or renaming `init.lua` (see also [FAQ](lua-developer-faq.md#how-do-i-avoid-a-panic-loop-in-initlua)). Your `init.lua` is most likely going to be different than the one below but it's a good starting point for customizations:

```lua
-- load credentials, 'SSID' and 'PASSWORD' declared and initialize in there
dofile("credentials.lua")

function startup()
    if file.open("init.lua") == nil then
        print("init.lua deleted or renamed")
    else
        print("Running")
        file.close("init.lua")
        -- the actual application is stored in 'application.lua'
        -- dofile("application.lua")
    end
end

print("Connecting to WiFi access point...")
wifi.setmode(wifi.STATION)
wifi.sta.config(SSID, PASSWORD)
-- wifi.sta.connect() not necessary because config() uses auto-connect=true by default
tmr.create():alarm(1000, tmr.ALARM_AUTO, function(cb_timer)
    if wifi.sta.getip() == nil then
        print("Waiting for IP address...")
    else
        cb_timer:unregister()
        print("WiFi connection established, IP address: " .. wifi.sta.getip())
        print("You have 3 seconds to abort")
        print("Waiting...")
        tmr.create():alarm(3000, tmr.ALARM_SINGLE, startup)
    end
end)
```

Inspired by [https://github.com/ckuehnel/NodeMCU-applications](https://github.com/ckuehnel/NodeMCU-applications)

# Compiling Lua on your PC for Uploading

If you install `lua` on your development PC or Laptop then you can use the standard Lua
compiler to syntax check any Lua source before downloading it to the ESP8266 module.  However,
the NodeMCU compiler output uses different data types (e.g. it supports ROMtables) so the
compiled output cannot run on the ESP8266.  

Compiling source on one platform for use on another (e.g. Intel x38 Window to ESP8266) is 
known as _cross-compilation_ and the NodeMCU firmware supports the compilation of `luac.cross` 
on \*nix patforms which have Lua 5.1, the Lua filesystem module (lfs), and the essential
GCC tools. Simply change directory to the firmware root directoy and run the command:

    lua tools/cross-lua.lua
    
This will generate a `luac.cross` executable in your root directory which can be used to
compile and to syntax-check Lua source on the Development machine for execution under 
NodeMCU Lua on the ESP8266. 
 
