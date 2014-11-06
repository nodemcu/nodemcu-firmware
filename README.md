# **nodeMcu API Instruction** #
   version 0.1 2014-10-11
   change log：
   2014-11-5
   delete log operation api from node module
   add log module
   modify wifi module api
   modify node.key long_press and short_press default function
   key is triged only when key is released

# Summary
- Easy to access wireless router
- Based on Lua 5.1.4，Developer are supposed to have experience in Lua Program language.
- Try to use Event-Drive programming modal.
- In-side  timer，pwm，i2c，net，gpio，wifi module.
- Serial Port BaudRate:74880
- Re-indexing the 8266 pin，use the index to program gpio，i2c，pwm.
- Index vs Pin-number Table:
| IO index | ESP8266 pin | IO index | ESP8266 pin |
| :------: | :---------: | :------: | :---------: |
|      0   |    GPIO 12  |      8   |    GPIO 00  |
|      1   |    GPIO 13  |      9   |    GPIO 02  |
|      2   |    GPIO 14  |      10  |    GPIO 04  |
|      3   |    GPIO 15  |      11  |    GPIO 05  |
|      4   |    GPIO 03  |          |             |
|      5   |    GPIO 01  |          |             |
|      6   |    GPIO 09  |          |             |
|      7   |    GPIO 10  |          |             |


#Firmware Program
Address
:    eagle.app.v6.flash.bin: 0x00000
:    eagle.app.v6.irom0text.bin: 0x10000
:    esp_init_data_default.bin: 0x7c000
:    blank.bin: 0x7e000


#node module
<a name="nm.restart">
## node.restart()
Description
:    module restart.

Syntax
:    node.restart()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    node.restart();
```

See also
:    **-**   <a href="other functions">other function</a>

<a name="nm.dsleep">
## node.dsleep()
Description
:    enter deep sleep mode for us micro seconds，restart when timed out
     us: sleep time in micro second

Syntax
:    node.dsleep(us)
     Note: This function can only be used in the condition of connecting esp8266 PIN32(rst) and PIN8(XPD_DCDC) together.

Parameters
:    us:sleep time in micro second

Returns
:    null

Example
:    ****
```
    node.dsleep(us);
```

See also
:    **-**   <a href="other functions">other function</a>

<a name="nm.chipid">
## node.chipid()
Description
:    return chip identifier

Syntax
:    node.chipid()

Parameters
:    null

Returns
:    number:chip identifier

Example
:    ****
```
    uint32 id = node.chipid();
```

See also
:    **-**   <a href="other functions">other function</a>

<a name="nm.heap">
## node.heap()
Description
:    return the available RAM size in bytes

Syntax
:    node.heap()

Parameters
:    null

Returns
:    number:system heap size left in bytes

Example
:    ****
```
    uint32 heap_size = node.heap();
```

See also
:    **-**   <a href="other functions">other function</a>

<a name="nm.key">
## node.key()
Description
:    define button function.

Syntax
:    node.key(type, function())

Parameters
:    type: type is either string ”long” or ”short”. long: press the button for 3 seconds, short: press shortly(less than 3 seconds)
     function(): user defined function for button. If null, cancling the user defined function, function are initialized to default.
     Default function: long：change LED blinking rate,  short：reset chip

Returns
:    null

Example
:    ****
```
    node.key(long, function(){print('hello world')})
```

See also
:    **-**   <a href="nm.startlog">node.startlog()</a>

<a name="nm.led">
## node.led()
Description
:    setup the on/off time for led

Syntax
:    node.key(type, function())

Parameters
:    Low: LED off time, 0 for LED keeps on. Unit: milliseconds, time resolution: 80~100ms
     High: LED off time. Unit: milliseconds, time resolution: 80~100ms

Returns
:    null

Example
:    ****
```
    //turn led on forever.
    node.led(0,null);
```

See also
:    **-**   <a href="nm.startlog">node.startlog()</a>

#log module
<a name="lg.format">
## log.format()
Description
:    format flash for users.

Syntax
:    log.format()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    //record log to init.lua. Call the file after system restart.
    log.format()
    log.startlog(“init.lua”, 1)
    print(“hello world”)
    log.stoplog()
```

See also
:    **-**   <a href="lg.startlog">log.startlog()</a>
:    **-**   <a href="lg.stoplog">log.stoplog()</a>

<a name="lg.startlog">
## log.startlog()
Description
:    start to log input

