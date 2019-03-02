# node Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [node.c](../../components/modules/node.c)|

The node module provides access to system-level features such as sleep, restart and various info and IDs.

## node.bootreason()

Returns the boot reason and extended reset info.

The first value returned is the raw code, not the new "reset info" code which was introduced in recent SDKs. Values are:

  - 1, power-on
  - 2, reset (software?)
  - 3, hardware reset via reset pin
  - 4, WDT reset (watchdog timeout)

The second value returned is the extended reset cause. Values are:

  - 0, power-on
  - 1, hardware watchdog reset
  - 2, exception reset
  - 3, software watchdog reset
  - 4, software restart
  - 5, wake from deep sleep
  - 6, external reset

In general, the extended reset cause supercedes the raw code. The raw code is kept for backwards compatibility only. For new applications it is highly recommended to use the extended reset cause instead.

In case of extended reset cause 3 (exception reset), additional values are returned containing the crash information. These are, in order, EXCCAUSE, EPC1, EPC2, EPC3, EXCVADDR, and DEPC.

#### Syntax
`node.bootreason()`

#### Parameters
none

#### Returns
`rawcode, reason [, exccause, epc1, epc2, epc3, excvaddr, depc ]`

#### Example
```lua
_, reset_reason = node.bootreason()
if reset_reason == 0 then print("Power UP!") end
```

## node.chipid()

Returns the ESP chip ID.

#### Syntax
`node.chipid()`

#### Parameters
none

#### Returns
chip ID (string)

Note that due to the chip id being a much larger value on the ESP32, it is
reported as a string now. E.g. `"0x1818fe346a88"`.

## node.compile()

Compiles a Lua text file into Lua bytecode, and saves it as .lc file.

#### Syntax
`node.compile("file.lua")`

#### Parameters
`filename` name of Lua text file

#### Returns
`nil`

#### Example
```lua
file.open("hello.lua","w+")
file.writeline([[print("hello nodemcu")]])
file.writeline([[print(node.heap())]])
file.close()

node.compile("hello.lua")
dofile("hello.lua")
dofile("hello.lc")
```

## node.dsleep()

Enters deep sleep mode, wakes up when timed out.

The maximum sleep time is 4294967295us, ~71 minutes. This is an SDK limitation.
Firmware from before 05 Jan 2016 have a maximum sleeptime of ~35 minutes.

!!! note "Note:"

    This function can only be used in the condition that esp8266 PIN32(RST) and PIN8(XPD_DCDC aka GPIO16) are connected together. Using sleep(0) will set no wake up timer, connect a GPIO to pin RST, the chip will wake up by a falling-edge on pin RST.

#### Syntax
`node.dsleep(us, option)`

#### Parameters
 - `us` number (integer) or `nil`, sleep time in micro second. If `us == 0`, it will sleep forever. If `us == nil`, will not set sleep time.

 - `option` number (integer) or `nil`. If `nil`, it will use last alive setting as default option.
	- 0, init data byte 108 is valuable
	- \> 0, init data byte 108 is valueless
	- 0, RF_CAL or not after deep-sleep wake up, depends on init data byte 108
	- 1, RF_CAL after deep-sleep wake up, there will belarge current
	- 2, no RF_CAL after deep-sleep wake up, there will only be small current
	- 4, disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current

#### Returns
`nil`

#### Example
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

## node.flashid()

Returns the flash chip ID.

#### Syntax
`node.flashid()`

#### Parameters
none

#### Returns
flash ID (number)

## node.heap()

Returns the current available heap size in bytes. Note that due to fragmentation, actual allocations of this size may not be possible.

#### Syntax
`node.heap()`

#### Parameters
none

#### Returns
system heap size left in bytes (number)

## node.info()

Returns NodeMCU version, chipid, flashid, flash size, flash mode, flash speed.

#### Syntax
`node.info()`

#### Parameters
none

#### Returns
 - `majorVer` (number)
 - `minorVer` (number)
 - `devVer` (number)
 - `chipid` (number)
 - `flashid` (number)
 - `flashsize` (number)
 - `flashmode` (number)
 - `flashspeed` (number)

#### Example
```lua
majorVer, minorVer, devVer, chipid, flashid, flashsize, flashmode, flashspeed = node.info()
print("NodeMCU "..majorVer.."."..minorVer.."."..devVer)
```

