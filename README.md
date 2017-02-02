# **NodeMCU 2.0.0** #

[![Join the chat at https://gitter.im/nodemcu/nodemcu-firmware](https://img.shields.io/gitter/room/badges/shields.svg)](https://gitter.im/nodemcu/nodemcu-firmware?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/nodemcu/nodemcu-firmware.svg)](https://travis-ci.org/nodemcu/nodemcu-firmware)
[![Documentation Status](https://img.shields.io/badge/docs-master-yellow.svg?style=flat)](http://nodemcu.readthedocs.io/en/master/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat)](https://github.com/nodemcu/nodemcu-firmware/blob/master/LICENSE)

### A Lua based firmware for ESP8266 WiFi SOC

NodeMCU is an [eLua](http://www.eluaproject.net/) based firmware for the [ESP8266 WiFi SOC from Espressif](http://espressif.com/en/products/esp8266/). The firmware is based on the [Espressif NON-OS SDK 2.0.0](https://espressif.com/sites/default/files/sdks/esp8266_nonos_sdk_v2.0.0_16_08_10.zip) and uses a file system based on [spiffs](https://github.com/pellepl/spiffs). The code repository consists of 98.1% C-code that glues the thin Lua veneer to the SDK.

The NodeMCU *firmware* is a companion project to the popular [NodeMCU dev kits](https://github.com/nodemcu/nodemcu-devkit-v1.0), ready-made open source development boards with ESP8266-12E chips.

# Summary

- Easy to program wireless node and/or access point
- Based on Lua 5.1.4 (without *debug, os* modules)
- Asynchronous event-driven programming model
- 40+ built-in modules
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
wifi.setmode(wifi.STATION)
wifi.sta.config("SSID", "password")
```

# Documentation

The entire [NodeMCU documentation](https://nodemcu.readthedocs.io) is maintained right in this repository at [/docs](docs). The fact that the API documentation is mainted in the same repository as the code that *provides* the API ensures consistency between the two. With every commit the documentation is rebuilt by Read the Docs and thus transformed from terse Markdown into a nicely browsable HTML site at [https://nodemcu.readthedocs.io](https://nodemcu.readthedocs.io). 

- How to [build the firmware](https://nodemcu.readthedocs.io/en/master/en/build/)
- How to [build the filesystem](https://nodemcu.readthedocs.io/en/master/en/spiffs/)
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

# Build Options

The following sections explain some of the options you have if you want to [build your own NodeMCU firmware](http://nodemcu.readthedocs.io/en/master/en/build/).

### Select Modules

Disable modules you won't be using to reduce firmware size and free up some RAM. The ESP8266 is quite limited in available RAM and running out of memory can cause a system panic. The default configuration is designed to run on all ESP modules including the 512 KB modules like ESP-01 and only includes general purpose interface modules which require at most two GPIO pins.

Edit `app/include/user_modules.h` and comment-out the `#define` statement for modules you don't need. Example:

```c
...
#define LUA_USE_MODULES_MQTT
// #define LUA_USE_MODULES_COAP
// #define LUA_USE_MODULES_U8G
...
```

### Tag Your Build

Identify your firmware builds by editing `app/include/user_version.h`

```c
#define NODE_VERSION    "NodeMCU 2.0.0+myname"
#ifndef BUILD_DATE
#define BUILD_DATE      "YYYYMMDD"
#endif
```

### Set UART Bit Rate

The initial baud rate at boot time is 115200bps. You can change this by
editing `BIT_RATE_DEFAULT` in `app/include/user_config.h`:

```c
#define BIT_RATE_DEFAULT BIT_RATE_115200
```

Note that, by default, the firmware runs an auto-baudrate detection algorithm so that typing a few characters at boot time will cause
the firmware to lock onto that baud rate (between 1200 and 230400). 

### Debugging

To enable runtime debug messages to serial console edit `app/include/user_config.h`

```c
#define DEVELOP_VERSION
```