Syntax
:    log.startlog(filename, noparse)

Parameters
:    filename: log file, directories are not supported
     noparse: 1 for lua VM doesn’t parse input, 0 for lua VM parse input

Returns
:    null

Example
:    ****
```
    //record log to init.lua. Call the file after system restart.
    log.format()
    log.startlog(“init.lua”, 1)
    print(“hello world”)
    log.stoplog()
    //At this point, the content of init.lua is “print(“hello world”)”. When system restart, print(“hello world”) are excuted.
```

See also
:    **-**   <a href="lg.format">log.format()</a>
:    **-**   <a href="lg.stoplog">log.stoplog()</a>

<a name="lg.stoplog">
## log.stoplog()
Description
:    stop log.

Syntax
:    log.stoplog()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    //record log to init.lua. Call the file after system restart.
    log.format()
    log.startlog(“init.lua”, 1)
    print(“hello world”)
    log.stoplog()
    //At this point, the content of init.lua is “print(“hello world”)”. When system restart, print(“hello world”) are excuted.
```

See also
:    **-**   <a href="lg.format">log.format()</a>
:    **-**   <a href="lg.startlog">log.startlog()</a>

<a name="lg.open">
## log.open()
Description
:    open the log file

Syntax
:    log.open(filename)

Parameters
:    filename: log file, directories are not supported

Returns
:    null

Example
:    ****
```
    //print the first line of 'init.lua'
    log.open(“init.lua”)
    print(log.readline())
    log.close()
```

See also
:    **-**   <a href="lg.close">log.close()</a>
:    **-**   <a href="lg.readline">log.readline()</a>

<a name="lg.close">
## log.close()
Description
:    close the log file which opened before

Syntax
:    log.close()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    //print the first line of 'init.lua'
    log.open(“init.lua”)
    print(log.readline())
    log.close()
```

See also
:    **-**   <a href="lg.open">log.open()</a>
:    **-**   <a href="lg.readline">log.readline()</a>

<a name="lg.readline">
## log.readline()
Description
:    read log file which is opened before line by line.

Syntax
:    log.readline()

Parameters
:    null

Returns
:    log file content in string

Example
:    ****
```
    //print the first line of 'init.lua'
    log.open(“init.lua”)
    print(log.readline())
    log.close()
```

See also
:    **-**   <a href="lg.open">log.open()</a>
:    **-**   <a href="lg.close">log.close()</a>

<a name="lg.list">
## log.list()
Description
:    list all files.

Syntax
:    log.list()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    log.list();
```

See also
:    **-**   <a href="lg.format">log.format()</a>

#wifi module
##CONSTANT
:    wifi.STATION, wifi.SOFTAP, wifi.STATIONAP

<a name="wf.setmode">
## wifi.setmode(mode)
Description
:    set wifi working mode.

Syntax
:    wifi.setmode(mode)

Parameters
:    mode: value should be: wifi. STATION, wifi.SOFTAP or wifi.STATIONAP

Returns
:    current mode after setup

Example
:    ****
```
    wifi.setmode(wifi.STATION)
```

See also
:    **-**   <a href="wf.getmode">wf.getmode()</a>

<a name="wf.getmode">
## wifi.getmode(mode)
Description
:    get wifi working mode.

Syntax
:    wifi.getmode()

Parameters
:    null

Returns
:    wifi working mode

Example
:    ****
```
    print(wifi.getmode())
```

See also
:    **-**   <a href="wf.setmode">wf.setmode()</a>

<a name="wf.startsmart">
## wifi.startsmart()
Description
:    starts to auto configuration，if success set up ssid and pwd automatically .

Syntax
:    wifi.startsmart(channel, function succeed_callback())

Parameters
:    channel: 1~13，startup channel for searching, if null, default to 6.  20 seconds for each channel.
     succeed_callback: callback function for success configuration, which is called after getting the password and the connection to AP.

Returns
:    null

Example
:    ****
```
    wifi.startsmart(6, cb())
```

See also
:    **-**   <a href="wf.stopsmart">wf.stopsmart()</a>

<a name="wf.stopsmart">
## wifi.stopsmart()
Description
:    stop the configuring process.

Syntax
:    wifi.stopsmart()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    wifi.stopsmart()
```

See also
:    **-**   <a href="wf.startsmart">wf.startsmart()</a>

#wifi.sta module

