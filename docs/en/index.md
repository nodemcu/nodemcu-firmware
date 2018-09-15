# NodeMCU Documentation

NodeMCU is an open source [Lua](https://www.lua.org/) based firmware for the [ESP8266 WiFi SOC from Espressif](http://espressif.com/en/products/esp8266/) and uses an on-module flash-based [SPIFFS](https://github.com/pellepl/spiffs) file system. NodeMCU is implemented in C and is layered on the [Espressif NON-OS SDK](https://github.com/espressif/ESP8266_NONOS_SDK).

The firmware was initially developed as is a companion project to the popular ESP8266-based [NodeMCU development modules](https://github.com/nodemcu/nodemcu-devkit-v1.0), but the project is now community-supported, and the firmware can now be run on _any_ ESP module.

## Programming Model
The NodeMCU programming model is similar to that of [Node.js](https://en.wikipedia.org/wiki/Node.js), only in Lua. It is asynchronous and event-driven. Many functions, therefore, have parameters for callback functions. To give you an idea what a NodeMCU program looks like study the short snippets below. For more extensive examples have a look at the `/lua_examples` folder in the repository on GitHub.

```lua
-- a simple HTTP server
srv = net.createServer(net.TCP)
srv:listen(80, function(conn)
	conn:on("receive", function(sck, payload)
		print(payload)
		sck:send("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1> Hello, NodeMCU.</h1>")
	end)
	conn:on("sent", function(sck) sck:close() end)
end)
```
```lua
-- connect to WiFi access point
wifi.setmode(wifi.STATION)
wifi.sta.config("SSID", "password")
```

```lua
-- register event callbacks for WiFi events
wifi.sta.eventMonReg(wifi.STA_CONNECTING, function(previous_state)
	if(previous_state==wifi.STA_GOTIP) then 
	    print("Station lost connection with access point. Attempting to reconnect...")
	else
	    print("STATION_CONNECTING")
	end
end)
```

```lua
-- manipulate hardware like with Arduino
pin = 1
gpio.mode(pin, gpio.OUTPUT)
gpio.write(pin, gpio.HIGH)
print(gpio.read(pin))
```
### Lua Flash Store (LFS)
In July 2018 support for a [Lua Flash Store (LFS)](lfs.md) was introduced. LFS allows Lua code and its associated constant data to be executed directly out of flash-memory; just as the firmware itself is executed. This now enables NodeMCU developers to create Lua applications with up to 256Kb Lua code and read-only constants executing out of flash. All of the RAM is available for read-write data!

## Getting Started
1. [Build the firmware](build.md) with the modules you need.
1. [Flash the firmware](flash.md) to the chip.
1. [Upload code](upload.md) to the firmware.


