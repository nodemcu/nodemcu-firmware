# **nodeMcu API说明** #
[English Version](https://github.com/funshine/nodemcu-firmware/wiki/nodemcu_api_en)
###版本 0.9.2 build 2014-11-20
<a id="change_log"></a>
###变更日志: 
2014-11-20<br />
修正tmr.delay，支持2s以上的延时，但是长延时可能会引起beacon timer out，导致与AP之间重新连接。<br />
增加tmr.wdclr()，用来重置看门狗计数器，用在长循环中，以防芯片因看门狗重启。<br />
修正net模块UDP无法连接问题。<br />
createServer(net.TCP, timeout)函数增加连接超时参数设置。

2014-11-19<br />
增加adc模块，adc.read(0)读取adc的值。<br />
wifi模块增加wifi.sta.getap() 函数，用于获取ap列表。

2014-11-18<br />
修正tcp服务器不能使用:close()函数关闭tcp连接的问题。<br />
tcp服务器: 服务器将关闭30s内未使用的闲置的连接。（修正前为180s）<br />
增加了函数node.input()用来向lua解释器输入lua代码段, 支持多行输入。<br />
增加了函数node.ouput(function)用来将串口输出重定向于回调函数。<br />
file.readline()函数返回值包含了EOL'\n', 当读到EOF时，返回nil。

2014-11-12<br />
全功能版本固件<br />

2014-11-11<br />
文件模块中增加了file.seek()函数。<br />
最多支持6个PWM输出。<br />

2014-11-10<br />
log模块更名为file模块<br />
文件操作支持多次读写。<br />
当前仅支持打开一个文件进行操作。<br />

2014-11-5<br />
node模块中删除了log函数。<br />
增加了log模块。<br />
修改wifi模块的函数。<br />
修改了node.key长按与短按的默认回调函数。<br />
只有当按钮被松开后，key才会被触发。<br />


###flash 错误
注意：有些模块在烧写之后启动，串口输出 ERROR in flash_read: r=。。。<br />
这是因为模块原来的flash内部没有擦除。<br />
可使用blank512k.bin，<br />
内容为全0xFF，从0x00000开始烧入。<br />
烧入之后可以正常运行。

# 概述
- 快速、自动连接无线路由器
- 基于Lua 5.1.4，使用者需了解最简单的Lua语法
- 采用事件驱动的编程模型
- 内置file, timer, pwm, i2c, net, gpio, wifi模块
- 串口波特率:9600-8N1
- 对模块的引脚进行编号；gpio，i2c，pwm等模块需要使用引脚编号进行索引
- 目前的编号对应表格:

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
	
#固件烧写
###地址
nodemcu_512k.bin: 0x00000<br />

#node模块
<a id="nm_restart"></a>
## node.restart()
####描述
重新启动

####语法

node.restart()

####参数
nil

####返回值
nil

####示例
   
```lua
    node.restart();
```

####参见
**-**   []()

<a id="nm_dsleep"></a>
## node.dsleep()
####描述

进入睡眠模式，计时时间之后唤醒<br />

####语法

node.dsleep(us)<br />
**-注意:** 如需使用此功能，需要将esp8266的PIN32(RST)和PIN8(XPD_DCDC)短接。

####参数
us: 睡眠时间，单位：us

####返回值
nil

####示例

```lua
    node.dsleep(us);
```

####参见
**-**   []()

<a id="nm_chipid"></a>
## node.chipid()
####描述
返回芯片ID

####语法
node.chipid()

####参数
nil

####返回值
number:芯片ID

####示例

```lua
    id = node.chipid();
```

####参见
**-**   []()

<a id="nm_heap"></a>
## node.heap()
####描述
返回当前系统剩余内存大小，单位：字节

####语法
node.heap()

####参数
nil

####返回值
number: 系统剩余内存字节数

####示例

```lua
    heap_size = node.heap();
```

####参见
**-**   []()

<a id="nm_key"></a>
## node.key()
####描述
定义按键的功能函数, 按键与GPIO16相连。

####语法
node.key(type, function())

####参数
type: type取字符串"long"或者"short". long:按下按键持续3s以上, short: 短按按键(时间短于3s)<br />
function(): 用户自定义的按键回调函数。 如果为nil, 则取消用户定义的回调函数。<br />
默认函数：long：改变LED闪烁频率，short：重新启动。

####返回值
nil

####示例    
```lua
    node.key("long", function(){print('hello world')})
```

####参见
**-**   []()

<a id="nm_led"></a>
## node.led()
####描述
设置LED的亮/暗时间, LED连接到GPIO16, 与node.key()复用。

####语法
node.led(low, high)

####参数
Low: LED关闭时间，如设置为0，则LED处于常亮状态。单位：毫秒，时间分辨率：80~100ms<br />
High: LED打开时间，单位：毫秒，时间分辨率：80~100ms

####返回值
nil

####示例

```lua
    -- LED常亮.
    node.led(0);
```

####参见
**-**   []()

<a id="nm_input"></a>
## node.input()
####描述
接收字符串并将字符串传入lua解释器。<br />
功能同pcall(loadstring(str))，增加了支持多行输入的功能。

####语法
node.input(str)

####参数
str: Lua代码段

####返回值
nil

####示例

```lua
    -- 注意：该函数不支持在命令行中使用。
    sk:on("receive", function(conn, payload) node.input(payload) end)
```

####参见
**-**   []()

<a id="nm_output"></a>
## node.output()
####描述
将lua解释器输出重定向于回调函数。

####语法
node.output(function(str), serial_debug)

####参数
function(str): 接收lua解释器输出的str作为输入，可以将该输出通过socket发送。<br />
serial_debug: 1：将输出送至串口； 0：输出不送至串口

####返回值
nil

####示例

```lua
    function tonet(str)
      sk:send(str)
      -- print(str) 错误!!! 千万不要在此函数中再使用print函数
      -- 因为这样会导致函数的嵌套调用！！
    end
    node.ouput(tonet, 1)  -- serial also get the lua output.
```

####参见
**-**   []()

#file 模块
<a id="fl_remove"></a>
## file.remove()
####描述
删除文件。

####语法
file.remove(filename)

####参数
filename: 需要删除的文件。

####返回值
nil

####示例

```lua
    -- 删除foo.lua文件
    file.remove("foo.lua")
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.close()](#fl_close)

<a id="fl_open"></a>
## file.open()
####描述
打开文件。

####语法
file.open(filename, mode)

####参数
filename: 需要打开的文件，不支持文件夹。<br />
mode:<br />
   "r": read mode (the default)<br />
   "w": write mode<br />
   "a": append mode<br />
   "r+": update mode, 文件内的数据保留<br />
   "w+": update mode, 文件内的数据清除<br />
   "a+": append update mode, 文件内的数据保留，要写入的数据仅能增加在文件最后。

####返回值
nil

####示例

```lua
    -- 打开'init.lua'，并打印文件的第一行。
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####参见
**-**   [file.close()](#fl_close)<br />
**-**   [file.readline()](#fl_readline)

<a id="fl_close"></a>
## file.close()
####描述
关闭文件。

####语法
file.close()

####参数
nil

####返回值
nil

####示例

```lua
    -- 打开'init.lua'，并打印文件的第一行，然后关闭文件。
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.readline()](#fl_readline)

<a id="fl_readline"></a>
## file.readline()
####描述
读取文件的一行。

####语法
file.readline()

####参数
nil

####返回值
逐行返回文件内容。返回值末尾包含EOL('\n')<br />
如果读到EOF返回nil。

####示例

```lua
    -- 打开'init.lua'，读取并打印文件的第一行，然后关闭文件。
    file.open("init.lua", "r")
    print(file.readline())
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.close()](#fl_close)

<a id="fl_writeline"></a>
## file.writeline()
####描述
向文件写入一行，行末尾增加'\n'。

####语法
file.writeline(string)

####参数
string: 需要写入的字符串

####返回值
true: 写入成功<br />
nil: 写入失败

####示例

```lua
    -- 以'a+'的模式打开'init.lua'
    file.open("init.lua", "a+")
    -- 将'foo bar'写到文件的末尾
    file.writeline('foo bar')
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.write()](#fl_write)

<a id="fl_write"></a>
## file.write()
####描述
向文件写入字符串。

####语法
file.write(string)

####参数
string: 需要写入的字符串

####返回值
true: 写入成功<br />
nil: 写入失败

####示例

```lua
    -- 以'a+'的模式打开'init.lua'
    file.open("init.lua", "a+")
    -- 将'foo bar'写到文件的末尾
    file.writeline('foo bar')
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_flush"></a>
## file.flush()
####描述
清空缓存写入文件。

####语法
file.flush()

####参数
nil

####返回值
nil

####示例

```lua
    -- 以'a+'的模式打开'init.lua'
    file.open("init.lua", "a+")
    -- 将'foo bar'写到文件的末尾
    file.write('foo bar')
    file.flush()
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_seek"></a>
## file.seek()
####描述
设置或者读取文件的读写位置，位置等于whence加上offset的值。

####语法
file.seek(whence, offset)

####参数
whence:<br />
"set": base is position 0 (beginning of the file);<br />
"cur": base is current position;(default value)<br />
"end": base is end of file;<br />
offset: default 0

####返回值
成功: 返回当前的文件读写位置<br />
失败: 返回nil

####示例

```lua
    -- 以'a+'的模式打开'init.lua'
    file.open("init.lua", "a+")
    -- 将'foo bar'写到文件的末尾
    file.write('foo bar')
    file.flush()
    --将文件读写位置设置在文件开始
    file.seek("set")
    --读取并打印文件的第一行
    print(file.readline())
    file.close()
```

####参见
**-**   [file.open()](#fl_open)<br />
**-**   [file.writeline()](#fl_writeline)

<a id="fl_list"></a>
## file.list()
####描述
显示所有文件。

####语法
file.list()

####参数
nil

####返回值
返回包含{文件名：文件大小}的lua table

####示例

```lua
    l = file.list();
    for k,v in pairs(l) do
      print("name:"..k..", size:"..v)
    end
```

####参见
**-**   [file.remove()](#fl_remove)

#wifi模块
##常量
wifi.STATION, wifi.SOFTAP, wifi.STATIONAP

<a id="wf_setmode"></a>
## wifi.setmode(mode)
####描述
设置wifi的工作模式。

####语法
wifi.setmode(mode)

####参数
mode: 取值为：wifi.STATION, wifi.SOFTAP or wifi.STATIONAP

####返回值
返回设置之后的mode值

####示例

```lua
    wifi.setmode(wifi.STATION)
```

####参见
**-**   [wifi.getmode()](#wf_getmode)


<a id="wf_getmode"></a>
## wifi.getmode(mode)
####描述
获取wifi的工作模式。

####语法
wifi.getmode()

####参数
nil

####返回值
返回wifi的工作模式

####示例

```lua
    print(wifi.getmode())
```

####参见
**-**   [wifi.setmode()](#wf_setmode)


<a id="wf_startsmart"></a>
## wifi.startsmart()
####描述
开始自动配置，如果配置成功自动设置ssid和密码。

####语法
wifi.startsmart(channel, function succeed_callback())

####参数

channel: 1~13, 启动寻找的初始频段，如果为nil默认值为6频段。每个频段搜寻20s。<br />
succeed_callback: 配置成功的回调函数，配置成功并连接至AP后调用此函数。

####返回值
nil

####示例

```lua
    wifi.startsmart(6, cb())
```

####参见
**-**   [wifi.stopsmart()](#wf_stopsmart)


<a id="wf_stopsmart"></a>
## wifi.stopsmart()
####描述
停止配置。

####语法
wifi.stopsmart()

####参数
nil

####返回值
nil

####示例

```lua
    wifi.stopsmart()
```

####参见
**-**   [wifi.startsmart()](#wf_startsmart)


#wifi.sta 子模块

<a id="ws_config"></a>
## wifi.sta.config()
####描述
设置station模式下的ssid和password。

####语法
wifi.sta.config(ssid, password)

####参数

ssid: 字符串，长度小于32字节。<br />
password: 字符串，长度小于64字节。

####返回值
nil

####示例

```lua
    wifi.sta.config("myssid","mypassword")
```

####参见
**-**   [wifi.sta.connect()](#ws_connect)<br />
**-**   [wifi.sta.disconnect()](#ws_disconnect)


<a id="ws_connect"></a>
## wifi.sta.connect()
####描述
station模式下连接AP。

####语法
wifi.sta.connect()

####参数
nil


####返回值
nil

####示例

```lua
    wifi.sta.connect()
```

####参见
**-**   [wifi.sta.disconnect()](#ws_disconnect)<br />
**-**   [wifi.sta.config()](#ws_config)


<a id="ws_disconnect"></a>
## wifi.sta.disconnect()
####描述
station模式下与AP断开连接。

####语法
wifi.sta.disconnect()

####参数
nil


####返回值
nil

####示例

```lua
    wifi.sta.disconnect()
```

####参见
**-**   [wifi.sta.config()](#ws_config)<br />
**-**   [wifi.sta.connect()](#ws_connect)


<a id="ws_autoconnect"></a>
## wifi.sta.autoconnect()
####描述
station模式下自动连接。

####语法
wifi.sta.autoconnect(auto)

####参数
auto: 0：取消自动连接，1：使能自动连接。


####返回值
nil

####示例

```lua
    wifi.sta.autoconnect()
```

####参见
**-**   [wifi.sta.config()](#ws_config)<br />
**-**   [wifi.sta.connect()](#ws_connect)<br />
**-**   [wifi.sta.disconnect()](#ws_disconnect)


<a id="ws_getip"></a>
## wifi.sta.getip()
####描述
station模式下获取ip

####语法
wifi.sta.getip()

####参数
nil


####返回值
ip地址字符串，如:"192.168.0.111"

####示例

```lua
    -- print current ip
    print(wifi.sta.getip())
```

####参见
**-**   [wifi.sta.getmac()](#ws_getmac)


<a id="ws_getmac"></a>
## wifi.sta.getmac()
####描述
station模式下获取mac地址。

####语法
wifi.sta.getmac()

####参数
nil


####返回值
mac地址字符串，如:"18-33-44-FE-55-BB"

####示例

```lua
    -- 打印当前的mac地址
    print(wifi.sta.getmac())
```

####参见
**-**   [wifi.sta.getip()](#ws_getip)

<a id="ws_getap"></a>
## wifi.sta.getap()
####描述
扫描并列出ap，结果以一个lua table为参数传递给回调函数。

####语法
wifi.sta.getap(function(table))

####参数
function(table): 当扫描结束时，调用此回调函数<br />
    扫描结果是一个lua table，key为ap的ssid，value为其他信息，格式：authmode,rssi,bssid,channel


####返回值
nil

####示例

```lua
    -- print ap list
    function listap(t)
      for k,v in pairs(t) do
        print(k.." : "..v)
      end
    end
    wifi.sta.getap(listap)
```

####参见
**-**   [wifi.sta.getip()](#ws_getip)

#wifi.ap 子模块

<a id="wa_config"></a>
## wifi.ap.config()
####描述
设置ap模式下的ssid和password

####语法
wifi.ap.config(cfg)

####参数
cfg: 设置AP的lua table

####示例:

```lua
     cfg={}
     cfg.ssid="myssid"
     cfg.pwd="mypwd"
     wifi.ap.setconfig(cfg)
```

####返回值
nil

####示例

```lua
    wifi.ap.config(ssid, 'password')
```

####参见
**-**    []()

<a id="wa_getip"></a>
## wifi.ap.getip()
####描述
ap模式下获取ip

####语法
wifi.ap.getip()

####参数
nil

####返回值
ip地址字符串，如:"192.168.0.111"

####示例

```lua
    wifi.ap.getip()
```

####参见
**-**   [wifi.ap.getmac()](#wa_getmac)


<a id="wa_getmac"></a>
## wifi.ap.getmac()
####描述
ap模式下获取mac地址。

####语法
wifi.ap.getmac()

####参数
nil

####返回值
mac地址字符串，如:"1A-33-44-FE-55-BB"

####示例

```lua
    wifi.ap.getmac()
```

####参见
**-**   [wifi.ap.getip()](#wa_getip)


#timer 模块
<a id="tm_delay"></a>
## tmr.delay()
####描述
延迟us微秒。

####语法
tmr.dealy(us)

####参数
us: 延迟时间，单位：微秒

####返回值
nil

####示例

```lua
    -- delay 100us
    tmr.delay(100)
```

####参见
**-**   [tmr.now()](#tm_now)


<a id="tm_now"></a>
## tmr.now()
####描述
返回系统计数器的当前值，uint32，单位：us。

####语法
tmr.now()

####参数
nil

####返回值
uint32: value of counter

####示例

```lua
    -- 打印计数器的当前值。
    print(tmr.now())
```

####参见
**-**   [tmr.delay()](#tm_delay)


<a id="tm_alarm"></a>
## tmr.alarm()
####描述
闹钟函数。<br />
**-注意:** 只能允许存在一个闹钟，如果在调用tmr.stop()之前重复调用tmr.alarm()，以最后一次设置的为准，此前定义的闹钟都将失效。

####语法
tmr.alarm(interval, repeat, function do())

####参数
Interval: 定时时间，单位：毫秒。<br />
repeat: 0：一次性闹钟；1：重复闹钟。<br />
function do(): 定时器到时回调函数。

####返回值
nil

####示例

```lua
    -- 每1000ms输出一个hello world
    tmr.alarm(1000, 1, function() print("hello world") end )
```

####参见
**-**   [tmr.now()](#tm_now)


<a id="tm_stop"></a>
## tmr.stop()
####描述
停止闹钟功能。<br />

####语法
tmr.stop()

####参数
nil.

####返回值
nil

####示例

```lua
    -- 每隔1000ms打印hello world
    tmr.alarm(1000, 1, function() print("hello world") end )

    -- 其它代码

    -- 停止闹钟
    tmr.stop()
```

####参见
**-**   [tmr.now()](#tm_now)

<a id="tm_wdclr"></a>
## tmr.wdclr()
####描述
清除看门狗计数器。<br />

####语法
tmr.wdclr()

####参数
nil.

####返回值
nil

####示例

```lua
    for i=1,10000 do 
      print(i)
      tmr.wdclr()   -- 一个长时间的循环或者事务，需内部调用tmr.wdclr() 清楚看门狗计数器，防止重启。
    end 
```

####参见
**-**   [tmr.delay()](#tm_delay)

#GPIO 模块
##常量
gpio.OUTPUT, gpio.INPUT, gpio.INT, gpio.HIGH, gpio.LOW


<a id="io_mode"></a>
## gpio.mode()
####描述
将pin初始化为GPIO并设置输入输出模式。

####语法
gpio.mode(pin, mode)

####参数
pin: 0~11, IO编号<br />
mode: 取值为：gpio.OUTPUT or gpio.INPUT, or gpio.INT(中断模式)

####返回值
nil

####示例

```lua
    -- 将GPIO0设置为输出模式
    gpio.mode(0, gpio.OUTPUT)

```

####参见
**-**   [gpio.read()](#io_read)


<a id="io_read"></a>
## gpio.read()
####描述
读取管脚电平高低。

####语法
gpio.read(pin)

####参数
pin: 0~11, IO编号

####返回值
number:0：低电平, 1：高电平。

####示例

```lua
    -- 读取GPIO0的电平
    gpio.read(0)
```

####参见
**-**   [gpio.mode()](#io_mode)


<a id="io_write"></a>
## gpio.write()
####描述
设置管脚电平

####语法
gpio.write(pin)

####参数
pin: 0~11, IO编号<br />
level: gpio.HIGH or gpio.LOW

####返回值
nil

####示例

```lua
    -- 设置GPIP1为输出模式，并将输出电平设置为高
    pin=1
    gpio.mode(pin, gpio.OUTPUT)
    gpio.write(pin, gpio.HIGH)
```

####参见
**-**   [gpio.mode()](#io_mode)<br />
**-**   [gpio.read()](#io_read)


<a id="io_trig"></a>
## gpio.trig()
####描述
设置管脚中断模式的回调函数。

####语法
gpio.trig(pin, type, function(level))

####参数
pin: 0~11, IO编号<br />
type: 取值为"up", "down", "both", "low", "high", 分别代表上升沿、下降沿、双边沿、低电平、高电平触发方式。<br />
function(level): 中断触发的回调函数，GPIO的电平作为输入参数。如果此处没有定义函数，则使用之前定义的回调函数。

####返回值
nil

####示例

```lua
    -- 使用GPIO0检测输入脉冲宽度
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

####参见
**-**   [gpio.mode()](#io_mode)<br />
**-**   [gpio.write()](#io_write)


#PWM模块
<a id="pw_setup"></a>
## pwm.setup()
####描述
设置管脚为pwm模式，最多支持6个pwm。

####语法
pwm.setup(pin, clock, duty)

####参数
pin: 0~11, IO编号<br />
clock: 1~500, pwm频率<br />
duty: 0~100, pwm占空比，百分比表示。

####返回值
nil

####示例

```lua
    -- 将管脚0设置为pwm输出模式，频率100Hz，占空比50-50
    pwm.setup(0, 100, 50)
```

####参见
**-**   [pwm.start()](#pw_start)


<a id="pw_close"></a>
## pwm.close()
####描述
退出pwm模式。

####语法
pwm.close(pin)

####参数
pin: 0~11, IO编号

####返回值
nil

####示例

```lua
    pwm.close(0)
```

####参见
**-**   [pwm.start()](#pw_start)


<a id="pw_start"></a>
## pwm.start()
####描述
pwm启动，可以在对应的GPIO检测到波形。

####语法
pwm.start(pin)

####参数
pin: 0~11, IO编号

####返回值
nil

####示例

```lua
    pwm.start(0)
```

####参见
**-**   [pwm.stop()](#pw_stop)


<a id="pw_stop"></a>
## pwm.stop()
####描述
暂停pwm输出波形。

####语法
pwm.stop(pin)

####参数
pin: 0~11, IO编号

####返回值
nil

####示例

```lua
    pwm.stop(0)
```

####参见
**-**   [pwm.start()](#pw_start)


<a id="pw_setclock"></a>
## pwm.setclock()
####描述
设置pwm的频率<br />
**-Note:** 设置pwm频率将会同步改变其他pwm输出的频率，当前版本的所有pwm仅支持同一频率输出。

####语法
pwm.setclock(pin, clock)

####参数
pin: 0~11, IO编号<br />
clock: 1~500, pwm周期

####返回值
nil

####示例

```lua
    pwm.setclock(0, 100)
```

####参见
**-**   [pwm.getclock()](#pw_getclock)


<a id="pw_getclock"></a>
## pwm.getclock()
####描述
获取pin的pwm工作频率

####语法
pwm.getclock(pin)

####参数
pin: 0~11, IO编号

####返回值
number:pin的pwm工作频率

####示例

```lua
    print(pwm.getclock(0))
```

####参见
**-**   [pwm.setclock()](#pw_setclock)


<a id="pw_setduty"></a>
## pwm.setduty()
####描述
设置pin的占空比。

####语法
pwm.setduty(pin, duty)

####参数
pin: 0~11, IO编号<br />
duty: 0~100, pwm的占空比，以百分数表示

####返回值
nil

####示例

```lua
    pwm.setduty(0, 50)
```

####参见
**-**   [pwm.getduty()](#pw_getduty)


<a id="pw_getduty"></a>
## pwm.getduty()
####描述
获取pin的pwm占空比。

####语法
pwm.getduty(pin)

####参数
pin: 0~11, IO编号

####返回值
nil

####示例

```lua
    -- D0 连接绿色led
    -- D1 连接蓝色led
    -- D2 连接红色led
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
    led(50,0,0) --  led显示红色
    led(0,0,50) -- led显示蓝色

```

####参见
**-**   [pwm.setduty()](#pw_setduty)


#net 模块
##常量
net.TCP, net.UDP

<a id="nt_createServer"></a>
## net.createServer()
####描述
创建一个server。

####语法
net.createServer(type, timeout)

####参数
type: 取值为：net.TCP 或者 net.UDP<br />
timeout: 1~28800, 当为tcp服务器时，客户端的超时时间设置。

####返回值
net.server子模块

####示例

```lua
    net.createServer(net.TCP, 30)
```

####参见
**-**   [net.createConnection()](#nt_createConnection)


<a id="nt_createConnection"></a>
## net.createConnection()
####描述
创建一个client。

####语法
net.createConnection(type, secure)

####参数
type: 取值为：net.TCP 或者 net.UDP<br />
secure: 设置为1或者0, 1代表安全连接，0代表普通连接。

####返回值
net.server子模块

####示例

```lua
    net.createConnection(net.UDP, 0)
```

####参见
**-**   [net.createServer()](#nt_createServer)


#net.server 子模块
<a id="ns_listen"></a>
## listen()
####描述
侦听指定ip地址的端口。

####语法
net.server.listen(port,[ip],function(net.socket))

####参数
port: 端口号<br />
ip:ip地址字符串，可以省略<br />
function(net.socket): 连接创建成功的回调函数，可以作为参数传给调用函数。

####返回值
nil

####示例

```lua
    -- 创建一个server
    sv=net.createServer(net.TCP, 30)  -- 30s 超时
    -- server侦听端口80，如果收到数据将数据打印至控制台，并向远端发送‘hello world’
    sv:listen(80,function(c)
      c:on("receive", function(sck, pl) print(pl) end)
      c:send("hello world")
      end)
```

####参见
**-**   [net.createServer()](#nt_createServer)


<a id="ns_close"></a>
## close()
####描述
关闭server

####语法
net.server.close()

####参数
nil

####返回值
nil

####示例

```lua
    -- 创建server
    sv=net.createServer(net.TCP, 5)
    -- 关闭server
    sv:close()
```

####参见
**-**   [net.createServer()](#nt_createServer)


#net.socket 子模块
<a id="nk_connect"></a>
## connect()
####描述
连接至远端。

####语法
connect(port, ip)

####参数
port: 端口号<br />
ip: ip地址字符串

####返回值
nil

####参见
**-**   [net.socket:on()](#nk_on)


<a id="nk_send"></a>
## send()
####描述
通过连接向远端发送数据。

####语法
send(string, function(sent))

####参数
string: 待发送的字符串<br />
function(sent): 发送字符串后的回调函数。

####返回值
nil

####参见
**-**   [net.socket:on()](#nk_on)


<a id="nk_on"></a>
## on()
####描述
向事件注册回调函数。

####语法
on(event, function cb())

####参数
event: 字符串，取值为: "connection", "reconnection", "disconnection", "receive", "sent"<br />
function cb(net.socket, [string]): 回调函数。第一个参数是socket.<br />
如果事件是"receive",  第二个参数则为接收到的字符串。

####返回值
nil

####示例

```lua
    sk=net.createConnection(net.TCP, 0)
    sk:on("receive", function(sck, c) print(c) end )
    sk:connect(80,"192.168.0.66")
    sk:send("GET / HTTP/1.1\r\nHost: 192.168.0.66\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")
```

####参见
**-**   [net.createServer()](#nt_createServer)


<a id="nk_close"></a>
## close()
####描述
关闭socket。

####语法
close()

####参数
nil

####返回值
nil

####参见
**-**   [net.createServer()](#nt_createServer)


<a id="nk_dns"></a>
## dns()
####描述
获取当前域的ip

####语法
dns(domain, function(net.socket, ip))

####参数
domain: 当前域的名称<br />
function (net.socket, ip): 回调函数。第一个参数是socket，第二个参数是当前域的ip字符串。

####返回值
nil

####参见
**-**   [net.createServer()](#nt_createServer)


#i2c模块
##常量
i2c.SLOW,  i2c.TRANSMITTER, i2c. RECEIVER.  FAST（400k）模式目前不支持。

<a id="ic_setup"></a>
## i2c.setup()
####描述
初始化i2c。

####语法
i2c.setup(id, pinSDA, pinSCL, speed)

####参数
id = 0<br />
pinSDA: 0~11, IO编号<br />
pinSCL: 0~11, IO编号<br />
speed:  i2c.SLOW

####返回值
nil

####参见
**-**   [i2c.read()](#ic_read)


<a id="ic_start"></a>
## i2c.start()
####描述
启动i2c传输。

####语法
i2c.start(id)

####参数
id = 0

####返回值
nil

####参见
**-**   [i2c.read()](#ic_read)


<a id="ic_stop"></a>
## i2c.stop()
####描述
停止i2c传输。

####语法
i2c.stop(id)

####参数
id = 0

####返回值
nil

####参见
**-**   [i2c.read()](#ic_read)


<a id="ic_address"></a>
## i2c.address()
####描述
设置i2c地址以及读写模式。

####语法
i2c.address(id, device_addr, direction)

####参数
id=0<br />
device_addr: 设备地址。<br />
direction: i2c.TRANSMITTER：写模式；i2c. RECEIVER：读模式。

####返回值
nil

####参见
**-**   [i2c.read()](#ic_read)

<a id="ic_write"></a>
## i2c.write()
####描述
向i2c写数据。数据可以是多个数字, 字符串或者lua table。

####语法
i2c.write(id, data1, data2,...)

####参数
id=0<br />
data: 数据可以是多个数字, 字符串或者lua table。

####返回值
nil

####示例

```lua
    i2c.write(0, "hello", "world")
```

####参见
**-**   [i2c.read()](#ic_read)


<a id="ic_read"></a>
## i2c.read()
####描述
读取len个字节的数据。

####语法
i2c.read(id, len)

####参数
id=0<br />
len: 数据长度。

####返回值
string:接收到的数据。

####示例

```lua
    id=0
    sda=1
    scl=0

    -- 初始化i2c, 将pin1设置为sda, 将pin0设置为scl
    i2c.setup(id,sda,scl,i2c.SLOW)

    -- 用户定义函数:读取地址dev_addr的寄存器reg_addr中的内容。
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

    -- 读取0x77的寄存器0xAA中的内容。
    reg = read_reg(0x77, 0xAA)
    pirnt(string.byte(reg))

```

####参见
**-**   [i2c.write()](#ic_write)

#adc 模块
##常量
无

<a id="adc_read"></a>
## adc.read()
####描述
读取adc的值，esp8266只有一个10bit adc，id为0，引脚为TOUT，最大值1024

####语法
adc.read(id)

####参数
id = 0<br />

####返回值
adc 值 10bit，最大1024.

####参见
**-**   []()