<a name="ws.config">
## wifi.sta.config()
Description
:    set ssid and password in station mode.

Syntax
:    wifi.sta.config(ssid, password)

Parameters
:    ssid: string which is less than 32 bytes.
     password: string which is less than 64 bytes.


Returns
:    null

Example
:    ****
```
    wifi.sta.config("myssid","mypassword")
```

See also
:    **-**   <a href="ws.connect">wifi.sta.connect()</a>
:    **-**   <a href="ws.disconnect">wifi.sta.disconnect()</a>

<a name="ws.connect">
## wifi.sta.connect()
Description
:    connect to AP in station mode.

Syntax
:    wifi.sta.connect()

Parameters
:    null


Returns
:    null

Example
:    ****
```
    wifi.sta.connect()
```

See also
:    **-**   <a href="ws.disconnect">wifi.sta.disconnect()</a>
:    **-**   <a href="ws.config">wifi.sta.config()</a>

<a name="ws.disconnect">
## wifi.sta.disconnect()
Description
:    disconnect from AP in station mode.

Syntax
:    wifi.sta.disconnect()

Parameters
:    null


Returns
:    null

Example
:    ****
```
    wifi.sta.disconnect()
```

See also
:    **-**   <a href="ws.config">wifi.sta..config()</a>
:    **-**   <a href="ws.connect">wifi.sta.connect()</a>

<a name="ws.autoconnect">
## wifi.sta.autoconnect()
Description
:    auto connect to AP in station mode.

Syntax
:    wifi.sta.autoconnect(auto)

Parameters
:    auto: 0 for disable auto connecting. 1 for enable auto connecting


Returns
:    null

Example
:    ****
```
    wifi.sta.autoconnect()
```

See also
:    **-**   <a href="ws.config">wifi.sta..config()</a>
:    **-**   <a href="ws.connect">wifi.sta.connect()</a>
:    **-**   <a href="ws.disconnect">wifi.sta.disconnect()</a>

<a name="ws.getip">
## wifi.sta.getip()
Description
:    get ip address in station mode.

Syntax
:    wifi.sta.getip()

Parameters
:    null


Returns
:    ip address in string, for example:"192.168.0.111"

Example
:    ****
```
    //print current ip
    print(wifi.sta.getip())
```

See also
:    **-**   <a href="ws.getmac">wifi.sta..getmac()</a>

<a name="ws.getmac">
## wifi.sta.getmac()
Description
:    get mac address in station mode.

Syntax
:    wifi.sta.getmac()

Parameters
:    null


Returns
:    mac address in string, for example:"18-33-44-FE-55-BB"

Example
:    ****
```
    //print current mac address
    print(wifi.sta.getmac())
```

See also
:    **-**   <a href="ws.getip">wifi.sta..getip()</a>

#wifi.ap module

<a name="wa.config">
## wifi.ap.config()
Description
:    set ssid and password in ap mode.

Syntax
:    wifi.ap.config(cfg)

Parameters
:    cfg: lua table for setup ap.
:    Example:
```
     cfg={}
     cfg.ssid="myssid"
     cfg.pwd="mypwd"
     wifi.ap.setconfig(cfg)
```

Returns
:    null

Example
:    ****
```
    wifi.ap.config(ssid, 'password')
```

See also
:    <>

<a name="wa.getip">
## wifi.ap.getip()
Description
:    get ip in ap mode.

Syntax
:    wifi.ap.getip()

Parameters
:    null

Returns
:    ip address in string, for example:"192.168.0.111"

Example
:    ****
```
    wifi.ap.getip()
```

See also
:    **-**   <a href="wa.getmac">wifi.ap..getmac()</a>

<a name="wa.getmac">
## wifi.ap.getmac()
Description
:    get mac address in ap mode.

Syntax
:    wifi.ap.getmac()

Parameters
:    null

Returns
:    mac address in string, for example:"1A-33-44-FE-55-BB"

Example
:    ****
```
    wifi.ap.getmac()
```

See also
:    **-**   <a href="wa.getip">wifi.ap..getip()</a>

#timer module
<a name="tm.delay">
## tmr.delay()
Description
:    delay us micro seconds.

Syntax
:    tmr.dealy(us)

Parameters
:    us: delay time in micro second

Returns
:    null

Example
:    ****
```
    //delay 100us
    tmr.delay(100)
```

See also
:    **-**   <a href="tm.now">tmr.now()</a>

