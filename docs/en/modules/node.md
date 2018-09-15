# node Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [node.c](../../../app/modules/node.c)|

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

In case of extended reset cause 3 (exception reset), additional values are returned containing the crash information. These are, in order, [EXCCAUSE](https://arduino-esp8266.readthedocs.io/en/latest/exception_causes.html), EPC1, EPC2, EPC3, EXCVADDR, and DEPC.

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
chip ID (number)

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

Theoretical maximum deep sleep duration can be found with [`node.dsleepMax()`](#nodedsleepmax). ["Max deep sleep for ESP8266"](https://thingpulse.com/max-deep-sleep-for-esp8266/) claims the realistic maximum be around 3.5h.
 
!!! caution

    This function can only be used in the condition that esp8266 PIN32(RST) and PIN8(XPD_DCDC aka GPIO16) are connected together. Using sleep(0) will set no wake up timer, connect a GPIO to pin RST, the chip will wake up by a falling-edge on pin RST.

#### Syntax
`node.dsleep(us, option, instant)`

#### Parameters
 - `us` number (integer) or `nil`, sleep time in micro second. If `us == 0`, it will sleep forever. If `us == nil`, will not set sleep time.

 - `option` number (integer) or `nil`. If `nil`, it will use last alive setting as default option.
	- 0, init data byte 108 is valuable
	- \> 0, init data byte 108 is valueless
	- 0, RF_CAL or not after deep-sleep wake up, depends on init data byte 108
	- 1, RF_CAL after deep-sleep wake up, there will be large current
	- 2, no RF_CAL after deep-sleep wake up, there will only be small current
	- 4, disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current
 - `instant` number (integer) or `nil`. If present and non-zero, the chip will enter Deep-sleep immediately and will not wait for the Wi-Fi core to be shutdown.

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

#### See also
- [`wifi.suspend()`](wifi.md#wifisuspend)
- [`wifi.resume()`](wifi.md#wifiresume)
- [`node.sleep()`](#nodesleep)
- [`node.dsleepMax()`](#nodedsleepmax)

## node.dsleepMax()

Returns the current theoretical maximum deep sleep duration.

!!! caution

	While it is possible to specify a longer sleep time than the theoretical maximum sleep duration, it is not recommended to exceed this maximum. In tests documented at ["Max deep sleep for ESP8266"](https://thingpulse.com/max-deep-sleep-for-esp8266/) the device never woke up again if the specified sleep time was beyond `dsleepMax()`.


!!! note

	This theoretical maximum is dependent on ambient temperature: lower temp = shorter sleep duration, higher temp = longer sleep duration

#### Syntax
`node.dsleepMax()`

#### Parameters
 none
 
#### Returns
`max_duration`

#### Example
```lua
node.dsleep(node.dsleepMax())
```

#### See also
- [`node.dsleep()`](#nodedsleep)

## node.flashid()

Returns the flash chip ID.

#### Syntax
`node.flashid()`

#### Parameters
none

#### Returns
flash ID (number)

## node.flashindex()

Returns the function reference for a function in the [LFS (Lua Flash Store)](../lfs.md).

#### Syntax
`node.flashindex(modulename)`

#### Parameters
`modulename`  The name of the module to be loaded.  If this is `nil` or invalid then an info list is returned

#### Returns
-  In the case where the LFS in not loaded, `node.flashindex` evaluates to `nil`, followed by the flash and mapped base addresss of the LFS
-  If the LFS is loaded and the function is called with the name of a valid module in the LFS, then the function is returned in the same way the `load()` and the other Lua load functions do.
-  Otherwise an extended info list is returned: the Unix time of the LFS build, the flash and mapped base addresses of the LFS and its current length, and an array of the valid module names in the LFS.

#### Example

The `node.flashindex()` is a low level API call that is normally wrapped using standard Lua code to present a simpler application API.  See the module `_init.lua` in the `lua_examples/lfs` directory for an example of how to do this.

## node.flashreload()

Reload the [LFS (Lua Flash Store)](../lfs.md) with the flash image provided. Flash images are generated on the host machine using the `luac.cross`commnad.

#### Syntax
`node.flashreload(imageName)`

#### Parameters
`imageName` The of name of a image file in the filesystem to be loaded into the LFS.

#### Returns
If the LFS image has the incorrect signature or size, then `false` is returned.
In the case of the `imagename` being a valid LFS image, this is then loaded into flash.  The ESP is then immediately rebooted so control is not returned to the calling application. 


## node.flashsize()

Returns the flash chip size in bytes. On 4MB modules like ESP-12 the return value is 4194304 = 4096KB.

#### Syntax
`node.flashsize()`

#### Parameters
none

#### Returns
flash size in bytes (integer)

## node.getcpufreq()

Get the current CPU Frequency.

#### Syntax
`node.getcpufreq()`

#### Parameters
none

#### Returns
Current CPU frequency (number)

#### Example
```lua
do
  local cpuFreq = node.getcpufreq()
  print("The current CPU frequency is " .. cpuFreq .. " MHz")
end
```

## node.heap()

Returns the current available heap size in bytes. Note that due to fragmentation, actual allocations of this size may not be possible.

#### Syntax
`node.heap()`

#### Parameters
none

#### Returns
system heap size left in bytes (number)

## node.info()

Returns NodeMCU version, chipid, flashid, flash size, flash mode, flash speed, and Lua File Store (LFS) usage statics.

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

!!! attention

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

## node.output()

Redirects the Lua interpreter output to a callback function. Optionally also prints it to the serial console.

!!! caution

    Do **not** attempt to `print()` or otherwise induce the Lua interpreter to produce output from within the callback function. Doing so results in infinite recursion, and leads to a watchdog-triggered restart.

#### Syntax
`node.output(function(str), serial_debug)`

#### Parameters
  - `output_fn(str)` a function accept every output as str, and can send the output to a socket (or maybe a file).
  - `serial_debug` 1 output also show in serial. 0: no serial output.

#### Returns
`nil`

#### Example
```lua
function tonet(str)
  sk:send(str)
end
node.output(tonet, 1)  -- serial also get the Lua output.
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

Restores system configuration to defaults using the SDK function `system_restore()`, which is described in the documentation as:

> Reset default settings of following APIs: `wifi_station_set_auto_connect`, `wifi_set_phy_mode`, `wifi_softap_set_config` related, `wifi_station_set_config` related, `wifi_set_opmode`, and APs’ information recorded by `#define	AP_CACHE`.

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


## node.sleep()

Put NodeMCU in light sleep mode to reduce current consumption. 

* NodeMCU can not enter light sleep mode if wifi is suspended.
* All active timers will be suspended and then resumed when NodeMCU wakes from sleep. 

!!! attention
    This is disabled by default. Modify `PMSLEEP_ENABLE` in `app/include/user_config.h` to enable it.

#### Syntax
<!---`node.sleep({wake_pin[, duration, int_type, resume_cb, preserve_mode]})`--->
`node.sleep({wake_pin[, int_type, resume_cb, preserve_mode]})`

#### Parameters
<!--- timed light_sleep currently does not work, the 'duration' parameter is here as a place holder---> 
<!--- * `duration` Sleep duration in microseconds(μs). If a sleep duration of `0` is specified, suspension will be indefinite (Range: 0 or 50000 - 268435454 μs (0:4:28.000454))--->

* `wake_pin` 1-12, pin to attach wake interrupt to. Note that pin 0(GPIO 16) does not support interrupts. 
    <!---* If sleep duration is indefinite, `wake_pin` must be specified--->
    * Please refer to the [`GPIO module`](gpio.md) for more info on the pin map.
* `int_type` type of interrupt that you would like to wake on. (Optional, Default: `node.INT_LOW`)
    * valid interrupt modes:
        * `node.INT_UP`   Rising edge
        * `node.INT_DOWN` Falling edge
        * `node.INT_BOTH` Both edges
        * `node.INT_LOW`  Low level
        * `node.INT_HIGH` High level
* `resume_cb` Callback to execute when WiFi wakes from suspension. (Optional)
* `preserve_mode` preserve current WiFi mode through node sleep. (Optional, Default: true)  
    * If true, Station and StationAP modes will automatically reconnect to previously configured Access Point when NodeMCU resumes.
    * If false, discard WiFi mode and leave NodeMCU in `wifi.NULL_MODE`. WiFi mode will be restored to original mode on restart.

#### Returns
- `nil`

#### Example

```lua

--Put NodeMCU in light sleep mode indefinitely with resume callback and wake interrupt
 cfg={}
 cfg.wake_pin=3
 cfg.resume_cb=function() print("WiFi resume") end

 node.sleep(cfg)

--Put NodeMCU in light sleep mode with interrupt, resume callback and discard WiFi mode
 cfg={}
 cfg.wake_pin=3 --GPIO0
 cfg.resume_cb=function() print("WiFi resume") end
 cfg.preserve_mode=false

 node.sleep(cfg)
```
<!---
--Put NodeMCU in light sleep mode for 10 seconds with resume callback
 cfg={}
 cfg.duration=10*1000*1000
 cfg.resume_cb=function() print("WiFi resume") end

 node.sleep(cfg)
--->

#### See also
- [`wifi.suspend()`](wifi.md#wifisuspend)
- [`wifi.resume()`](wifi.md#wifiresume)
- [`node.dsleep()`](#nodedsleep)

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

## node.random()

This behaves like math.random except that it uses true random numbers derived from the ESP8266 hardware. It returns uniformly distributed
numbers in the required range. It also takes care to get large ranges correct. 

It can be called in three ways. Without arguments in the floating point build of NodeMCU, it returns a random real number with uniform distribution in the interval [0,1). 
When called with only one argument, an integer n, it returns an integer random number x such that 1 <= x <= n. For instance, you can simulate the result of a die with random(6). 
Finally, random can be called with two integer arguments, l and u, to get a pseudo-random integer x such that l <= x <= u.

#### Syntax
`node.random()`
`node.random(n)`
`node.random(l, u)`

#### Parameters
- `n` the number of distinct integer values that can be returned -- in the (inclusive) range 1 .. `n`
- `l` the lower bound of the range
- `u` the upper bound of the range

#### Returns
The random number in the appropriate range. Note that the zero argument form will always return 0 in the integer build.

#### Example
```lua
print ("I rolled a", node.random(6))
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
	- `node.egc.ON_MEM_LIMIT` Run the garbage collector when the memory used by the Lua script goes beyond an upper `limit`. If the upper limit can't be satisfied even after running the garbage collector, the allocator will return with error. If the given limit is negative, it is interpreted as the desired amount of heap which should be left available. Whenever the free heap (as reported by `node.heap()` falls below the requested limit, the garbage collector will be run.
	- `node.egc.ALWAYS` Run the garbage collector before each memory allocation. If the allocation fails even after running the garbage collector, the allocator will return with error. This mode is very efficient with regards to memory savings, but it's also the slowest.
- `level` in the case of `node.egc.ON_MEM_LIMIT`, this specifies the memory limit.
  
#### Returns
`nil`

#### Example

`node.egc.setmode(node.egc.ALWAYS, 4096)  -- This is the default setting at startup.`
`node.egc.setmode(node.egc.ON_ALLOC_FAILURE) -- This is the fastest activeEGC mode.`
`node.egc.setmode(node.egc.ON_MEM_LIMIT, 30720)  -- Only allow the Lua runtime to allocate at most 30k, collect garbage if limit is about to be hit`
`node.egc.setmode(node.egc.ON_MEM_LIMIT, -6144)  -- Try to keep at least 6k heap available for non-Lua use (e.g. network buffers)`


## node.egc.meminfo()

Returns memory usage information for the Lua runtime.

####Syntax
`total_allocated, estimated_used = node.egc.meminfo()`

#### Parameters
None.

#### Returns
 - `total_allocated` The total number of bytes allocated by the Lua runtime. This is the number which is relevant when using the `node.egc.ON_MEM_LIMIT` option with positive limit values.
 - `estimated_used` This value shows the estimated usage of the allocated memory.

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

