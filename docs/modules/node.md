# node Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [TerryE](https://github.com/TerryE) | [node.c](../../app/modules/node.c)|

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

Deprecated synonym for [`node.LFS.get()`](#nodelfsget) to return an LFS function reference.

Note that this returns `nil` if the function does not exist in LFS. 

## node.flashreload()

Deprecated synonym for [`node.LFS.reload()`](#nodelfsreload) to reload [LFS (Lua Flash Store)](../lfs.md) with the named flash image provided.

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

## node.getpartitiontable()

Get the current LFS and SPIFFS partition information.

#### Syntax
`node.getpartitiontable()`

#### Parameters
none

#### Returns
An array containing entries for `lfs_addr`, `lfs_size`, `spiffs_addr` and `spiffs_size`. The address values are offsets relative to the start of the Flash memory.

#### Example
```lua
print("The LFS size is " .. node.getpartitiontable().lfs_size)
```

#### See also
[`node.setpartitiontable()`](#nodesetpartitiontable)


## node.heap()

Returns the current available heap size in bytes. Note that due to fragmentation, actual allocations of this size may not be possible.

#### Syntax
`node.heap()`

#### Parameters
none

#### Returns
system heap size left in bytes (number)

## node.info()

Returns information about hardware, software version and build configuration.

#### Syntax
`node.info([group])`

#### Parameters
`group` A designator for a group of properties. May be one of `"hw"`, `"sw_version"`, `"build_config"`. It is currently optional; if omitted the legacy structure is returned. However, not providing any value is deprecated.

#### Returns
If a `group` is given the return value will be a table containing the following elements:

- for `group` = `"hw"`
	- `chip_id` (number)
	- `flash_id` (number)
	- `flash_size` (number)
	- `flash_mode` (number) 0 = QIO, 1 = QOUT, 2 = DIO, 15 = DOUT.
	- `flash_speed` (number)

- for `group` = `"lfs"`
	- `lfs_base` (number)	Flash offset of selected LFS region 
	- `lfs_mapped` (number)	Mapped memory address of selected LFS region
	- `lfs_size` (number)	size of selected LFS region
	- `lfs_used` (number)	actual size used by current LFS image

- for `group` = `"sw_version"`
	- `git_branch` (string)
	- `git_commit_id` (string)
	- `git_release` (string) release name +additional commits e.g. "2.0.0-master_20170202 +403"
	- `git_commit_dts` (string) commit timestamp in an ordering format. e.g. "201908111200"
	- `node_version_major` (number)
	- `node_version_minor` (number)
	- `node_version_revision` (number)

- for `group` = `"build_config"`
	- `ssl` (boolean)
	- `lfs_size` (number) as defined at build time
	- `modules` (string) comma separated list
	- `number_type` (string) `integer` or `float`

!!! attention

	Not providing a `group` is deprecated and support for that will be removed in one of the next releases.

- for `group` = `nil`
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

```lua
for k,v in pairs(node.info("build_config")) do
  print (k,v)
end
```

```lua
print(node.info("sw_version").git_release)
```


## node.input()

Submits a string to the Lua interpreter. Similar to `pcall(loadstring(str))`, but without the single-line limitation.  Note that the Line interpreter only actions complete Lua chunks.  A Lue Lua chunk must comprise one or more complete `'\n'` terminaed lines that form a complete compilation unit.

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
See the `telnet/telnet.lua` in `lua_examples` for a more comprehensive example.

#### See also
[`node.output()`](#nodeoutput)


## node.LFS

Sub-table containing the API for [Lua Flash Store](../lfs.md)(**LFS**) access.  Programmers might prefer to map this to a global or local variable for convenience for example:
```lua
local LFS = node.LFS
```
This table contains the following methods and properties:

Property/Method | Description
-------|---------
`config` | A synonym for [`node.info('lfs')`](#nodeinfo).  Returns the properties `lfs_base`, `lfs_mapped`, `lfs_size`, `lfs_used`.
`get()` | See [node.LFS.get()](#nodelfsget).
`list()` | See [node.LFS.list()](#nodelfslist).
`reload()` |See [node.LFS.reload()](#nodelfsreload).
`time` | Returns the Unix timestamp at time of image creation.


## node.LFS.get() 

Returns the function reference for a function in LFS.

Note that unused `node.LFS` properties map onto the equialent `get()` call so for example: `node.LFS.mySub1` is a synonym for `node.LFS.get('mySub1')`.

#### Syntax
`node.LFS.get(modulename)`

#### Parameters
`modulename`  The name of the module to be loaded.

#### Returns
-  If the LFS is loaded and the `modulename` is a string that is the name of a valid module in the LFS, then the function is returned in the same way the `load()` and the other Lua load functions do
-  Otherwise `nil` is returned.


## node.LFS.list() 

List the modules in LFS.


#### Returns
-  If no LFS image IS LOADED then `nil` is returned.
-  Otherwise an sorted array of the name of modules in LFS is returned.

## node.LFS.reload()

Reload LFS with the flash image provided. Flash images can be generated on the host machine using the `luac.cross`command.

#### Syntax
`node.LFS.reload(imageName)`

#### Parameters
`imageName` The name of a image file in the filesystem to be loaded into the LFS.

#### Returns
-  In the case when the `imagename` is a valid LFS image, this is expanded and loaded into flash, and the ESP is then immediately rebooted, _so control is not returned to the calling Lua application_ in the case of a successful reload.
-  The reload process internally makes multiple passes through the LFS image file. The first pass validates the file and header formats and detects many errors.  If any is detected then an error string is returned.


## node.output()

Redirects the Lua interpreter to a `stdout` pipe when a CB function is specified (See  `pipe` module) and resets output to normal otherwise. Optionally also prints to the serial console.

#### Syntax
`node.output(function(pipe), serial_debug)`

#### Parameters
  - `output_fn(pipe)` a function accept every output as str, and can send the output to a socket (or maybe a file). Note that this function must conform to the fules for a pipe reader callback.
  - `serial_debug` 1 output also show in serial. 0: no serial output.

#### Returns
`nil`

#### Example

See the `telnet/telnet.lua` in `lua_examples` for a more comprehensive example of its use.

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

## node.setonerror()

Overrides the default crash handling which always restarts the system. It can be used to e.g. write an error to a logfile or to secure connected hardware before restarting.

!!! attention

    It is strongly advised to ensure that the callback ends with a restart. Something has gone quite wrong and it is probably not safe to just wait for the next event (e.g., timer tick) and hope everything works out.

#### Syntax
`node.setonerror(function)`

#### Parameters
`function` a callback function to be executed when an error occurs, gets the error string as an argument, remember to **trigger a restart** at the end of the callback

#### Returns
`nil`

#### Example
```lua
node.setonerror(function(s)
    print("Error: "..s)
    node.restart()
  end)
```


## node.setpartitiontable()

Sets the current LFS and / or SPIFFS partition information.

#### Syntax
`node.setpartitiontable(partition_info)`

!!! note
	This function is typically only used once during initial provisioning after first flashing the firmware.  It does some consistency checks to validate the specified parameters, and it then reboots the ESP module to load the new partition table. If the LFS or SPIFFS regions have changed then you will need to reload LFS, reformat the SPIFSS and reload its contents.

#### Parameters
An array containing one or more of the following enties. The address values are byte offsets relative to the start of the Flash memory. The size values are in bytes. Note that these parameters must be a multiple of 8Kb to align to Flash page boundaries.
-  `lfs_addr`.  The base address of the LFS region.
-  `lfs_size`.  The size of the LFS region.
-  `spiffs_addr`. The base address of the SPIFFS region.
-  `spiffs_size`. The size of the SPIFFS region.

#### Returns
Not applicable.  The ESP module will be rebooted for a valid new set, or a Lua error will be thown if inconsistencies are detected.

#### Example
```lua
node.setpartitiontable{lfs_size = 0x20000, spiffs_addr = 0x120000, spiffs_size = 0x20000}
```

#### See also
[`node.getpartitiontable()`](#nodegetpartitiontable)



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

## node.startupcommand()

Overrides the default startup action on processor restart, preplacing the executing `init.lua` if it exists. This is now deprecated in favor of `node.startup({command="the command"})`.

#### Syntax
`node.startupcommand(string)`

#### Parameters

- `string` prefixed with either
	- `@`, the remaining string is a filename to be executed.
	- `=`, the remaining string is Lua chunk to be compiled and executed.

####  Returns
 	`status` this is `false` if write to the Reboot Config Record fails.  Note that no attempt is made to	parse or validate the string. If the command is invalid or the file missing then this will be reported on the next restart.

#### Example
```lua
node.startupcommand("@myappstart.lc")  -- Execute the compiled file myappstart.lc on startup
```

```lua
-- Execute the LFS routine init() in preference to init.lua
node.startupcommand("=if LFS.init then LFS.init() else dofile('init.lua') end")
```


## node.startupcounts()

Query the performance of system startup.

!!! Important

    This function is only available if the firmware is built with `PLATFORM_STARTUP_COUNT` defined. This would normally be done by uncommenting the `#define PLATFORM_STARTUP_COUNT` line in `app/include/user_config.h`.

#### Syntax
`node.startupcounts([marker])`

#### Parameters

- `marker` If present, this will add another entry into the startup counts

####  Returns
An array of tables which indicate how many CPU cycles had been consumed at each step of platform boot.

#### Example
```lua
=sjson.encode(node.startupcounts()) 
```

This might generate the output (formatted for readability):

```
[
 {"ccount":3774328,"name":"user_pre_init","line":124},
 {"ccount":3842297,"name":"user_pre_init","line":180},
 {"ccount":9849869,"name":"user_init","line":327},
 {"ccount":10008843,"name":"nodemcu_init","line":293},
 {"ccount":10295779,"name":"pmain","line":234},
 {"ccount":11378766,"name":"pmain","line":256},
 {"ccount":11565912,"name":"pmain","line":260},
 {"ccount":12158242,"name":"node_startup_counts","line":1},
 {"ccount":12425790,"name":"myspiffs_mount","line":126},
 {"ccount":12741862,"name":"myspiffs_mount","line":148},
 {"ccount":13983567,"name":"pmain","line":265}
]
```

The crucial entry is the one for `node_startup_counts` which is when the application had started running. This was on a Wemos D1 Mini with flash running at 80MHz. The startup options were all turned on. 
Note that the clock speed changes in `user_pre_init` to 160MHz. The total time was (approximately): `3.8 / 80 + (12 - 3.8) / 160 = 98ms`. With startup options of 0, the time is 166ms. These times may be slightly optimistic as the clock is only 52MHz for a time during boot.

## node.startup()

Get/set options that control the startup process. This interface will grow over time.

#### Syntax
`node.startup([{table}])`

#### Parameters

If the argument is omitted, then no change is made to the current set of startup options. If the argument is the empty table `{}` then all options are
reset to their default values.

- `table` one or more options:
    - `banner` - set to true or false to indicate whether the startup banner should be displayed or not. (default: true)
    - `frequency` - set to node.CPU80MHZ or node.CPU160MHZ to indicate the initial CPU speed. (default: node.CPU80MHZ)
    - `delay_mount` - set to true or false to indicate whether the SPIFFS filesystem mount is delayed until it is first needed or not. (default: false)
    - `command` - set to a string which is the initial command that is run. This is the same string as in the `node.startupcommand`.

####  Returns
`table` This is the complete set of options in the state that will take effect on the next boot. Note that the `command` key may be missing -- in which
        case the default value will be used.

#### Example
```lua
node.startup({banner=false, frequency=node.CPU160MHZ})  -- Prevent printing the banner and run at 160MHz
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
