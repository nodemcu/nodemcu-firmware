# **NodeMCU 1.4.0** #

[![Join the chat at https://gitter.im/nodemcu/nodemcu-firmware](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/nodemcu/nodemcu-firmware?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/nodemcu/nodemcu-firmware.svg)](https://travis-ci.org/nodemcu/nodemcu-firmware)

###A lua based firmware for wifi-soc esp8266
  - Build on [ESP8266 NONOS SDK 1.4.0](http://bbs.espressif.com/viewtopic.php?f=46&t=1124)
  - Lua core based on [eLua project](http://www.eluaproject.net/)
  - cjson based on [lua-cjson](https://github.com/mpx/lua-cjson)
  - File system based on [spiffs](https://github.com/pellepl/spiffs)
  - Open source development kit for NodeMCU [nodemcu-devkit-v0.9](https://github.com/nodemcu/nodemcu-devkit) [nodemcu-devkit-v1.0](https://github.com/nodemcu/nodemcu-devkit-v1.0)

# Summary

- Easy to program wireless node and/or Access Point
- Based on Lua 5.1.4 (without *debug, os* module.)
- Event-driven programming model preferred
- Built-in modules: node, json, file, timer, pwm, i2c, spi, onewire, net, mqtt, coap, gpio, wifi, adc, uart, bit, u8g, ucg, ws2801, ws2812, crypto, dht, rtc, sntp, bmp085, tls2561, hx711 and system api.
- Both Integer (less memory usage) and Float version firmware provided.

## Useful links

| Resource | Location |
| -------------- | -------------- |
| Developer Wiki       | https://github.com/nodemcu/nodemcu-firmware/wiki |
| API docs             | [NodeMCU api](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_en) |
| Home                 | [nodemcu.com](http://www.nodemcu.com) |
| BBS                  | [Chinese BBS](http://bbs.nodemcu.com) |
| Docs                 | [NodeMCU docs](http://www.nodemcu.com/docs/) |
| Tencent QQ group     | 309957875 |
| Windows flash tool   | [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher) |
| Linux flash tool     | [Esptool](https://github.com/themadinventor/esptool) |
| ESPlorer GUI         | https://github.com/4refr0nt/ESPlorer |
| NodeMCU Studio GUI   | https://github.com/nodemcu/nodemcu-studio-csharp |

# Programming Examples

Because Lua is a high level language and several modules are built into the firmware, you can very easily program your ESP8266. Here are some examples!

## Connect to your AP

```lua
    ip = wifi.sta.getip()
    print(ip)
    --nil
    wifi.setmode(wifi.STATION)
    wifi.sta.config("SSID", "password")
    ip = wifi.sta.getip()
    print(ip)
    --192.168.18.110
```

## Manipulate hardware like an Arduino

```lua
    pin = 1
    gpio.mode(pin, gpio.OUTPUT)
    gpio.write(pin, gpio.HIGH)
    print(gpio.read(pin))
```

## Write a network application in Node.js style

```lua
    -- A simple http client
    conn=net.createConnection(net.TCP, 0)
    conn:on("receive", function(conn, payload) print(payload) end)
    conn:connect(80, "115.239.210.27")
    conn:send("GET / HTTP/1.1\r\nHost: www.baidu.com\r\n"
        .. "Connection: keep-alive\r\nAccept: */*\r\n\r\n")
```

## Or a simple HTTP server

```lua
    -- A simple http server
    srv=net.createServer(net.TCP)
    srv:listen(80, function(conn)
      conn:on("receive", function(conn,payload)
        print(payload)
        conn:send("<h1> Hello, NodeMCU.</h1>")
      end)
      conn:on("sent", function(conn) conn:close() end)
    end)
```

## Connect to MQTT broker

```lua
-- init mqtt client with keepalive timer 120sec
m = mqtt.Client("clientid", 120, "user", "password")

-- setup Last Will and Testament (optional)
-- Broker will publish a message with qos = 0, retain = 0, data = "offline"
-- to topic "/lwt" if client doesn't send keepalive packet
m:lwt("/lwt", "offline", 0, 0)

m:on("connect", function(con) print("connected") end)
m:on("offline", function(con) print("offline") end)

-- on publish message receive event
m:on("message", function(conn, topic, data)
  print(topic .. ":")
  if data ~= nil then
    print(data)
  end
end)

-- m:connect(host, port, secure, auto_reconnect, function(client) end)
-- for secure: m:connect("192.168.11.118", 1880, 1, 0)
-- for auto-reconnect: m:connect("192.168.11.118", 1880, 0, 1)
m:connect("192.168.11.118", 1880, 0, 0, function(conn) print("connected") end)

-- subscribe to topic with qos = 0
m:subscribe("/topic", 0, function(conn) print("subscribe success") end)
-- or subscribe multiple topics (topic/0, qos = 0; topic/1, qos = 1; topic2, qos = 2)
-- m:subscribe({["topic/0"]=0,["topic/1"]=1,topic2=2}, function(conn) print("subscribe success") end)
-- publish a message with data = hello, QoS = 0, retain = 0
m:publish("/topic", "hello", 0, 0, function(conn) print("sent") end)

m:close();  -- if auto-reconnect == 1, it will disable auto-reconnect and then disconnect from host.
-- you can call m:connect again

```

## UDP client and server

```lua
-- a udp server
s=net.createServer(net.UDP)
s:on("receive", function(s, c) print(c) end)
s:listen(5683)

-- a udp client
cu=net.createConnection(net.UDP)
cu:on("receive", function(cu, c) print(c) end)
cu:connect(5683, "192.168.18.101")
cu:send("hello")
```

## Do something shiny with an RGB LED

```lua
  function led(r, g, b)
    pwm.setduty(1, r)
    pwm.setduty(2, g)
    pwm.setduty(3, b)
  end
  pwm.setup(1, 500, 512)
  pwm.setup(2, 500, 512)
  pwm.setup(3, 500, 512)
  pwm.start(1)
  pwm.start(2)
  pwm.start(3)
  led(512, 0, 0) -- red
  led(0, 0, 512) -- blue
```

## And blink it

```lua
  lighton=0
  tmr.alarm(1, 1000, 1, function()
    if lighton==0 then
      lighton=1
      led(512, 512, 512)
    else
      lighton=0
      led(0, 0, 0)
    end
  end)
```

## If you want to run something when the system boots

```lua
  --init.lua will be executed
  file.open("init.lua", "w")
  file.writeline([[print("Hello, do this at the beginning.")]])
  file.close()
  node.restart()  -- this will restart the module.
```

## Add a simple telnet server to the Lua interpreter

```lua
    -- a simple telnet server
    s=net.createServer(net.TCP, 180)
    s:listen(2323, function(c)
       function s_output(str)
          if(c~=nil)
             then c:send(str)
          end
       end
       node.output(s_output, 0)   -- re-direct output to function s_ouput.
       c:on("receive", function(c, l)
          node.input(l)           -- works like pcall(loadstring(l)) but support multiples separate lines
       end)
       c:on("disconnection", function(c)
          node.output(nil)        -- un-register the redirect output function, output goes to serial
       end)
       print("Welcome to NodeMCU world.")
    end)
```

# Building the firmware

There are several options for building the NodeMCU firmware.

## Online firmware custom build

Please try Marcel's [NodeMCU custom builds](http://frightanic.com/nodemcu-custom-build) cloud service and you can choose only the modules you need, and download the firmware once built.

NodeMCU custom builds can build from the master branch and dev branch (with the latest fixes).

## Docker containerised build

See https://hub.docker.com/r/asmaps/nodemcu-builder/

This docker image includes the build toolchain and SDK. You just run the docker
image with your checked-out NodeMCU firmware repository (this one). The docker
image can also flash the firmware to your device.

You will need to see BUILD OPTIONS below, to configure the firmware before building.

## Build it yourself

See BUILD OPTIONS below, to configure the firmware before building.

### Minimum requirements:

  - unrar
  - GNU autoconf, automake, libtool
  - GNU gcc, g++, make
  - GNU flex, bison, gawk, sed
  - python, python-serial, libexpat-dev
  - srecord
  - The esp-open-sdk from https://github.com/pfalcon/esp-open-sdk

### Build instructions:

Assuming NodeMCU firmware is checked-out to `/opt/nodemcu-firmware`:

```sh
git clone --recursive https://github.com/pfalcon/esp-open-sdk.git /opt/esp-open-sdk
cd /opt/esp-open-sdk
make STANDALONE=y
PATH=/opt/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
cd /opt/nodemcu-firmware
make
```

# BUILD OPTIONS

Disable modules you won't be using, to reduce firmware size on flash and
free more RAM. The ESP8266 is quite limited in available RAM, and running
out can cause a system panic.

## Edit `app/include/user_modules.h`

Comment-out the #define statement for unused modules. Example:

```c
#ifdef LUA_USE_MODULES
#define LUA_USE_MODULES_NODE
#define LUA_USE_MODULES_FILE
#define LUA_USE_MODULES_GPIO
#define LUA_USE_MODULES_WIFI
#define LUA_USE_MODULES_NET
#define LUA_USE_MODULES_PWM
#define LUA_USE_MODULES_I2C
#define LUA_USE_MODULES_SPI
#define LUA_USE_MODULES_TMR
#define LUA_USE_MODULES_ADC
#define LUA_USE_MODULES_UART
#define LUA_USE_MODULES_OW
#define LUA_USE_MODULES_BIT
#define LUA_USE_MODULES_MQTT
// #define LUA_USE_MODULES_COAP
// #define LUA_USE_MODULES_U8G
// #define LUA_USE_MODULES_WS2801
// #define LUA_USE_MODULES_WS2812
// #define LUA_USE_MODULES_CJSON
#define LUA_USE_MODULES_CRYPTO
#define LUA_USE_MODULES_RC
#define LUA_USE_MODULES_DHT
#define LUA_USE_MODULES_RTCMEM
#define LUA_USE_MODULES_RTCTIME
#define LUA_USE_MODULES_RTCFIFO
#define LUA_USE_MODULES_SNTP
// #define LUA_USE_MODULES_BMP085
#define LUA_USE_MODULES_TSL2561
// #define LUA_USE_MODULES_HX711

#endif /* LUA_USE_MODULES */
```

## Tagging your build

Identify your firmware builds by editing `app/include/user_version.h`

```c
#define NODE_VERSION    "NodeMCU 1.4.0+myname"
#ifndef BUILD_DATE
#define BUILD_DATE        "YYYYMMDD"
#endif
```

## Setting the boot time serial interface rate

The initial baud rate at boot time is 9600 bps, but you can change this by
editing `app/include/user_config.h` and change BIT_RATE_DEFAULT, e.g.:

```c
#define BIT_RATE_DEFAULT BIT_RATE_115200
```

## Debugging

To enable runtime debug messages to serial console, edit `app/include/user_config.h`

```c
#define DEVELOP_VERSION
```

`DEVELOP_VERSION` changes the startup baud rate to 74880.

# Flash the firmware

## Flash tools for Windows

You can use the [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher) to burn the firmware.

## Flash tools for Linux

Esptool is a python utility which can read and write the flash in an ESP8266 device. See https://github.com/themadinventor/esptool

## Preparing the hardware for firmware upgrade

To enable ESP8266 firmware flashing, the GPIO0 pin must be pulled low before
the device is reset. Conversely, for a normal boot, GPIO0 must be pulled high
or floating.

If you have a [NodeMCU Development Kit](http://www.nodemcu.com/index_en.html) then
you don't need to do anything, as the USB connection can pull GPIO0
low by asserting DTR, and reset your board by asserting RTS.

If you have an ESP-01 or other device without inbuilt USB, you will need to
enable flashing yourself by pulling GPIO0 low or pressing a "flash" switch.

## Files to burn to the flash

If you got your firmware from [NodeMCU custom builds](http://frightanic.com/nodemcu-custom-build) then you can flash that file directly to address 0x00000.

Otherwise, if you built your own firmware from source code:
  - bin/0x00000.bin to 0x00000
  - bin/0x10000.bin to 0x10000

Also, in some special circumstances, you may need to flash `blank.bin` or `esp_init_data_default.bin` to various addresses on the flash (depending on flash size and type).

If upgrading from `spiffs` version 0.3.2 to 0.3.3 or later, or after flashing any new firmware (particularly one with a much different size), you may need to run `file.format()` to re-format your flash filesystem.
You will know if you need to do this because your flash files disappeared, or they exist but seem empty, or data cannot be written to new files.

# Connecting to your NodeMCU device

NodeMCU serial interface uses 9600 baud at boot time. To increase the speed after booting, issue `uart.setup(0,115200,8,0,1,1)` (ESPlorer will do this automatically when changing the speed in the dropdown list).

If the device panics and resets at any time, errors will be written to the serial interface at 115200 bps.

# User Interface tools

## Esplorer

Victor Brutskiy's [ESPlorer](https://github.com/4refr0nt/ESPlorer) is written in Java, is open source and runs on most platforms such as Linux, Windows, Mac OS, etc.

#### Features

  - Edit Lua scripts and run on the ESP8266 and save to its flash
  - Serial console log
  - Also supports original AT firmware (reading and setting WiFi modes, etc)

## NodeMCU Studio

[NodeMCU Studio](https://github.com/nodemcu/nodemcu-studio-csharp) is written in C# and supports Windows. This software is open source and can write lua files to filesystem.

# OPTIONAL MODULES

####Use DS18B20 module extends your esp8266
```lua
    -- read temperature with DS18B20
    node.compile("ds18b20.lua")   --  run this only once to compile and save to "ds18b20.lc"
    t=require("ds18b20")
    t.setup(9)
    addrs=t.addrs()
    -- Total DS18B20 numbers, assume it is 2
    print(table.getn(addrs))
    -- The first DS18B20
    print(t.read(addrs[1],t.C))
    print(t.read(addrs[1],t.F))
    print(t.read(addrs[1],t.K))
    -- The second DS18B20
    print(t.read(addrs[2],t.C))
    print(t.read(addrs[2],t.F))
    print(t.read(addrs[2],t.K))
    -- Just read
    print(t.read())
    -- Just read as centigrade
    print(t.read(nil,t.C))
    -- Don't forget to release it after use
    t = nil
	ds18b20 = nil
    package.loaded["ds18b20"]=nil
```

####Operate a display with u8glib
u8glib is a graphics library with support for many different displays. The nodemcu firmware supports a subset of these.
Both I2C and SPI:
* sh1106_128x64
* ssd1306 - 128x64 and 64x48 variants
* ssd1309_128x64
* ssd1327_96x96_gr
* uc1611 - dogm240 and dogxl240 variants

SPI only:
* ld7032_60x32
* pcd8544_84x48
* pcf8812_96x65
* ssd1322_nhd31oled - bw and gr variants
* ssd1325_nhd27oled - bw and gr variants
* ssd1351_128x128 - gh and hicolor variants
* st7565_64128n - variants 64128n, dogm128/132, lm6059/lm6063, c12832/c12864
* uc1601_c128032
* uc1608 - 240x128 and 240x64 variants
* uc1610_dogxl160 - bw and gr variants
* uc1611 - dogm240 and dogxl240 variants
* uc1701 - dogs102 and mini12864 variants

U8glib v1.18.1

#####I2C connection
Hook up SDA and SCL to any free GPIOs. Eg. [u8g_graphics_test.lua](lua_examples/u8glib/u8g_graphics_test.lua) expects SDA=5 (GPIO14) and SCL=6 (GPIO12). They are used to set up nodemcu's I2C driver before accessing the display:
```lua
sda = 5
scl = 6
i2c.setup(0, sda, scl, i2c.SLOW)
```

#####SPI connection
The HSPI module is used, so certain pins are fixed:
* HSPI CLK  = GPIO14
* HSPI MOSI = GPIO13
* HSPI MISO = GPIO12 (not used)

All other pins can be assigned to any available GPIO:
* CS
* D/C
* RES (optional for some displays)

Also refer to the initialization sequence eg in [u8g_graphics_test.lua](lua_examples/u8glib/u8g_graphics_test.lua):
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
```


#####Library usage
The Lua bindings for this library closely follow u8glib's object oriented C++ API. Based on the u8g class, you create an object for your display type.

SSD1306 via I2C:
```lua
sla = 0x3c
disp = u8g.ssd1306_128x64_i2c(sla)
```
SSD1306 via SPI:
```lua
cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = u8g.ssd1306_128x64_hw_spi(cs, dc, res)
```

This object provides all of u8glib's methods to control the display.
Again, refer to [u8g_graphics_test.lua](lua_examples/u8glib/u8g_graphics_test.lua) to get an impression how this is achieved with Lua code. Visit the [u8glib homepage](https://github.com/olikraus/u8glib) for technical details.

#####Displays
I2C and HW SPI based displays with support in u8glib can be enabled. To get access to the respective constructors, add the desired entries to the I2C or SPI display tables in [app/include/u8g_config.h](app/include/u8g_config.h):
```c
#define U8G_DISPLAY_TABLE_I2C                   \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_i2c) \

#define U8G_DISPLAY_TABLE_SPI                      \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_hw_spi) \
    U8G_DISPLAY_TABLE_ENTRY(pcd8544_84x48_hw_spi)  \
    U8G_DISPLAY_TABLE_ENTRY(pcf8812_96x65_hw_spi)  \
```
An exhaustive list of available displays can be found in the [u8g module wiki entry](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_en#u8g-module).


#####Fonts
u8glib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in [app/include/u8g_config.h](app/include/u8g_config.h) and recompile. Simply add the desired fonts to the font table:
```c
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
    U8G_FONT_TABLE_ENTRY(font_chikita)
```
They'll be available as `u8g.<font_name>` in Lua.

#####Bitmaps
Bitmaps and XBMs are supplied as strings to `drawBitmap()` and `drawXBM()`. This off-loads all data handling from the u8g module to generic methods for binary files. See [u8g_bitmaps.lua](lua_examples/u8glib/u8g_bitmaps.lua).
In contrast to the source code based inclusion of XBMs into u8glib, it's required to provide precompiled binary files. This can be performed online with [Online-Utility's Image Converter](http://www.online-utility.org/image_converter.jsp): Convert from XBM to MONO format and upload the binary result with [nodemcu-uploader.py](https://github.com/kmpm/nodemcu-uploader).

#####Unimplemented functions
- [ ] Cursor handling
  - [ ] disableCursor()
  - [ ] enableCursor()
  - [ ] setCursorColor()
  - [ ] setCursorFont()
  - [ ] setCursorPos()
  - [ ] setCursorStyle()
- [ ] General functions
  - [ ] setContrast()
  - [ ] setPrintPos()
  - [ ] setHardwareBackup()
  - [ ] setRGB()
  - [ ] setDefaultMidColor()

####Operate a display with ucglib
Ucglib is a graphics library with support for color TFT displays.

Ucglib v1.3.3

#####SPI connection
The HSPI module is used, so certain pins are fixed:
* HSPI CLK  = GPIO14
* HSPI MOSI = GPIO13
* HSPI MISO = GPIO12 (not used)

All other pins can be assigned to any available GPIO:
* CS
* D/C
* RES (optional for some displays)

Also refer to the initialization sequence eg in [GraphicsTest.lua](lua_examples/ucglib/GraphicsRest.lua):
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
```

#####Library usage
The Lua bindings for this library closely follow ucglib's object oriented C++ API. Based on the ucg class, you create an object for your display type.

ILI9341 via SPI:
```lua
cs  = 8 -- GPIO15, pull-down 10k to GND
dc  = 4 -- GPIO2
res = 0 -- GPIO16, RES is optional YMMV
disp = ucg.ili9341_18x240x320_hw_spi(cs, dc, res)
```

This object provides all of ucglib's methods to control the display.
Again, refer to [GraphicsTest.lua](lua_examples/ucglib/GraphicsTest.lua) to get an impression how this is achieved with Lua code. Visit the [ucglib homepage](https://github.com/olikraus/ucglib) for technical details.

#####Displays
To get access to the display constructors, add the desired entries to the display table in [app/include/ucg_config.h](app/include/ucg_config.h):
```c
#define UCG_DISPLAY_TABLE                          \
    UCG_DISPLAY_TABLE_ENTRY(ili9341_18x240x320_hw_spi, ucg_dev_ili9341_18x240x320, ucg_ext_ili9341_18) \
    UCG_DISPLAY_TABLE_ENTRY(st7735_18x128x160_hw_spi, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18) \
```

#####Fonts
ucglib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in [app/include/ucg_config.h](app/include/ucg_config.h) and recompile. Simply add the desired fonts to the font table:
```c
#define UCG_FONT_TABLE                              \
    UCG_FONT_TABLE_ENTRY(font_7x13B_tr)             \
    UCG_FONT_TABLE_ENTRY(font_helvB12_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB18_hr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR12_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR14_hr)
```
They'll be available as `ucg.<font_name>` in Lua.


####Control a WS2812 based light strip
```lua
	-- set the color of one LED on GPIO2 to red
	ws2812.writergb(4, string.char(255, 0, 0))
	-- set the color of 10 LEDs on GPIO0 to blue
	ws2812.writergb(3, string.char(0, 0, 255):rep(10))
	-- first LED green, second LED white
	ws2812.writergb(4, string.char(0, 255, 0, 255, 255, 255))
```

####coap client and server
```lua
-- use copper addon for firefox
cs=coap.Server()
cs:listen(5683)

myvar=1
cs:var("myvar") -- get coap://192.168.18.103:5683/v1/v/myvar will return the value of myvar: 1

all='[1,2,3]'
cs:var("all", coap.JSON) -- sets content type to json

-- function should tack one string, return one string.
function myfun(payload)
  print("myfun called")
  respond = "hello"
  return respond
end
cs:func("myfun") -- post coap://192.168.18.103:5683/v1/f/myfun will call myfun

cc = coap.Client()
cc:get(coap.CON, "coap://192.168.18.100:5683/.well-known/core")
cc:post(coap.NON, "coap://192.168.18.100:5683/", "Hello")
```

####cjson
```lua
-- Note that when cjson deal with large content, it may fails a memory allocation, and leaks a bit of memory.
-- so it's better to detect that and schedule a restart.
--
-- Translate Lua value to/from JSON
-- text = cjson.encode(value)
-- value = cjson.decode(text)
json_text = '[ true, { "foo": "bar" } ]'
value = cjson.decode(json_text)
-- Returns: { true, { foo = "bar" } }
value = { true, { foo = "bar" } }
json_text = cjson.encode(value)
-- Returns: '[true,{"foo":"bar"}]'
```

####Read an HX711 load cell ADC.
Note: currently only chanel A with gain 128 is supported.
The HX711 is an inexpensive 24bit ADC with programmable 128x, 64x, and 32x gain.
```lua
	-- Initialize the hx711 with clk on pin 5 and data on pin 6
	hx711.init(5,6)
	-- Read ch A with 128 gain.
	raw_data = hx711.read(0)
```

####Universal DHT Sensor support
Support DHT11, DHT21, DHT22, DHT33, DHT44, etc.
Use all-in-one function to read DHT sensor.
```lua

pin = 5
status,temp,humi,temp_decimial,humi_decimial = dht.readxx(pin)
if( status == dht.OK ) then
  -- Integer firmware using this example
  print(
    string.format(
      "DHT Temperature:%d.%03d;Humidity:%d.%03d\r\n",
      math.floor(temp),
      temp_decimial,
      math.floor(humi),
      humi_decimial
    )
  )
  -- Float firmware using this example
  print("DHT Temperature:"..temp..";".."Humidity:"..humi)
elseif( status == dht.ERROR_CHECKSUM ) then
  print( "DHT Checksum error." );
elseif( status == dht.ERROR_TIMEOUT ) then
  print( "DHT Time out." );
end

```
