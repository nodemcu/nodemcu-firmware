# **NodeMCU** #
version 0.9.5

[![Join the chat at https://gitter.im/nodemcu/nodemcu-firmware](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/nodemcu/nodemcu-firmware?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/nodemcu/nodemcu-firmware.svg)](https://travis-ci.org/nodemcu/nodemcu-firmware)  [![Download](https://img.shields.io/badge/download-~400k-orange.svg)](https://github.com/nodemcu/nodemcu-firmware/releases/latest)

###A lua based firmware for wifi-soc esp8266
Build on [ESP8266 sdk 0.9.5](http://bbs.espressif.com/viewtopic.php?f=5&t=154)<br />
Lua core based on [eLua project](http://www.eluaproject.net/)<br />
cjson based on [lua-cjson](https://github.com/mpx/lua-cjson)<br />
File system based on [spiffs](https://github.com/pellepl/spiffs)<br />
Open source development kit for NodeMCU [nodemcu-devkit](https://github.com/nodemcu/nodemcu-devkit)<br />
Flash tool for NodeMCU [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher)<br />

wiki: [NodeMCU wiki](https://github.com/nodemcu/nodemcu-firmware/wiki)<br />
api: [NodeMCU api](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_en)<br />
home: [nodemcu.com](http://www.nodemcu.com)<br />
bbs: [Chinese bbs](http://bbs.nodemcu.com)<br />
docs: [NodeMCU docs](http://www.nodemcu.com/docs/)<br />
Tencent QQ group: 309957875<br />

# Summary
- Easy to access wireless router
- Based on Lua 5.1.4 (without *debug, os* module.)
- Event-Drive programming preferred.
- Build-in json, file, timer, pwm, i2c, spi, 1-wire, net, mqtt, coap, gpio, wifi, adc, uart and system api.
- GPIO pin re-mapped, use the index to access gpio, i2c, pwm.
- Both Integer(less memory usage) and Float version firmware provided.

# To Do List (pull requests are very welcomed)
- loadable c module
- fix wifi smart connect
- add spi module (done)
- add mqtt module (done)
- add coap module (done)
- cross compiler (done)

# Change log
2015-03-18<br />
update u8glib.<br />
merge everything to master.

2015-03-17<br />
add cjson module, only cjson.encode() and cjson.decode() is implemented.<br />
read doc [here](https://github.com/nodemcu/nodemcu-firmware/blob/master/app/cjson/manual.txt)

2015-03-15<br />
bugs fixed: #239, #273.<br />
reduce coap module memory usage, add coap module to default built.

2015-03-11<br />
fix bugs of spiffs.<br />
build both float and integer version [latest releases](https://github.com/nodemcu/nodemcu-firmware/releases/latest).<br />
fix tmr.time().<br />
fix memory leak when DNS fail.

2015-03-10<br />
update to the recent spiffs.<br />
add file.fsinfo() api, usage: remain, used, total = file.fsinfo().<br />
add Travis CI. please download the latest firmware from [releases](https://github.com/nodemcu/nodemcu-firmware/releases).<br />
add math lib, partial api work.<br />
u8g module, ws2812 module default enabled in dev-branch build.

2015-02-13<br />
add node.compile() api to compile lua text file into lua bytecode file.<br />
this will reduce memory usage noticeably when require modules into NodeMCU.<br />
raise internal LUA_BUFFERSIZE from 1024 to 4096.<br />
lua require("mod") will load "mod.lc" file first if exist.<br />
build latest pre_build bin.

[more change log](https://github.com/nodemcu/nodemcu-firmware/wiki)<br />

##GPIO NEW TABLE ( Build 20141219 and later)

<a id="new_gpio_map"></a>
<table>
  <tr>
    <th scope="col">IO index</th><th scope="col">ESP8266 pin</th><th scope="col">IO index</th><th scope="col">ESP8266 pin</th>
  </tr>
  <tr>
    <td>0 [*]</td><td>GPIO16</td><td>8</td><td>GPIO15</td>
  </tr>
  <tr>
    <td>1</td><td>GPIO5</td><td>9</td><td>GPIO3</td>
   </tr>
   <tr>
    <td>2</td><td>GPIO4</td><td>10</td><td>GPIO1</td>
  </tr>
  <tr>
    <td>3</td><td>GPIO0</td><td>11</td><td>GPIO9</td>
   </tr>
   <tr>
    <td>4</td><td>GPIO2</td><td>12</td><td>GPIO10</td>
  </tr>
  <tr>
    <td>5</td><td>GPIO14</td><td></td><td></td>
   </tr>
   <tr>
    <td>6</td><td>GPIO12</td><td></td><td></td>
  </tr>
  <tr>
    <td>7</td><td>GPIO13</td><td></td><td></td>
   </tr>
</table>
#### [*] D0(GPIO16) can only be used as gpio read/write. no interrupt supported. no pwm/i2c/ow supported.

#Build option
####file ./app/include/user_modules.h
```c
#define LUA_USE_BUILTIN_STRING    // for string.xxx()
#define LUA_USE_BUILTIN_TABLE   // for table.xxx()
#define LUA_USE_BUILTIN_COROUTINE // for coroutine.xxx()
#define LUA_USE_BUILTIN_MATH    // for math.xxx(), partially work
// #define LUA_USE_BUILTIN_IO       // for io.xxx(), partially work

// #define LUA_USE_BUILTIN_OS     // for os.xxx(), not work
// #define LUA_USE_BUILTIN_DEBUG    // for debug.xxx(), not work

#define LUA_USE_MODULES

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
#define LUA_USE_MODULES_U8G
#define LUA_USE_MODULES_WS2812
#define LUA_USE_MODULES_CJSON
#endif /* LUA_USE_MODULES */
```

#Flash the firmware
nodemcu_latest.bin: 0x00000<br />
for most esp8266 modules, just pull GPIO0 down and restart.<br />
You can use the [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher) to burn the firmware.

Or, if you build your own bin from source code.<br />
0x00000.bin: 0x00000<br />
0x10000.bin: 0x10000<br />

*Better run file.format() after flash*

#Connect the hardware in serial
baudrate:9600

#Start play

####Connect to your ap

```lua
    ip = wifi.sta.getip()
    print(ip)
    --nil
    wifi.setmode(wifi.STATION)
    wifi.sta.config("SSID","password")
    ip = wifi.sta.getip()
    print(ip)
    --192.168.18.110
```

####Manipulate hardware like a arduino

```lua
    pin = 1
    gpio.mode(pin,gpio.OUTPUT)
    gpio.write(pin,gpio.HIGH)
    print(gpio.read(pin))
```

####Write network application in nodejs style

```lua
    -- A simple http client
    conn=net.createConnection(net.TCP, 0)
    conn:on("receive", function(conn, payload) print(payload) end )
    conn:connect(80,"115.239.210.27")
    conn:send("GET / HTTP/1.1\r\nHost: www.baidu.com\r\n"
        .."Connection: keep-alive\r\nAccept: */*\r\n\r\n")
```

####Or a simple http server

```lua
    -- A simple http server
    srv=net.createServer(net.TCP)
    srv:listen(80,function(conn)
      conn:on("receive",function(conn,payload)
        print(payload)
        conn:send("<h1> Hello, NodeMcu.</h1>")
      end)
      conn:on("sent",function(conn) conn:close() end)
    end)
```

####Connect to MQTT Broker

```lua
-- init mqtt client with keepalive timer 120sec
m = mqtt.Client("clientid", 120, "user", "password")

-- setup Last Will and Testament (optional)
-- Broker will publish a message with qos = 0, retain = 0, data = "offline"
-- to topic "/lwt" if client don't send keepalive packet
m:lwt("/lwt", "offline", 0, 0)

m:on("connect", function(con) print ("connected") end)
m:on("offline", function(con) print ("offline") end)

-- on publish message receive event
m:on("message", function(conn, topic, data)
  print(topic .. ":" )
  if data ~= nil then
    print(data)
  end
end)

-- for secure: m:connect("192.168.11.118", 1880, 1)
m:connect("192.168.11.118", 1880, 0, function(conn) print("connected") end)

-- subscribe topic with qos = 0
m:subscribe("/topic",0, function(conn) print("subscribe success") end)
-- or subscribe multiple topic (topic/0, qos = 0; topic/1, qos = 1; topic2 , qos = 2)
-- m:subscribe({["topic/0"]=0,["topic/1"]=1,topic2=2}, function(conn) print("subscribe success") end)
-- publish a message with data = hello, QoS = 0, retain = 0
m:publish("/topic","hello",0,0, function(conn) print("sent") end)

m:close();
-- you can call m:connect again

```

#### UDP client and server
```lua
-- a udp server
s=net.createServer(net.UDP)
s:on("receive",function(s,c) print(c) end)
s:listen(5683)

-- a udp client
cu=net.createConnection(net.UDP)
cu:on("receive",function(cu,c) print(c) end)
cu:connect(5683,"192.168.18.101")
cu:send("hello")
```

####Do something shining
```lua
  function led(r,g,b)
    pwm.setduty(1,r)
    pwm.setduty(2,g)
    pwm.setduty(3,b)
  end
  pwm.setup(1,500,512)
  pwm.setup(2,500,512)
  pwm.setup(3,500,512)
  pwm.start(1)
  pwm.start(2)
  pwm.start(3)
  led(512,0,0) -- red
  led(0,0,512) -- blue
```

####And blink it
```lua
  lighton=0
  tmr.alarm(1,1000,1,function()
    if lighton==0 then
      lighton=1
      led(512,512,512)
    else
      lighton=0
      led(0,0,0)
    end
  end)
```

####If you want to run something when system started
```lua
  --init.lua will be excuted
  file.open("init.lua","w")
  file.writeline([[print("Hello, do this at the beginning.")]])
  file.close()
  node.restart()  -- this will restart the module.
```

####With below code, you can telnet to your esp8266 now
```lua
    -- a simple telnet server
    s=net.createServer(net.TCP,180)
    s:listen(2323,function(c)
       function s_output(str)
          if(c~=nil)
             then c:send(str)
          end
       end
       node.output(s_output, 0)   -- re-direct output to function s_ouput.
       c:on("receive",function(c,l)
          node.input(l)           -- works like pcall(loadstring(l)) but support multiple separate line
       end)
       c:on("disconnection",function(c)
          node.output(nil)        -- un-regist the redirect output function, output goes to serial
       end)
       print("Welcome to NodeMcu world.")
    end)
```

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

####Operate a display via I2c with u8glib
u8glib is a graphics library with support for many different displays.
The integration in nodemcu is developed for SSD1306 based display attached via the I2C port. Further display types and SPI connectivity will be added in the future.

U8glib v1.17

#####I2C connection
Hook up SDA and SCL to any free GPIOs. Eg. `lua_examples/u8glib/graphics_test.lua` expects SDA=5 (GPIO14) and SCL=6 (GPIO12). They are used to set up nodemcu's I2C driver before accessing the display:
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

Also refer to the initialization sequence eg in `lua_examples/u8glib/graphics_test.lua`:
```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, spi.DATABITS_8, 0)
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
disp = u8g.ssd1306_128x64_spi(cs, dc, res)
```

This object provides all of u8glib's methods to control the display.
Again, refer to `lua_examples/u8glib/graphics_test.lua` to get an impression how this is achieved with Lua code. Visit the [u8glib homepage](https://code.google.com/p/u8glib/) for technical details.

#####Fonts
u8glib comes with a wide range of fonts for small displays. Since they need to be compiled into the firmware image, you'd need to include them in `app/include/u8g_config.h` and recompile. Simply add the desired fonts to the font table:
```c
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
    U8G_FONT_TABLE_ENTRY(font_chikita)
```
They'll be available as `u8g.<font_name>` in Lua.

#####Bitmaps
Bitmaps and XBMs are supplied as strings to `drawBitmap()` and `drawXBM()`. This off-loads all data handling from the u8g module to generic methods for binary files. See `lua_examples/u8glib/u8g_bitmaps.lua`. Binary files can be uploaded with [nodemcu-uploader.py](https://github.com/kmpm/nodemcu-uploader).

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