## node.input()

Submits a string to the Lua interpreter. Similar to `pcall(loadstring(str))`, but without the single-line limitation.

!!! note "Note:"

    This function only has an effect when invoked from a callback. Using it directly on the console **does not work**.

#### Syntax
`node.input(str)`

#### Parameters
`str` Lua chunk

#### Returns
`nil`

#### Example
```lua
sk:on("receive", function(conn, payload) node.input(payload) end)
```

#### See also
[`node.output()`](#nodeoutput)

## node.key() --deprecated

Defines action to take on button press (on the old devkit 0.9), button connected to GPIO 16.

This function is only available if the firmware was compiled with DEVKIT_VERSION_0_9 defined.

#### Syntax
`node.key(type, function())`

#### Parameters
  - `type`: type is either string "long" or "short". long: press the key for 3 seconds, short: press shortly(less than 3 seconds)
  - `function`: user defined function which is called when key is pressed. If nil, remove the user defined function. Default function: long: change LED blinking rate,  short: reset chip

#### Returns
`nil`

#### Example
```lua
node.key("long", function() print('hello world') end)
```
#### See also
[`node.led()`](#nodeled-deprecated)

## node.led() --deprecated

Sets the on/off time for the LED (on the old devkit 0.9), with the LED connected to GPIO16, multiplexed with [`node.key()`](#nodekey-deprecated).

This function is only available if the firmware was compiled with DEVKIT_VERSION_0_9 defined.

#### Syntax
`node.led(low, high)`

#### Parameters
  - `low` LED off time, LED keeps on when low=0. Unit: milliseconds, time resolution: 80~100ms
  - `high` LED on time. Unit: milliseconds, time resolution: 80~100ms

#### Returns
`nil`

#### Example
```lua
-- turn led on forever.
node.led(0)
```

#### See also
[`node.key()`](#nodekey-deprecated)

## node.output()

Redirects the Lua interpreter output to a callback function. Optionally also prints it to the serial console.

!!! note "Note:"

    Do **not** attempt to `print()` or otherwise induce the Lua interpreter to produce output from within the callback function. Doing so results in infinite recursion, and leads to a watchdog-triggered restart.

#### Syntax
`node.output(function(str), serial_output)`

#### Parameters
  - `output_fn(str)` a function accept every output as str, and can send the output to a socket (or maybe a file). `nil` to unregister the previous function.
  - `serial_output` 1 output also sent out the serial port. 0: no serial output. Defaults to 1 if not specified.

#### Returns
`nil`

#### Example
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

```lua
-- disable all output completely
node.output(nil, 0)
```
#### See also
[`node.input()`](#nodeinput)

## node.readvdd33() --deprecated
Moved to [`adc.readvdd33()`](adc/#adcreadvdd33).

## node.restart()

Restarts the chip.

#### Syntax
`node.restart()`

#### Parameters
none

#### Returns
`nil`

## node.restore()

Restores system configuration to defaults using the SDK function `system_restore()`, which doesn't document precisely what it erases/restores.

#### Syntax
`node.restore()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
node.restore()
node.restart() -- ensure the restored settings take effect
```

## node.setcpufreq()

Change the working CPU Frequency.

#### Syntax
`node.setcpufreq(speed)`

#### Parameters
`speed` constant 'node.CPU80MHZ' or 'node.CPU160MHZ'

#### Returns
target CPU frequency (number)

#### Example
```lua
node.setcpufreq(node.CPU80MHZ)
```

## node.stripdebug()

Controls the amount of debug information kept during [`node.compile()`](#nodecompile), and allows removal of debug information from already compiled Lua code.

Only recommended for advanced users, the NodeMCU defaults are fine for almost all use cases.

####Syntax
`node.stripdebug([level[, function]])`

#### Parameters
- `level`
	- 1, don't discard debug info
	- 2, discard Local and Upvalue debug info
	- 3, discard Local, Upvalue and line-number debug info
- `function` a compiled function to be stripped per setfenv except 0 is not permitted.

If no arguments are given then the current default setting is returned. If function is omitted, this is the default setting for future compiles. The function argument uses the same rules as for `setfenv()`.

####  Returns
If invoked without arguments, returns the current level settings. Otherwise, `nil` is returned.

#### Example
```lua
node.stripdebug(3)
node.compile('bigstuff.lua')
```

#### See also
[`node.compile()`](#nodecompile)

## node.osprint()

Controls whether the debugging output from the Espressif SDK is printed. Note that this is only available if
the firmware is build with DEVELOPMENT_TOOLS defined.

####Syntax
`node.osprint(enabled)`

#### Parameters
- `enabled` This is either `true` to enable printing, or `false` to disable it. The default is `false`.

#### Returns
Nothing

#### Example
```lua
node.osprint(true)
```

## node.uptime()

Returns the value of the system counter, which counts in microseconds starting at 0 when the device is booted. Because a 32-bit signed integer can only fit 35 minutes' worth of microseconds before wrapping, when using integer-only 32-bit Lua this API returns 2 results, the first being the bottom 31 bits of the counter, and the second being the next 31 bits (ie bits 31 through 62 of the underlying 64-bit counter).

Therefore the first result will wrap back to 0 after about 35 minutes (2^31 microseconds), at which point the second result will increment by one. It is important to consider both values, or otherwise handle the first result wrapping, when for example using this function to [debounce or throttle GPIO input](https://github.com/hackhitchin/esp8266-co-uk/issues/2).

When using floating-point Lua, hundreds of years of microseconds can fit in a single value with no loss of precision. In such configurations, the first result will be the bottom 53 bits of the system counter as a floating-point number, and the second result will be the top 11 bits, which in practice will always be zero unless the system counter exceeds 285 years.

#### Syntax
`node.uptime()`

#### Parameters
none

#### Returns
Two results `lowbits, highbits`. The first is the time in microseconds since boot or the last time the counter wrapped, and the second is the number of times the counter has wrapped. Depending on whether Lua was compiled with floating-point support or not determines when the counter wraps.

#### Example
```lua
print(node.uptime())
print(node.uptime())

lowbits, highbits = node.uptime()
print(lowbits, "microseconds have elapsed since the uptime last wrapped")
print(string.format("Uptime has wrapped %d times", highbits))
```

# node.egc module

## node.egc.setmode()

Sets the Emergency Garbage Collector mode. [The EGC whitepaper](http://www.eluaproject.net/doc/v0.9/en_elua_egc.html)
provides more detailed information on the EGC.

####Syntax
`node.egc.setmode(mode, [param])`

#### Parameters
- `mode`
	- `node.egc.NOT_ACTIVE` EGC inactive, no collection cycle will be forced in low memory situations
	- `node.egc.ON_ALLOC_FAILURE` Try to allocate a new block of memory, and run the garbage collector if the allocation fails. If the allocation fails even after running the garbage collector, the allocator will return with error. 
	- `node.egc.ON_MEM_LIMIT` Run the garbage collector when the memory used by the Lua script goes beyond an upper `limit`. If the upper limit can't be satisfied even after running the garbage collector, the allocator will return with error.
	- `node.egc.ALWAYS` Run the garbage collector before each memory allocation. If the allocation fails even after running the garbage collector, the allocator will return with error. This mode is very efficient with regards to memory savings, but it's also the slowest.
- `level` in the case of `node.egc.ON_MEM_LIMIT`, this specifies the memory limit.
  
#### Returns
`nil`

#### Example

`node.egc.setmode(node.egc.ALWAYS, 4096)  -- This is the default setting at startup.`
`node.egc.setmode(node.egc.ON_ALLOC_FAILURE) -- This is the fastest activeEGC mode.`

# node.task module

## node.task.post()

Enable a Lua callback or task to post another task request. Note that as per the 
example multiple tasks can be posted in any task, but the highest priority is 
always delivered first.

If the task queue is full then a queue full error is raised.  

####Syntax
`node.task.post([task_priority], function)`

#### Parameters
- `task_priority` (optional)
	- `node.task.LOW_PRIORITY` = 0
	- `node.task.MEDIUM_PRIORITY` = 1
	- `node.task.HIGH_PRIORITY` = 2
- `function` a callback function to be executed when the task is run. 

If the priority is omitted then  this defaults  to `node.task.MEDIUM_PRIORITY`

####  Returns
`nil`

#### Example
```lua
for i = node.task.LOW_PRIORITY, node.task.HIGH_PRIORITY do 
  node.task.post(i,function(p2)
    print("priority is "..p2)
  end) 
end      
``` 
prints
```
priority is 2
priority is 1
priority is 0
```

