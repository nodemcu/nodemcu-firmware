# **NodeMCU 2.2.1** #

[![Join the chat at https://gitter.im/nodemcu/nodemcu-firmware](https://img.shields.io/gitter/room/badges/shields.svg)](https://gitter.im/nodemcu/nodemcu-firmware?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/nodemcu/nodemcu-firmware.svg)](https://travis-ci.org/nodemcu/nodemcu-firmware)
[![Documentation Status](https://img.shields.io/badge/docs-master-yellow.svg?style=flat)](http://nodemcu.readthedocs.io/en/master/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat)](https://github.com/nodemcu/nodemcu-firmware/blob/master/LICENSE)

### A Lua based firmware for ESP8266 WiFi SOC

NodeMCU is an open source [Lua](https://www.lua.org/) based firmware for the [ESP8266 WiFi SOC from Espressif](http://espressif.com/en/products/esp8266/) and uses an on-module flash-based [SPIFFS](https://github.com/pellepl/spiffs) file system. NodeMCU is implemented in C and is layered on the [Espressif NON-OS SDK](https://github.com/espressif/ESP8266_NONOS_SDK).

The firmware was initially developed as is a companion project to the popular ESP8266-based [NodeMCU development modules]((https://github.com/nodemcu/nodemcu-devkit-v1.0)), but the project is now community-supported, and the firmware can now be run on _any_ ESP module.

# Summary

- Easy to program wireless node and/or access point
- Based on Lua 5.1.4 (without `debug` & `os` modules)
- Asynchronous event-driven programming model
- more than **65 built-in modules**
- Firmware available with or without floating point support (integer-only uses less memory)
- Up-to-date documentation at [https://nodemcu.readthedocs.io](https://nodemcu.readthedocs.io)

### LFS support
In July 2018 support for a Lua Flash Store (LFS) was introduced. LFS  allows Lua code and its associated constant data to be executed directly out of flash-memory; just as the firmware itself is executed. This now enables NodeMCU developers to create **Lua applications with up to 256Kb** Lua code and read-only constants executing out of flash. All of the RAM is available for read-write data!

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
wifi.setmode(wifi.STATION)
wifi.sta.config{ssid="SSID", pwd="password"}
```

# Documentation

The entire [NodeMCU documentation](https://nodemcu.readthedocs.io) is maintained right in this repository at [/docs](docs). The fact that the API documentation is maintained in the same repository as the code that *provides* the API ensures consistency between the two. With every commit the documentation is rebuilt by Read the Docs and thus transformed from terse Markdown into a nicely browsable HTML site at [https://nodemcu.readthedocs.io](https://nodemcu.readthedocs.io).

- How to [build the firmware](https://nodemcu.readthedocs.io/en/master/en/build/)
- How to [flash the firmware](https://nodemcu.readthedocs.io/en/master/en/flash/)
- How to [upload code and NodeMCU IDEs](https://nodemcu.readthedocs.io/en/master/en/upload/)
- API documentation for every module

# Releases

Due to the ever-growing number of modules available within NodeMCU, pre-built binaries are no longer made available. Use the automated [custom firmware build service](http://nodemcu-build.com/) to get the specific firmware configuration you need, or consult the [documentation](http://nodemcu.readthedocs.io/en/master/en/build/) for other options to build your own firmware.

This project uses two main branches, `master` and `dev`. `dev` is actively worked on and it's also where PRs should be created against. `master` thus can be considered "stable" even though there are no automated regression tests. The goal is to merge back to `master` roughly every 2 months. Depending on the current "heat" (issues, PRs) we accept changes to `dev` for 5-6 weeks and then hold back for 2-3 weeks before the next snap is completed.

A new tag is created every time `dev` is merged back to `master`. They are listed in the [releases section here on GitHub](https://github.com/nodemcu/nodemcu-firmware/releases). Tag names follow the \<SDK-version\>-master_yyyymmdd pattern.

# Support

See [https://nodemcu.readthedocs.io/en/master/en/support/](https://nodemcu.readthedocs.io/en/master/en/support/).

# License

[MIT](https://github.com/nodemcu/nodemcu-firmware/blob/master/LICENSE) © [zeroday](https://github.com/NodeMCU)/[nodemcu.com](http://nodemcu.com/index_en.html)
