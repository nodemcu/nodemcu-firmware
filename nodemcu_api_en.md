# **nodeMcu API Instruction** #
[中文版本](https://github.com/funshine/nodemcu-firmware/wiki/nodemcu_api_cn)
###version 0.9.2 build 2014-11-20
<a id="change_log"></a>
###change log: 
2014-11-20<br />
fix tmr.delay to support more than 2s delay, may cause bacon time out, lost connection to AP.<br />
add tmr.wdclr to clear watchdog counter in chip, use in long time loop.<br />
fix UDP part of net module.<br />
add a timeout para to createServer(net.TCP, timeout).

2014-11-19<br />
add adc module, use adc.read(0) to read adc value, no tests made.<br />
add wifi.sta.getap() api to wifi.sta module, to get ap list.

2014-11-18<br />
bug fixed: net.socket:connect() has no effect.

2014-11-18<br />
bug fixed: as a tcp server, the connection can't closed with :close().<br />
tcp server: inactive connection will closed by server in 30s (previously 180s).<br />
add a test api node.input() to put lua chunk into lua interpretor, multi-line supported.<br />
add a test api node.ouput(function) to direct serial output to a callback function.<br />
file.readline() now returns line include EOL('\n'), and returns nil when EOF. 

2014-11-12<br />
full version firmware<br />

2014-11-11<br />
add file.seek() api to file module<br />
now max 6 pwm channel is supported<br />

2014-11-10<br />
change log module to file module<br />
now file operation support multiple read/write<br />
for now file module only allowed one file opened<br />

2014-11-5<br />
delete log operation api from node module<br />
add log module<br />
modify wifi module api<br />
modify node.key long_press and short_press default function<br />
key is triged only when key is released<br />


# Summary
- Easy to access wireless router
- Based on Lua 5.1.4, Developers are supposed to have experience with Lua Program language.
- Event-Drive programming modal.
- Build-in file, timer, pwm, i2c, net, gpio, wifi module.
- Serial Port BaudRate:9600
- Re-mapped GPIO pin, use the index to program gpio, i2c, pwm.
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


#Burn/Flash Firmware
###Address

nodemcu_512k.bin: 0x00000<br />

#node module
<a id="nm_restart"></a>
## node.restart()
####Description
restart the chip.

####Syntax

node.restart()

####Parameters
nil

####Returns
nil

####Example
   
```lua
    node.restart();
```

####See also
**-**   []()

<a id="nm_dsleep"></a>
## node.dsleep()
####Description

Enter deep sleep mode, wake up when timed out<br />

####Syntax

node.dsleep(us)<br />
**-Note:** This function can only be used in the condition that esp8266 PIN32(RST) and PIN8(XPD_DCDC) are connected together.

####Parameters
us: sleep time in micro second

####Returns
nil

####Example

```lua
    node.dsleep(us);
```

####See also
**-**   []()

<a id="nm_chipid"></a>
## node.chipid()
####Description
return chip ID

####Syntax
node.chipid()

####Parameters
nil

####Returns
number:chip ID

####Example

```lua
    id = node.chipid();
```

####See also
**-**   []()

<a id="nm_heap"></a>
## node.heap()
####Description
return the remain HEAP size in bytes

####Syntax
node.heap()

####Parameters
nil

####Returns
number: system heap size left in bytes

####Example

```lua
    heap_size = node.heap();
```

####See also
**-**   []()

<a id="nm_key"></a>
## node.key()
####Description
define button function, button is connected to GPIO16.

####Syntax
node.key(type, function())

####Parameters
type: type is either string "long" or "short". long: press the key for 3 seconds, short: press shortly(less than 3 seconds)<br />
function(): user defined function which is called when key is pressed. If nil, cancling the user defined function.<br />
Default function: long: change LED blinking rate,  short: reset chip

####Returns
nil

####Example    
```lua
    node.key("long", function(){print('hello world')})
```

####See also
**-**   []()

<a id="nm_led"></a>
## node.led()
####Description
setup the on/off time for led, which connected to GPIO16, multiplexing with node.key()

####Syntax
node.led(low, high)

####Parameters
Low: LED off time, LED keeps on when low=0. Unit: milliseconds, time resolution: 80~100ms<br />
High: LED off time. Unit: milliseconds, time resolution: 80~100ms

####Returns
nil

####Example

```lua
    -- turn led on forever.
    node.led(0);
```

####See also
**-**   []()

<a id="nm_input"></a>
## node.input()
####Description
accept a string and put the string into Lua interpretor.<br />
same as pcall(loadstring(str)) but support multi seperated line.

####Syntax
node.input(str)

####Parameters
str: Lua chunk

####Returns
nil

####Example

```lua
    -- never use node.input() in console. no effect.
    sk:on("receive", function(conn, payload) node.input(payload) end)
```

####See also
**-**   []()

<a id="nm_output"></a>
## node.output()
####Description
direct output from lua interpretor to a call back function.

####Syntax
node.output(function(str), serial_debug)

####Parameters
function(str): a function accept every output as str, and can send the output to a socket.<br />
serial_debug: 1 output also show in serial. 0: no serial output.

####Returns
nil

####Example

```lua
    function tonet(str)
      sk:send(str)
      -- print(str) WRONG!!! never ever print something in this function
      -- because this will cause a recursive function call!!!
    end
    node.ouput(tonet, 1)  -- serial also get the lua output.
```

```lua
    -- a simple telnet server
    s=net.createServer(net.TCP) 
    s:listen(2323,function(c) 
       con_std = c 
       function s_output(str) 
          if(con_std~=nil) 
             then con_std:send(str) 
          end 
       end 
       node.output(s_output, 0)   -- re-direct output to function s_ouput.
       c:on("receive",function(c,l) 
          node.input(l)           -- works like pcall(loadstring(l)) but support multiple separate line
       end) 
       c:on("disconnection",function(c) 
          con_std = nil 
          node.output(nil)        -- un-regist the redirect output function, output goes to serial
       end) 
    end)
```

####See also
**-**   []()

#file module
<a id="fl_remove"></a>
## file.remove()
####Description
remove file from file system.

####Syntax
file.remove(filename)

####Parameters
filename: file to remove

####Returns
nil

####Example

```lua
    -- remove "foo.lua" from file system.
    file.remove("foo.lua")
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.close()](#fl_close)

<a id="fl_open"></a>
## file.open()
####Description
open file.

####Syntax
file.open(filename, mode)

####Parameters
filename: file to be opened, directories are not supported<br />
mode:<br />
   "r": read mode (the default)<br />
   "w": write mode<br />
   "a": append mode<br />
   "r+": update mode, all previous data is preserved<br />
   "w+": update mode, all previous data is erased<br />
   "a+": append update mode, previous data is preserved, writing is only allowed at the end of file

####Returns
nil

####Example

```lua
    -- open 'init.lua', print the first line.
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####See also
**-**   [file.close()](#fl_close)<br />
**-**   [file.readline()](#fl_readline)

<a id="fl_close"></a>
## file.close()
####Description
close the file.

####Syntax
file.close()

####Parameters
nil

####Returns
nil

####Example

```lua
    -- open 'init.lua', print the first line.
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.readline()](#fl_readline)

<a id="fl_readline"></a>
## file.readline()
####Description
read one line of file which is opened before.

####Syntax
file.readline()

####Parameters
nil

####Returns
file content in string, line by line, include EOL('\n')<br />
return nil when EOF.

####Example

```lua
    -- print the first line of 'init.lua'
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.close()](#fl_close)

<a id="fl_writeline"></a>
## file.writeline()
####Description
write string to file and add a '\n' at the end.

####Syntax
file.writeline(string)

####Parameters
string: content to be write to file

####Returns
true: write ok.
nil: there is error

####Example

```lua
    -- open 'init.lua' in 'a+' mode
    file.open("init.lua", "a+")
    -- write 'foo bar' to the end of the file
    file.writeline('foo bar')
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.write()](#fl_write)

<a id="fl_write"></a>
## file.write()
####Description
write string to file.

####Syntax
file.write(string)

####Parameters
string: content to be write to file.

####Returns
true: write ok.
nil: there is error

####Example

```lua
    -- open 'init.lua' in 'a+' mode
    file.open("init.lua", "a+")
    -- write 'foo bar' to the end of the file
    file.write('foo bar')
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_flush"></a>
## file.flush()
####Description
flush to file.

####Syntax
file.flush()

####Parameters
nil

####Returns
nil

####Example

```lua
    -- open 'init.lua' in 'a+' mode
    file.open("init.lua", "a+")
    -- write 'foo bar' to the end of the file
    file.write('foo bar')
    file.flush()
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_seek"></a>
## file.seek()
####Description
Sets and gets the file position, measured from the beginning of the file, to the position given by offset plus a base specified by the string whence.

####Syntax
file.seek(whence, offset)

####Parameters
whence:<br />
"set": base is position 0 (beginning of the file);<br />
"cur": base is current position;(default value)<br />
"end": base is end of file;<br />
offset: default 0

####Returns
success: returns the final file position<br />
fail: returns nil

####Example

```lua
    -- open 'init.lua' in 'a+' mode
    file.open("init.lua", "a+")
    -- write 'foo bar' to the end of the file
    file.write('foo bar')
    file.flush()
    file.seek("set")
    print(file.readline())
    file.close()
```

####See also
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_list"></a>
## file.list()
####Description
list all files.

####Syntax
file.list()

####Parameters
nil

####Returns
a lua table which contains the {file name: file size} pairs

####Example

```lua
    l = file.list();
    for k,v in l do
      print("name:"..k..", size:"..v)
    end
```

####See also
**-**   [file.remove()](#fl_remove)

#wifi module
##CONSTANT
wifi.STATION, wifi.SOFTAP, wifi.STATIONAP

<a id="wf_setmode"></a>
## wifi.setmode(mode)
####Description
setup wifi operation mode.

####Syntax
wifi.setmode(mode)

####Parameters
mode: value should be: wifi.STATION, wifi.SOFTAP or wifi.STATIONAP

####Returns
current mode after setup

####Example

```lua
    wifi.setmode(wifi.STATION)
```

####See also
**-**   [wifi.getmode()](#wf_getmode)


<a id="wf_getmode"></a>
## wifi.getmode(mode)
####Description
get wifi operation mode.

####Syntax
wifi.getmode()

####Parameters
nil

####Returns
wifi operation mode

####Example

```lua
    print(wifi.getmode())
```

####See also
**-**   [wifi.setmode()](#wf_setmode)


<a id="wf_startsmart"></a>
## wifi.startsmart()
####Description
starts to auto configuration, if success set up ssid and pwd automatically .

####Syntax
wifi.startsmart(channel, function succeed_callback())

####Parameters

channel: 1~13, startup channel for searching, if nil, default to 6.  20 seconds for each channel.<br />
succeed_callback: callback function called after configuration, which is called when got password and connected to AP.

####Returns
nil

####Example

```lua
    wifi.startsmart(6, cb())
```

####See also
**-**   [wifi.stopsmart()](#wf_stopsmart)


<a id="wf_stopsmart"></a>
## wifi.stopsmart()
####Description
stop the configuring process.

####Syntax
wifi.stopsmart()

####Parameters
nil

####Returns
nil

####Example

```lua
    wifi.stopsmart()
```

####See also
**-**   [wifi.startsmart()](#wf_startsmart)


#wifi.sta module

<a id="ws_config"></a>
## wifi.sta.config()
####Description
set ssid and password in station mode.

####Syntax
wifi.sta.config(ssid, password)

####Parameters

ssid: string which is less than 32 bytes.<br />
password: string which is less than 64 bytes.

####Returns
nil

####Example

```lua
    wifi.sta.config("myssid","mypassword")
```

####See also
**-**   [wifi.sta.connect()](#ws_connect)<br />
**-**   [wifi.sta.disconnect()](#ws_disconnect)


<a id="ws_connect"></a>
## wifi.sta.connect()
####Description
connect to AP in station mode.

####Syntax
wifi.sta.connect()

####Parameters
nil


####Returns
nil

####Example

```lua
    wifi.sta.connect()
```

####See also
**-**   [wifi.sta.disconnect()](#ws_disconnect)<br />
**-**   [wifi.sta.config()](#ws_config)


<a id="ws_disconnect"></a>
## wifi.sta.disconnect()
####Description
disconnect from AP in station mode.

####Syntax
wifi.sta.disconnect()

####Parameters
nil


####Returns
nil

####Example

```lua
    wifi.sta.disconnect()
```

####See also
**-**   [wifi.sta.config()](#ws_config)<br />
**-**   [wifi.sta.connect()](#ws_connect)


<a id="ws_autoconnect"></a>
## wifi.sta.autoconnect()
####Description
auto connect to AP in station mode.

####Syntax
wifi.sta.autoconnect(auto)

####Parameters
auto: 0 to disable auto connecting. 1 to enable auto connecting


####Returns
nil

####Example

```lua
    wifi.sta.autoconnect()
```

####See also
**-**   [wifi.sta.config()](#ws_config)<br />
**-**   [wifi.sta.connect()](#ws_connect)<br />
**-**   [wifi.sta.disconnect()](#ws_disconnect)


<a id="ws_getip"></a>
## wifi.sta.getip()
####Description
get ip address in station mode.

####Syntax
wifi.sta.getip()

####Parameters
nil


####Returns
ip address in string, for example:"192.168.0.111"

####Example

```lua
    -- print current ip
    print(wifi.sta.getip())
```

####See also
**-**   [wifi.sta.getmac()](#ws_getmac)


<a id="ws_getmac"></a>
## wifi.sta.getmac()
####Description
get mac address in station mode.

####Syntax
wifi.sta.getmac()

####Parameters
nil


####Returns
mac address in string, for example:"18-33-44-FE-55-BB"

####Example

```lua
    -- print current mac address
    print(wifi.sta.getmac())
```

####See also
**-**   [wifi.sta.getip()](#ws_getip)

<a id="ws_getap"></a>
## wifi.sta.getap()
####Description
scan and get ap list as a lua table into callback function.

####Syntax
wifi.sta.getap(function(table))

####Parameters
function(table): a callback function to receive ap table when scan is done<br />
    this function receive a table, the key is the ssid, value is other info in format: authmode,rssi,bssid,channel


####Returns
nil

####Example

```lua
    -- print ap list
    function listap(t)
      for k,v in pairs(t) do
        print(k.." : "..v)
      end
    end
    wifi.sta.getap(listap)
```

####See also
**-**   [wifi.sta.getip()](#ws_getip)

#wifi.ap module

<a id="wa_config"></a>
## wifi.ap.config()
####Description
set ssid and password in ap mode.

####Syntax
wifi.ap.config(cfg)

####Parameters
cfg: lua table to setup ap.

####Example:

```lua
     cfg={}
     cfg.ssid="myssid"
     cfg.pwd="mypwd"
     wifi.ap.setconfig(cfg)
```

####Returns
nil

####Example

```lua
    wifi.ap.config(ssid, 'password')
```

####See also
**-**    []()

<a id="wa_getip"></a>
## wifi.ap.getip()
####Description
get ip in ap mode.

####Syntax
wifi.ap.getip()

####Parameters
nil

####Returns
ip address in string, for example:"192.168.0.111"

####Example

```lua
    wifi.ap.getip()
```

####See also
**-**   [wifi.ap.getmac()](#wa_getmac)


<a id="wa_getmac"></a>
## wifi.ap.getmac()
####Description
get mac address in ap mode.

####Syntax
wifi.ap.getmac()

####Parameters
nil

####Returns
mac address in string, for example:"1A-33-44-FE-55-BB"

####Example

```lua
    wifi.ap.getmac()
```

####See also
**-**   [wifi.ap.getip()](#wa_getip)


#timer module
<a id="tm_delay"></a>
## tmr.delay()
####Description
delay us micro seconds.

####Syntax
tmr.dealy(us)

####Parameters
us: delay time in micro second

####Returns
nil

####Example

```lua
    -- delay 100us
    tmr.delay(100)
```

####See also
**-**   [tmr.now()](#tm_now)


<a id="tm_now"></a>
## tmr.now()
####Description
return the current value of system counter: uint32, us.

####Syntax
tmr.now()

####Parameters
nil

####Returns
uint32: value of counter

####Example

```lua
    -- print current value of counter
    print(tmr.now())
```

####See also
**-**   [tmr.delay()](#tm_delay)


<a id="tm_alarm"></a>
## tmr.alarm()
####Description
alarm time.

####Syntax
tmr.alarm(interval, repeat, function do())

####Parameters
Interval: alarm time, unit: millisecond<br />
repeat: 0 - one time alarm, 1 - repeat<br />
function do(): callback function for alarm timed out

####Returns
nil

####Example

```lua
    -- print "hello world" every 1000ms
    tmr.alarm(1000, 1, function() print("hello world") end )
```

####See also
**-**   [tmr.now()](#tm_now)


<a id="tm_stop"></a>
## tmr.stop()
####Description

stop alarm.<br />
**-Note:** only one alarm is allowed, the previous one would be replaced if tmr.alarm() called again before tmr.stop().

####Syntax
tmr.stop()

####Parameters
nil.

####Returns
nil

####Example

```lua
    -- print "hello world" every 1000ms
    tmr.alarm(1000, 1, function() print("hello world") end )

    -- something else

    -- stop alarm
    tmr.stop()
```

####See also
**-**   [tmr.now()](#tm_now)


<a id="tm_wdclr"></a>
## tmr.wdclr()
####Description
clear system watchdog counter.<br />

####Syntax
tmr.wdclr()

####Parameters
nil.

####Returns
nil

####Example

```lua
    for i=1,10000 do 
      print(i)
      tmr.wdclr()   -- should call tmr.wdclr() in a long loop to avoid hardware reset caused by watchdog.
    end 
```

####See also
**-**   [tmr.delay()](#tm_delay)

#GPIO module
##CONSTANT
gpio.OUTPUT, gpio.INPUT, gpio.INT, gpio.HIGH, gpio.LOW


<a id="io_mode"></a>
## gpio.mode()
####Description
initialize pin to GPIO mode, set the pin in/out mode.

####Syntax
gpio.mode(pin, mode)

####Parameters
pin: 0~11, IO index<br />
mode: gpio.OUTPUT or gpio.INPUT, or gpio.INT(interrupt mode)

####Returns
nil

####Example

```lua
    -- set gpio 0 as output.
    gpio.mode(0, gpio.OUTPUT)

```

####See also
**-**   [gpio.read()](#io_read)


<a id="io_read"></a>
## gpio.read()
####Description
read pin value.

####Syntax
gpio.read(pin)

####Parameters
pin: 0~11, IO index

####Returns
number:0 - low, 1 - high

####Example

```lua
    -- read value of gpio 0.
    gpio.read(0)
```

####See also
**-**   [gpio.mode()](#io_mode)


<a id="io_write"></a>
## gpio.write()
####Description
set pin value.

####Syntax
gpio.write(pin)

####Parameters
pin: 0~11, IO index<br />
level: gpio.HIGH or gpio.LOW

####Returns
nil

####Example

```lua
    -- set pin index 1 to GPIO mode, and set the pin to high.
    pin=1
    gpio.mode(pin, gpio.OUTPUT)
    gpio.write(pin, gpio.HIGH)
```

####See also
**-**   [gpio.mode()](#io_mode)<br />
**-**   [gpio.read()](#io_read)


<a id="io_trig"></a>
## gpio.trig()
####Description
set the interrupt callback function for pin.

####Syntax
gpio.trig(pin, type, function(level))

####Parameters
pin: 0~11, IO index<br />
type: "up", "down", "both", "low", "high", which represent rising edge, falling edge, both edge, low level, high level trig mode separately.<br />
function(level): callback function when triggered. The gpio level is the param. Use previous callback function if undefined here.

####Returns
nil

####Example

```lua
    -- use pin 0 as the input pulse width counter
    pulse0 = 0
    du = 0
    gpio.mode(0,gpio.INT)
    function pin0cb(level)
     du = tmr.now() – pulse0
     print(du)
     pulse0 = tmr.now()
     if level == 1 then gpio.trig(0, "down ") else gpio.trig(0, "up ") end
    end
    gpio.trig(0, "down ",pin0cb)

```

####See also
**-**   [gpio.mode()](#io_mode)<br />
**-**   [gpio.write()](#io_write)


#PWM module
<a id="pw_setup"></a>
## pwm.setup()
####Description
set pin to PWM mode. Only 3 pins can be set to PWM mode at the most.

####Syntax
pwm.setup(pin, clock, duty)

####Parameters
pin: 0~11, IO index<br />
clock: 1~500, pwm frequency<br />
duty: 0~100, pwm duty cycle in percentage

####Returns
nil

####Example

```lua
    -- set pin index 0 as pwm output, frequency is 100Hz, duty cycle is 50-50.
    pwm.setup(0, 100, 50)
```

####See also
**-**   [pwm.start()](#pw_start)


<a id="pw_close"></a>
## pwm.close()
####Description
quit PWM mode for specified pin.

####Syntax
pwm.close(pin)

####Parameters
pin: 0~11, IO index

####Returns
nil

####Example

```lua
    pwm.close(0)
```

####See also
**-**   [pwm.start()](#pw_start)


<a id="pw_start"></a>
## pwm.start()
####Description
pwm starts, you can detect the waveform on the gpio.

####Syntax
pwm.start(pin)

####Parameters
pin: 0~11, IO index

####Returns
nil

####Example

```lua
    pwm.start(0)
```

####See also
**-**   [pwm.stop()](#pw_stop)


<a id="pw_stop"></a>
## pwm.stop()
####Description
pause the output of PWM waveform.

####Syntax
pwm.stop(pin)

####Parameters
pin: 0~11, IO index

####Returns
nil

####Example

```lua
    pwm.stop(0)
```

####See also
**-**   [pwm.start()](#pw_start)


<a id="pw_setclock"></a>
## pwm.setclock()
####Description

set pwm frequency for pin.<br />
**-Note:** setup pwm frequency will synchronously change others if there are any. Only one PWM frequency can be allowed for the system.

####Syntax
pwm.setclock(pin, clock)

####Parameters
pin: 0~11, IO index.<br />
clock: 1~500, pwm frequency.

####Returns
nil

####Example

```lua
    pwm.setclock(0, 100)
```

####See also
**-**   [pwm.getclock()](#pw_getclock)


<a id="pw_getclock"></a>
## pwm.getclock()
####Description
get pwm frequency of pin.

####Syntax
pwm.getclock(pin)

####Parameters
pin: 0~11, IO index.

####Returns
number:pwm frequency of pin

####Example

```lua
    print(pwm.getclock(0))
```

####See also
**-**   [pwm.setclock()](#pw_setclock)


<a id="pw_setduty"></a>
## pwm.setduty()
####Description
set duty clycle for pin.

####Syntax
pwm.setduty(pin, duty)

####Parameters
pin: 0~11, IO index<br />
duty: 0~100, pwm duty cycle in percentage

####Returns
nil

####Example

```lua
    pwm.setduty(0, 50)
```

####See also
**-**   [pwm.getduty()](#pw_getduty)


<a id="pw_getduty"></a>
## pwm.getduty()
####Description
get duty clycle for pin.

####Syntax
pwm.getduty(pin)

####Parameters
pin: 0~11, IO index

####Returns
nil

####Example

```lua
    -- D0 is connected to green led
    -- D1 is connected to blue led
    -- D2 is connected to red led
    pwm.setup(0,500,50)
    pwm.setup(1,500,50)
    pwm.setup(2,500,50)
    pwm.start(0)
    pwm.start(1)
    pwm.start(2)
    function led(r,g,b)
      pwm.setduty(0,g)
      pwm.setduty(1,b)
      pwm.setduty(2,r)
    end
    led(50,0,0) --  set led to red
    led(0,0,50) -- set led to blue.

```

####See also
**-**   [pwm.setduty()](#pw_setduty)


#net module
##CONSTANT
net.TCP, net.UDP

<a id="nt_createServer"></a>
## net.createServer()
####Description
create a server.

####Syntax
net.createServer(type, timeout)

####Parameters
type: net.TCP or net.UDP<br />
timeout: for a TCP server, timeout is 1~28800 seconds, for a inactive client to disconnected.

####Returns
net.server sub module

####Example

```lua
    net.createServer(net.TCP, 30) -- 30s timeout
```

####See also
**-**   [net.createConnection()](#nt_createConnection)


<a id="nt_createConnection"></a>
## net.createConnection()
####Description
create a client.

####Syntax
net.createConnection(type, secure)

####Parameters
type: net.TCP or net.UDP<br />
secure: 1 or 0, 1 for ssl link, 0 for normal link

####Returns
net.server sub module

####Example

```lua
    net.createConnection(net.UDP, 0)
```

####See also
**-**   [net.createServer()](#nt_createServer)


#net.server module
<a id="ns_listen"></a>
## listen()
####Description
listen on port from [ip] address.

####Syntax
net.server.listen(port,[ip],function(net.socket))

####Parameters
port: port number<br />
ip:ip address string, can be omitted<br />
function(net.socket): callback function, pass to Caller function as param if a connection is created successfully

####Returns
nil

####Example

```lua
    -- create a server
    sv=net.createServer(net.TCP, 30)    -- 30s time out for a inactive client
    -- server listen on 80, if data received, print data to console, and send "hello world" to remote.
    sv:listen(80,function(c)
      c:on("receive", function(sck, pl) print(pl) end)
      c:send("hello world")
      end)
```

####See also
**-**   [net.createServer()](#nt_createServer)


<a id="ns_close"></a>
## close()
####Description
close server.

####Syntax
net.server.close()

####Parameters
nil

####Returns
nil

####Example

```lua
    -- create a server
    sv=net.createServer(net.TCP, 30)
    -- close server
    sv:close()
```

####See also
**-**   [net.createServer()](#nt_createServer)


#net.socket module
<a id="nk_connect"></a>
## connect()
####Description
connect to remote.

####Syntax
connect(port, ip)

####Parameters
port: port number<br />
ip: ip address in string

####Returns
nil

####See also
**-**   [net.socket:on()](#nk_on)


<a id="nk_send"></a>
## send()
####Description
send data to remote via connection.

####Syntax
send(string, function(sent))

####Parameters
string: data in string which will be sent to remote<br />
function(sent): callback function for sending string

####Returns
nil

####See also
**-**   [net.socket:on()](#nk_on)


<a id="nk_on"></a>
## on()
####Description
register callback function for event.

####Syntax
on(event, function cb())

####Parameters
event: string, which can be: "connection", "reconnection", "disconnection", "receive", "sent"<br />
function cb(net.socket, [string]): callback function. The first param is the socket.<br />
If  event is"receive",  the second param is received data in string.

####Returns
nil

####Example

```lua
    sk=net.createConnection(net.TCP, 0)
    sk:on("receive", function(sck, c) print(c) end )
    sk:connect(80,"192.168.0.66")
    sk:send("GET / HTTP/1.1\r\nHost: 192.168.0.66\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")
```

####See also
**-**   [net.createServer()](#nt_createServer)


<a id="nk_close"></a>
## close()
####Description
close socket.

####Syntax
close()

####Parameters
nil

####Returns
nil

####See also
**-**   [net.createServer()](#nt_createServer)


<a id="nk_dns"></a>
## dns()
####Description
get domain ip

####Syntax
dns(domain, function(net.socket, ip))

####Parameters
domain: domain name.<br />
function (net.socket, ip): callback function. The first param is the socket, the second param is the ip address in string.

####Returns
nil

####See also
**-**   [net.createServer()](#nt_createServer)


#i2c module
##CONSTANT
i2c.SLOW,  i2c.TRANSMITTER, i2c. RECEIVER.  FAST（400k）is not supported for now.

<a id="ic_setup"></a>
## i2c.setup()
####Description
initialize i2c.

####Syntax
i2c.setup(id, pinSDA, pinSCL, speed)

####Parameters
id = 0<br />
pinSDA: 0~11, IO index<br />
pinSCL: 0~11, IO index<br />
speed:  i2c.SLOW

####Returns
nil

####See also
**-**   [i2c.read()](#ic_read)


<a id="ic_start"></a>
## i2c.start()
####Description
start i2c transporting.

####Syntax
i2c.start(id)

####Parameters
id = 0

####Returns
nil

####See also
**-**   [i2c.read()](#ic_read)


<a id="ic_stop"></a>
## i2c.stop()
####Description
stop i2c transporting.

####Syntax
i2c.stop(id)

####Parameters
id = 0

####Returns
nil

####See also
**-**   [i2c.read()](#ic_read)


<a id="ic_address"></a>
## i2c.address()
####Description
setup i2c address and read/write mode.

####Syntax
i2c.address(id, device_addr, direction)

####Parameters
id=0<br />
device_addr: device address.<br />
direction: i2c.TRANSMITTER for writing mode , i2c. RECEIVER for reading mode

####Returns
nil

####See also
**-**   [i2c.read()](#ic_read)

<a id="ic_write"></a>
## i2c.write()
####Description
write data to i2c, data can be multi numbers, string or lua table.

####Syntax
i2c.write(id, data1, data2,...)

####Parameters
id=0<br />
data: data can be numbers, string or lua table.

####Returns
nil

####Example

```lua
    i2c.write(0, "hello", "world")
```

####See also
**-**   [i2c.read()](#ic_read)


<a id="ic_read"></a>
## i2c.read()
####Description
read data for len bytes.

####Syntax
i2c.read(id, len)

####Parameters
id=0<br />
len: data length

####Returns
string:data received.

####Example

```lua
    id=0
    sda=1
    scl=0

    -- initialize i2c, set pin1 as sda, set pin0 as scl
    i2c.setup(id,sda,scl,i2c.SLOW)

    -- user defined function: read from reg_addr content of dev_addr
    function read_reg(dev_addr, reg_addr)
      i2c.start(id)
      i2c.address(id, dev_addr ,i2c.TRANSMITTER)
      i2c.write(id,reg_addr)
      i2c.stop(id)
      i2c.start(id)
      i2c.address(id, dev_addr,i2c.RECEIVER)
      c=i2c.read(id,1)
      i2c.stop(id)
      return c
    end

    -- get content of register 0xAA of device 0x77
    reg = read_reg(0x77, 0xAA)
    pirnt(string.byte(reg))

```

####See also
**-**   [i2c.write()](#ic_write)

#adc module
##CONSTANT
none

<a id="adc_read"></a>
## adc.read()
####Description
read adc value of id, esp8266 has only one 10bit adc, id=0, pin TOUT

####Syntax
adc.read(id)

####Parameters
id = 0<br />

####Returns
adc value

####See also
**-**   []()