<a name="tm.now">
## tmr.now()
Description
:    return the current value of system counter: uint32, loopback, us.

Syntax
:    tmr.now()

Parameters
:    null

Returns
:    uint32: value of counter

Example
:    ****
```
    //print current value of counter
    print(tmr.now())
```

See also
:    **-**   <a href="tm.delay">tmr.delay()</a>

<a name="tm.alarm">
## tmr.alarm()
Description
:    alarm time.

Syntax
:    tmr.alarm(interval, repeat, function do())

Parameters
:    Interval: alarm time, unit: millisecond;
     repeat: 0 for one time alarm, 1 for repeat;
     function do(): callback function for alarm timed out.

Returns
:    null

Example
:    ****
```
    //print "hello world" every 1000ms
    tmr.alarm(1000, 1, function() print(“hello world”) end )
```

See also
:    **-**   <a href="tm.now">tmr.now()</a>

<a name="tm.stop">
## tmr.stop()
Description
:    stop alarm.
:    Note: only one alarm is allowed, the previous one would be replaced if tmr.alarm() again before tmr.stop().

Syntax
:    tmr.stop()

Parameters
:    null.

Returns
:    null

Example
:    ****
```
    //print "hello world" every 1000ms
    tmr.alarm(1000, 1, function() print(“hello world”) end )

    //something else

    //stop alarm
    tmr.stop()
```

See also
:    **-**   <a href="tm.now">tmr.now()</a>

#GPIO module
##CONSTANT
:    gpio.OUTPUT, gpio.INPUT, gpio.INT, gpio.HIGH, gpio.LOW


<a name="io.mode">
## gpio.mode()
Description
:    initialize pin to GPIO mode, set the pin in/out mode.

Syntax
:    gpio.mode(pin, mode)

Parameters
:    pin: 0~11，IO index
     mode: gpio.OUTPUT or gpio.INPUT, or gpio.INT(interrupt mode)

Returns
:    null

Example
:    ****
```
    //set gpio 0 as output.
    gpio.mode(0, gpio.OUTPUT)

```

See also
:    **-**   <a href="io.read">gpio.read()</a>

<a name="io.read">
## gpio.read()
Description
:    read pin value.

Syntax
:    gpio.read(pin)

Parameters
:    pin: 0~11，IO index

Returns
:    number:0 for low, 1 for high

Example
:    ****
```
    //read value of gpio 0.
    gpio.read(0)
```

See also
:    **-**   <a href="io.mode">gpio.mode()</a>

<a name="io.write">
## gpio.write()
Description
:    set pin value.

Syntax
:    gpio.write(pin)

Parameters
:    pin: 0~11，IO index
:    level: gpio.HIGH or gpio.LOW

Returns
:    null

Example
:    ****
```
    //set pin index 1 to GPIO mode, and set the pin to high.
    pin=1
    gpio.mode(pin, gpio.OUTPUT)
    gpio.write(pin, gpio.HIGH)
```

See also
:    **-**   <a href="io.mode">gpio.mode()</a>
:    **-**   <a href="io.read">gpio.read()</a>

<a name="io.trig">
## gpio.trig()
Description
:    set the interrupt callback function for pin.

Syntax
:    gpio.trig(pin, type, function(level))

Parameters
:    pin: 0~11，IO index
     type: ”up”, “down”, “both”, “low”, “high”, which represent rising edge, falling edge, both edge, low level, high level trig mode separately.
     function(level): callback function when triggered. The gpio level is the param.
     Use previous callback function if undefined here.

Returns
:    null

Example
:    ****
```
    //use pin 0 as the input pulse width counter
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

See also
:    **-**   <a href="io.mode">gpio.mode()</a>
:    **-**   <a href="io.write">gpio.write()</a>

#PWM module
<a name="pw.setup">
## pwm.setup()
Description
:    set pin to PWM mode. Only 3 pins can be set to PWM mode at the most.

Syntax
:    pwm.setup(pin, clock, duty)

Parameters
:    pin: 0~11，IO index
     clock: 1~500，pwm frequency
     duty: 0~100，pwm duty cycle in percentage

Returns
:    null

Example
:    ****
```
    //set pin index 0 as pwm output, frequency is 100Hz, duty cycle is 50-50..
    pwm.setup(0, 100, 50)
