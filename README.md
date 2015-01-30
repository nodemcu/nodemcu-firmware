# **NodeMcu** #
version 0.9.5
###A lua based firmware for wifi-soc esp8266
Build on [ESP8266 sdk 0.9.5](http://bbs.espressif.com/viewtopic.php?f=5&t=154)<br />
Lua core based on [eLua project](http://www.eluaproject.net/)<br />
File system based on [spiffs](https://github.com/pellepl/spiffs)<br />
Open source development kit for NodeMCU [nodemcu-devkit](https://github.com/nodemcu/nodemcu-devkit)<br />
Flash tool for NodeMCU [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher)<br />

wiki: [nodemcu wiki](https://github.com/nodemcu/nodemcu-firmware/wiki)<br />
api: [nodemcu api](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_en)<br />
home: [nodemcu.com](http://www.nodemcu.com)<br />
bbs: [Chinese bbs](http://bbs.nodemcu.com)<br />
Tencent QQ group: 309957875<br />

# Summary
- Easy to access wireless router
- Based on Lua 5.1.4 (without *io, math, debug, os* module.)
- Event-Drive programming preferred.
- Build-in file, timer, pwm, i2c, spi, 1-wire, net, mqtt, gpio, wifi, adc, uart and system api.
- GPIO pin re-mapped, use the index to access gpio, i2c, pwm.

# To Do List (pull requests are very welcomed)
- fix wifi smart connect
- add spi module (done)
- add mqtt module (done)
- add coap module
- cross compiler

# Change log
2015-01-27<br />
support floating point LUA.<br />
use macro LUA_NUMBER_INTEGRAL in user_config.h control this feature.<br />
LUA_NUMBER_INTEGRAL to disable floating point support,<br />
// LUA_NUMBER_INTEGRAL to enable floating point support.<br />
fix tmr.time(). #132<br />
fix filesystem length. #113<br />
fix ssl reboots. #134<br />
build pre_build bin.

2015-01-26<br />
applied sdk095_patch1 to sdk 0.9.5.<br />
added LUA examples and modules [by dvv](https://github.com/dvv). <br />
added node.readvdd33() API [by alonewolfx2](https://github.com/alonewolfx2).<br />
build pre_build bin.

2015-01-24<br />
migrate to sdk 0.9.5 release.<br />
tmr.time() now return second(not precise yet). <br />
build pre_build bin.

2015-01-23<br />
merge mqtt branch to master.<br />
build pre_build bin.

2015-01-18<br />
merge mqtt module to [new branch mqtt](https://github.com/nodemcu/nodemcu-firmware/tree/mqtt) from [https://github.com/tuanpmt/esp_mqtt](https://github.com/tuanpmt/esp_mqtt).<br />
merge spi module from iabdalkader:spi. <br />
fix #110,set local port to random in client mode.<br />
modify gpio.read to NOT set pin to input mode automatic.<br />
add PATH env with C:\MinGW\bin;C:\MinGW\msys\1.0\bin;C:\Python27 in eclipse project. resolve #103.

2015-01-08<br />
fix net.socket:send() issue when multi sends are called. <br />
*NOTE*: if data length is bigger than 1460, send next packet AFTER "sent" callback is called.<br />
fix file.read() api, take 0xFF as a regular byte, not EOF.<br />
pre_build/latest/nodemcu_512k_latest.bin is removed. use pre_build/latest/nodemcu_latest.bin instead.

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
    <td>7</td><td>GPIO13</td<td></td><td></td>
   </tr>
</table>
#### [*] D0(GPIO16) can only be used as gpio read/write. no interrupt supported. no pwm/i2c/ow supported.

#Build option
####file ./app/include/user_config.h
```c
// #define FLASH_512K
// #define FLASH_1M
// #define FLASH_2M
// #define FLASH_4M
#define FLASH_AUTOSIZE
...
#define LUA_USE_MODULES
#ifdef LUA_USE_MODULES
#define LUA_USE_MODULES_NODE
#define LUA_USE_MODULES_FILE
#define LUA_USE_MODULES_GPIO
#define LUA_USE_MODULES_WIFI
#define LUA_USE_MODULES_NET
#define LUA_USE_MODULES_PWM
#define LUA_USE_MODULES_I2C
#define LUA_USE_MODULES_TMR
#define LUA_USE_MODULES_ADC
#define LUA_USE_MODULES_UART
#define LUA_USE_MODULES_OW
#define LUA_USE_MODULES_BIT
#endif /* LUA_USE_MODULES */
...
// LUA_NUMBER_INTEGRAL
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
