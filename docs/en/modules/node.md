# node Module
The node module provides access to system-level features such as sleep, restart and various info and IDs.

## node.bootreason()

Returns the boot reason code.

This is the raw code, not the new "reset info" code which was introduced in recent SDKs. Values are:
  - 1: power-on
  - 2: reset (software?)
  - 3: hardware reset via reset pin
  - 4: WDT reset (watchdog timeout)

####Syntax
`node.bootreason()`

####Parameters
`nil`

####Returns
number:the boot reason code

####Example
```lua
rsn = node.bootreason()
```
___
## node.restart()

Restarts the chip.

####Syntax
`node.restart()`

####Parameters
`nil`

####Returns
`nil`

####Example
   
```lua
node.restart();
```
___
## node.dsleep()

Enter deep sleep mode, wake up when timed out.

The maximum sleep time is 4294967295us, ~71 minutes. This is an SDK limitation.
Firmware from before 05 Jan 2016 have a maximum sleeptime of ~35 minutes.

####Syntax
`node.dsleep(us, option)`

**Note:** This function can only be used in the condition that esp8266 PIN32(RST) and PIN8(XPD_DCDC aka GPIO16) are connected together. Using sleep(0) will set no wake up timer, connect a GPIO to pin RST, the chip will wake up by a falling-edge on pin RST.<br />
option=0, init data byte 108 is valuable;<br />
option>0, init data byte 108 is valueless.<br />
More details as follows:<br />
0, RF_CAL or not after deep-sleep wake up, depends on init data byte 108.<br />
1, RF_CAL after deep-sleep wake up, there will belarge current.<br />
2, no RF_CAL after deep-sleep wake up, there will only be small current.<br />
4, disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current.

####Parameters
 - `us`: number(Integer) or nil, sleep time in micro second. If us = 0, it will sleep forever. If us = nil, will not set sleep time.

 - `option`: number(Integer) or nil. If option = nil, it will use last alive setting as default option.

####Returns
`nil`

####Example

```lua
--do nothing
node.dsleep()
--sleep μs
node.dsleep(1000000)
--set sleep option, then sleep μs
node.dsleep(1000000, 4)
--set sleep option only
node.dsleep(nil,4)
```
___
## node.info()

Returns NodeMCU version, chipid, flashid, flash size, flash mode, flash speed.

####Syntax
`node.info()`

####Parameters
`nil`

####Returns
 - `majorVer` (number)
 - `minorVer` (number)
 - `devVer` (number)
 - `chipid` (number)
 - `flashid` (number)
 - `flashsize` (number)
 - `flashmode` (number)
 - `flashspeed` (number)

####Example

```lua
    majorVer, minorVer, devVer, chipid, flashid, flashsize, flashmode, flashspeed = node.info()
    print("NodeMCU "..majorVer.."."..minorVer.."."..devVer)
```
___
## node.chipid()

Returns the ESP chip ID.

####Syntax
`node.chipid()`

####Parameters
`nil`

####Returns
number:chip ID

####Example

```lua
    id = node.chipid();
```
___
## node.flashid()
Returns the flash chip ID.

####Syntax
`node.flashid()`

####Parameters
`nil`

####Returns
number:flash ID

####Example

```lua
    flashid = node.flashid();
```
___
## node.heap()
Returns the current available heap size in bytes. Note that due to fragmentation, actual allocations of this size may not be possible.

####Syntax
`node.heap()`

####Parameters
`nil`

####Returns
number: system heap size left in bytes

####Example

```lua
    heap_size = node.heap();
```
___
## node.key() --deprecated

Define action to take on button press (on the old devkit 0.9), button connected to GPIO 16.

This function is only available if the firmware was compiled with DEVKIT_VERSION_0_9 defined.


####Syntax
`node.key(type, function)`

####Parameters
  - `type`: type is either string "long" or "short". long: press the key for 3 seconds, short: press shortly(less than 3 seconds)
  - `function`: user defined function which is called when key is pressed. If nil, remove the user defined function. Default function: long: change LED blinking rate,  short: reset chip

####Returns
`nil`

####Example
```lua
    node.key("long", function() print('hello world') end)
```
####See also
  - `node.led()`