```

See also
:    **-**   <a href="pw.start">pwm.start()</a>

<a name="pw.close">
## pwm.close()
Description
:    quit PWM mode for specified pin.

Syntax
:    pwm.close(pin)

Parameters
:    pin: 0~11，IO index

Returns
:    null

Example
:    ****
```
    pwm.close(0)
```

See also
:    **-**   <a href="pw.start">pwm.start()</a>

<a name="pw.start">
## pwm.start()
Description
:    pwm starts, you can detect the waveform on the gpio.

Syntax
:    pwm.start(pin)

Parameters
:    pin: 0~11，IO index

Returns
:    null

Example
:    ****
```
    pwm.start(0)
```

See also
:    **-**   <a href="pw.stop">pwm.stop()</a>

<a name="pw.stop">
## pwm.stop()
Description
:    pause the output of PWM waveform.

Syntax
:    pwm.stop(pin)

Parameters
:    pin: 0~11，IO index

Returns
:    null

Example
:    ****
```
    pwm.stop(0)
```

See also
:    **-**   <a href="pw.start">pwm.start()</a>

<a name="pw.setclock">
## pwm.setclock()
Description
:    set pwm frequency for pin.
:    Note: setup pwm frequency will synchronously change others if there are any. Only one PWM frequency can be allowed for the system.

Syntax
:    pwm.setclock(pin, clock)

Parameters
:    pin: 0~11，IO index.
     clock: 1~500, pwm frequency.

Returns
:    null

Example
:    ****
```
    pwm.setclock(0, 100)
```

See also
:    **-**   <a href="pw.getclock">pwm.getclock()</a>

<a name="pw.getclock">
## pwm.getclock()
Description
:    get pwm frequency of pin.

Syntax
:    pwm.getclock(pin)

Parameters
:    pin: 0~11，IO index.

Returns
:    number:pwm frequency of pin

Example
:    ****
```
    print(pwm.getclock(0))
```

See also
:    **-**   <a href="pw.setclock">pwm.setclock()</a>

<a name="pw.setduty">
## pwm.setduty()
Description
:    set duty clycle for pin.

Syntax
:    pwm.setduty(pin, duty)

Parameters
:    pin: 0~11，IO index
:    duty: 0~100，pwm duty cycle in percentage

Returns
:    null

Example
:    ****
```
    pwm.setduty(0, 50)
```

See also
:    **-**   <a href="pw.getduty">pwm.getduty()</a>

<a name="pw.getduty">
## pwm.getduty()
Description
:    get duty clycle for pin.

Syntax
:    pwm.getduty(pin)

Parameters
:    pin: 0~11，IO index

Returns
:    null

Example
:    ****
```
    //D0 is connected to green led
    //D1 is connected to blue led
    //D2 is connected to red led
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
    led(50,0,0)	// set led to red
    led(0,0,50)	//set led to blue.

```

See also
:    **-**   <a href="pw.setduty">pwm.setduty()</a>

#net module
##CONSTANT
:    net.TCP, net.UDP

<a name="nt.createServer">
## net.createServer()
Description
:    create a server.

Syntax
:    net.createServer(type, secure)

Parameters
:    type: net.TCP or net.UDP
     secure: true or false, true for safe link, false for ordinary link

Returns
:    net.server sub module

Example
:    ****
```
    net.createServer(net.TCP, true)
```

See also
:    **-**   <a href="nt.createConnection">net.createConnection()</a>

<a name="nt.createConnection">
## net.createConnection()
Description
:    create a client.

Syntax
:    net.createConnection(type, secure)

Parameters
:    type: net.TCP or net.UDP
     secure: true or false, true for safe link, false for ordinary link

Returns
:    net.server sub module

Example
:    ****
```
    net.createConnection(net.UDP, false)
```

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

#net.server module
<a name="ns.listen">
## listen()
Description
:    listen on port from [ip] address.

Syntax
:    net.server.listen(port,[ip],function(net.socket))

Parameters
:    port: port number
     ip:ip address string, can be omitted
     function(net.socket): callback function, pass to Caller function as param if a connection is created successfully

Returns
:    null

Example
:    ****
```
    //create a server
    sv=net.createServer(net.TCP, false)
    //server listen on 80, if data received, print data to console, and send "hello world" to remote.
    sv:listen(80,function(c)
    	c:on("receive", function(sck, pl) print(pl) end)
    	c:send("hello world")
    	end)
