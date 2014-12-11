# **NodeMcu** #
###A lua based firmware for wifi-soc esp8266
version 0.9.2 build 2014-12-11
# Change log
2014-12-11<br />
fix uart.setup(), now setup can set pins in other mode back to uart mode. <br />
add wifi.sta.status() api, to get connection status in station mode.

[more change log](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_en#change_log)<br />
[更多变更日志](https://github.com/nodemcu/nodemcu-firmware/wiki/nodemcu_api_cn#change_log)
# Summary
- Easy to access wireless router
- Based on Lua 5.1.4
- Event-Drive programming preferred.
- Build-in file, timer, pwm, i2c, 1-wire, net, gpio, wifi, adc, uart and system api.
- GPIO pin re-mapped, use the index to access gpio, i2c, pwm.
- GPIO Map Table:

<table>
  <tr>
    <th scope="col">IO index</th><th scope="col">ESP8266 pin</th><th scope="col">IO index</th><th scope="col">ESP8266 pin</th>
  </tr>
  <tr>
    <td>0</td><td>GPIO12</td><td>8</td><td>GPIO0</td>
  </tr>
  <tr>
    <td>1</td><td>GPIO13</td><td>9</td><td>GPIO2</td>
   </tr>
   <tr>
    <td>2</td><td>GPIO14</td><td>10</td><td>GPIO4</td>
  </tr>
  <tr>
    <td>3</td><td>GPIO15</td><td>11</td><td>GPIO5</td>
   </tr>
   <tr>
    <td>4</td><td>GPIO3</td><td></td><td></td>
  </tr>
  <tr>
    <td>5</td><td>GPIO1</td><td></td><td></td>
   </tr>
   <tr>
    <td>6</td><td>GPIO9</td><td></td><td></td>
  </tr>
  <tr>
    <td>7</td><td>GPIO10</td<td></td><td></td>
   </tr>
</table>


#Flash the firmware
nodemcu_512k.bin: 0x00000<br />
for most esp8266 modules, just pull GPIO0 down and restart.<br />
You can use the [nodemcu-flasher](https://github.com/nodemcu/nodemcu-flasher) to burn the firmware.

#Connect the hardware in serial
braudrate:9600

#Start play

####Connect to your ap

```lua
    print(wifi.sta.getip())
    --0.0.0.0
    wifi.setmode(wifi.STATION)
    wifi.sta.config("SSID","password")
    print(wifi.sta.getip())
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

####Do something shining
```lua
  function led(r,g,b) 
    pwm.setduty(0,r) 
    pwm.setduty(1,g) 
    pwm.setduty(2,b) 
  end
  pwm.setup(0,500,512) 
  pwm.setup(1,500,512) 
  pwm.setup(2,500,512)
  pwm.start(0) 
  pwm.start(1) 
  pwm.start(2)
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
    package.loaded["ds18b20"]=nil   
```

#Check this out
Tencent QQ group: 309957875<br/>
[nodemcu wiki](https://github.com/nodemcu/nodemcu-firmware/wiki)<br/>
[nodemcu.com](http://www.nodemcu.com)