___
## node.led() --deprecated

Set the on/off time for the LED (on the old devkit 0.9), with the LED connected to GPIO16, multiplexed with `node.key()`.

This function is only available if the firmware was compiled with DEVKIT_VERSION_0_9 defined.

####Syntax
`node.led(low, high)`

####Parameters
  - `low`: LED off time, LED keeps on when low=0. Unit: milliseconds, time resolution: 80~100ms<br />
  - `high`: LED on time. Unit: milliseconds, time resolution: 80~100ms

####Returns
`nil`

####Example

```lua
    -- turn led on forever.
    node.led(0)
```

####See also
  - `node.key()`

___
## node.input()

Submit a string to the Lua interpreter. Similar to `pcall(loadstring(str))`, but without the single-line limitation.

!!! note "Note:"

  This function only has an effect when invoked from a callback. Using it directly on the console **does not work**.

####Syntax
`node.input(str)`

####Parameters
  - `str`: Lua chunk

####Returns
`nil`

####Example

```lua
    sk:on("receive", function(conn, payload) node.input(payload) end)
```

####See also
  - `node.output()`

___
## node.output()

Redirects the Lua interpreter output to a callback function. Optionally also prints it to the serial console.

!!! note "Note:"

  Do **not** attempt to `print()` or otherwise induce the Lua interpreter to produce output from within the callback function. Doing so results in infinite recursion, and leads to a watchdog-triggered restart.

####Syntax
`node.output(output_fn, serial_debug)`

####Parameters
  - `output_fn(str)`: a function accept every output as str, and can send the output to a socket (or maybe a file).
  - `serial_debug`: 1 output also show in serial. 0: no serial output.

####Returns
`nil`

####Example

```lua
    function tonet(str)
      sk:send(str)
    end
    node.output(tonet, 1)  -- serial also get the lua output.
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
  - `node.input()`

___
## node.readvdd33() --deprecated, moved to adc.readvdd33()
####See also
  - `adc.readvdd33()`

___
## node.compile()

Compile a Lua text file into Lua bytecode, and save it as .lc file.

####Syntax
`node.compile(filename)`

####Parameters
  - `filename`: name of Lua text file

####Returns
`nil`

####Example
```lua
  file.open("hello.lua","w+")
  file.writeline([[print("hello nodemcu")]])
  file.writeline([[print(node.heap())]])
  file.close()

  node.compile("hello.lua")
  dofile("hello.lua")
  dofile("hello.lc")
```
___
## node.setcpufreq()

Change the working CPU Frequency.

####Syntax
`node.setcpufreq(speed)`

####Parameters
  - `speed`: `node.CPU80MHZ` or `node.CPU160MHZ`

####Returns
number:target CPU Frequency

####Example

```lua
    node.setcpufreq(node.CPU80MHZ)
```
___
## node.restore()

Restore system configuration to defaults. Erases all stored WiFi settings, and resets the "esp init data" to the defaults. This function is intended as a last-resort without having to reflash the ESP altogether.

This also uses the SDK function `system_restore()`, which doesn't document precisely what it erases/restores.

####Syntax
`node.restore()`

####Parameters
`nil`

####Returns
`nil`

####Example

```lua
    node.restore()
    node.restart() -- ensure the restored settings take effect
```
___
## node.stripdebug()

Controls the amount of debug information kept during `node.compile()`, and
allows removal of debug information from already compiled Lua code.

Only recommended for advanced users, the NodeMCU defaults are fine for almost all use cases.

####Syntax
`node.stripdebug([level[, function]])``

####Parameters
  - `level`:
    - 1: don't discard debug info
    - 2: discard Local and Upvalue debug info
    - 3: discard Local, Upvalue and line-number debug info
  - function: a compiled function to be stripped per setfenv except 0 is not permitted.

If no arguments are given then the current default setting is returned. If function is omitted, this is the default setting for future compiles. The function argument uses the same rules as for `setfenv()`.


#### Returns
If invoked without arguments, returns the current level settings. Otherwise, `nil` is returned.

####Example
```lua
node.stripdebug(3)
node.compile('bigstuff.lua')
```

####See also
  - `node.compile()`

___