```

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

<a name="ns.close">
## close()
Description
:    close server.

Syntax
:    net.server.close()

Parameters
:    null

Returns
:    null

Example
:    ****
```
    //create a server
    sv=net.createServer(net.TCP, false)
    //close server
    sv:close()
```

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

#net.socket module
<a name="nk.connect">
## connect()
Description
:    connect to remote.

Syntax
:    connect(port, ip)

Parameters
:    port: port number
     ip: ip address in string

Returns
:    null

See also
:    **-**   <a href="nk.on">net.socket:on()</a>

<a name="nk.send">
## send()
Description
:    send data to remote via connection.

Syntax
:    send(string, function(sent))

Parameters
:    string: data in string which will be sent to remote
     function(sent): callback function for sending string

Returns
:    null

See also
:    **-**   <a href="nk.on">net.socket:on()</a>

<a name="nk.on">
## on()
Description
:    register callback function for event.

Syntax
:    on(event, function cb())

Parameters
:    event: string, which can be: "connection"，"reconnection"，"disconnection"，"receive"，"sent"
     function cb(net.socket, [string]): callback function. The first param is the socket.
     If  event is”receive”， the second param is received data in string.

Returns
:    null

Example
:    ****
```
    sk=net.createConnection(net.TCP, false)
    sk:on("receive", function(sck, c) print(c) end )
    sk:connect(80,"192.168.0.66")
    sk:send("GET / HTTP/1.1\r\nHost: 192.168.0.66\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")
```

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

<a name="nk.close">
## close()
Description
:    close socket.

Syntax
:    close()

Parameters
:    null

Returns
:    null

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

<a name="nk.dns">
## dns()
Description
:    get domain ip

Syntax
:    dns(domain, function(net.socket, ip))

Parameters
:    domain: domain name.
     function (net.socket, ip): callback function. The first param is the socket, the second param is the ip address in string.

Returns
:    null

See also
:    **-**   <a href="nt.createServer">net.createServer()</a>

#i2c module
##CONSTANT
:     i2c.SLOW,  i2c.TRANSMITTER, i2c. RECEIVER.  FAST（400k）is not supported for now.

<a name="ic.setup">
## i2c.setup()
Description
:    initialize i2c.

Syntax
:    i2c.setup(id, pinSDA, pinSCL, speed)

Parameters
:    id = 0
     pinSDA: 0~11，IO index
     pinSCL: 0~11，IO index
     speed:  i2c.SLOW

Returns
:    null

See also
:    **-**   <a href="ic.read">i2c.read()</a>

<a name="ic.start">
## i2c.start()
Description
:    start i2c transporting.

Syntax
:    i2c.start(id)

Parameters
:    id = 0

Returns
:    null

See also
:    **-**   <a href="ic.read">i2c.read()</a>

<a name="ic.stop">
## i2c.stop()
Description
:    stop i2c transporting.

Syntax
:    i2c.stop(id)

Parameters
:    id = 0

Returns
:    null

See also
:    **-**   <a href="ic.read">i2c.read()</a>

<a name="ic.address">
## i2c.address()
Description
:    setup i2c address and read/write mode.

Syntax
:    i2c.address(id, device_addr, direction)

Parameters
:    id=0
     device_addr: device address.
     direction: i2c.TRANSMITTER for writing mode , i2c. RECEIVER for reading mode

Returns
:    null

See also
:    **-**   <a href="ic.read">i2c.read()</a>

<a name="ic.write">
## i2c.write()
Description
:    write data to i2c, data can be multi numbers, string or lua table.

Syntax
:    i2c.write(id, data1, data2,...)

Parameters
:    id=0
     data: data can be numbers, string or lua table.

Returns
:    null

Example
:    ****
```
    i2c.write(0, "hello", "world")
```

See also
:    **-**   <a href="ic.read">i2c.read()</a>

<a name="ic.read">
## i2c.read()
Description
:    read data for len bytes.

Syntax
:    i2c.read(id, len)

Parameters
:    id=0
     len: data length

Returns
:    string:data received.

Example
:    ****
```
    id=0
    sda=1
    scl=0

    //initialize i2c, set pin1 as sda, set pin0 as scl
    i2c.setup(id,sda,scl,i2c.SLOW)

    //user defined function: read from reg_addr content of dev_addr
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

    //get content of register 0xAA of device 0x77
    reg = read_reg(0x77, 0xAA)
    pirnt(string.byte(reg))

```

See also
:    **-**   <a href="ic.write">i2c.write()</a>
