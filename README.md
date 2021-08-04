# **NodeMCU on ESP32** #

[![Join the chat at https://gitter.im/nodemcu/nodemcu-firmware](https://img.shields.io/gitter/room/badges/shields.svg)](https://gitter.im/nodemcu/nodemcu-firmware?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![CI](https://github.com/nodemcu/nodemcu-firmware/actions/workflows/build.yml/badge.svg)](https://github.com/nodemcu/nodemcu-firmware/actions/workflows/build.yml)
[![Documentation Status](https://img.shields.io/badge/docs-dev_esp32-yellow.svg?style=flat)](http://nodemcu.readthedocs.io/en/dev-esp32/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat)](https://github.com/nodemcu/nodemcu-firmware/blob/master/LICENSE)

### A Lua based firmware for ESP32 WiFi SOC

NodeMCU is an [eLua](http://www.eluaproject.net/) based firmware for the [ESP32 WiFi SOC from Espressif](http://http://espressif.com/en/products/hardware/esp32/overview). The firmware is based on the [Espressif IoT Development Framework](https://github.com/espressif/esp-idf) and uses a file system based on [spiffs](https://github.com/pellepl/spiffs). The code repository consists of 98.1% C-code that glues the thin Lua veneer to the SDK.

The NodeMCU *firmware* is a companion project to the popular [NodeMCU dev kits](https://github.com/nodemcu/nodemcu-devkit-v1.0), ready-made open source development boards with ESP8266-12E chips.

# Summary

- Easy to program wireless node and/or access point
- Based on Lua 5.1.4 (without *debug, os* modules)
- Asynchronous event-driven programming model
- 10+ built-in modules
- Firmware available with or without floating point support (integer-only uses less memory)
- Up-to-date documentation at [https://nodemcu.readthedocs.io](https://nodemcu.readthedocs.io)

# Programming Model

The NodeMCU programming model is similar to that of [Node.js](https://en.wikipedia.org/wiki/Node.js), only in Lua. It is asynchronous and event-driven. Many functions, therefore, have parameters for callback functions. To give you an idea what a NodeMCU program looks like study the short snippets below. For more extensive examples have a look at the [`/lua_examples`](lua_examples) folder in the repository on GitHub.

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
wifi.mode(wifi.STATION, true)

wifi.sta.on("connected", function() print("connected") end)
wifi.sta.on("got_ip", function(event, info) print("got ip "..info.ip) end)

-- mandatory to start wifi after reset
wifi.start()
wifi.sta.config({ssid="SSID", pwd="password", auto=true}, true)
```

# Documentation

The entire [NodeMCU documentation](https://nodemcu.readthedocs.io/en/dev-esp32/) is maintained right in this repository at [/docs](docs). The fact that the API documentation is maintained in the same repository as the code that *provides* the API ensures consistency between the two. With every commit the documentation is rebuilt by Read the Docs and thus transformed from terse Markdown into a nicely browsable HTML site at [https://nodemcu.readthedocs.io/en/dev-esp32/](https://nodemcu.readthedocs.io/en/dev-esp32/). 

- How to [build the firmware](https://nodemcu.readthedocs.io/en/dev-esp32/en/build/)
- How to [flash the firmware](https://nodemcu.readthedocs.io/en/dev-esp32/en/flash/)
- How to [upload code and NodeMCU IDEs](https://nodemcu.readthedocs.io/en/dev-esp32/en/upload/)
- API documentation for every module

# Support

See [https://nodemcu.readthedocs.io/en/dev/en/support/](https://nodemcu.readthedocs.io/en/dev/en/support/).

# License

[MIT](https://github.com/nodemcu/nodemcu-firmware/blob/master/LICENSE) © [zeroday](https://github.com/NodeMCU)/[nodemcu.com](http://nodemcu.com/index_en.html)
