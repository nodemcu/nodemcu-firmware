# FAQ

*This FAQ was started by [Terry Ellison](https://github.com/TerryE) as an unofficial FAQ in mid 2015. This version as at April 2017 includes some significant rewrites.*

## What is this FAQ for?

This FAQ does not aim to help you to learn to program or even how to program in Lua. There are plenty of resources on the Internet for this, some of which are listed in [Where to start](#where-to-start). What this FAQ does is to answer some of the common questions that a competent Lua developer would ask in learning how to develop Lua applications for the ESP8266 based boards running the [NodeMcu firmware](https://github.com/nodemcu/nodemcu-firmware). This includes the NodeMCU Devkits. However, the scope of the firmware is far wider than this as it can be used on any ESP8266 module.

## What has changed since the first version of this FAQ?

The [NodeMCU company](http://NodeMCU.com/index_en.html) was set up by [Zeroday](zeroday@nodemcu.com) to develop and to market a set of Lua firmware-based development boards which employ the Espressif ESP8266 SoC. The initial development of the firmware was done by Zeroday and a colleague, Vowstar, in-house with the firmware being first open-sourced on Github in late 2014. In mid-2015, Zeroday decided to open the firmware development to a wider group of community developers, so the core group of developers now comprises 6 community developers (including this author), and we are also supported by another dozen or so active contributors, and two NodeMCU originators.
 
This larger active team has allowed us to address most of the outstanding issues present at the first version of this FAQ. These include:

-  For some time the project was locked into an old SDK version, but we now regularly rebaseline to the current SDK version.
-  Johny Mattsson's software exception handler and my LCD patch have allowed us to move the bulk of constant data out of RAM and into the firmware address space, and as a result current builds now typically boot with over 40Kb free RAM instead of 15Kb free and the code density is roughly 40% better.
-  We have  fixed error reporting so errors now correctly report line numbers in tracebacks.
-  We have addressed most of the various library resource leaks, so memory exhaustion is much less of an issue.
-  We have reimplemented the network stack natively over the now Open-sourced Espressif implementation of LwIP.
-  Thanks to a documentation effort lead by Marcel Stör, we now have a complete documentation online, and this FAQ forms a small part.
-  We have fixed various stability issues relating to the use of GPIO trigger callbacks.
-  Johny Mattsson is currently leading an ESP32 port.
-  We have a lot more hardware modules supported.

Because the development is active this list will no doubt continue to be revised and updated. See the [development README](https://github.com/nodemcu/nodemcu-firmware/blob/dev/README.md) for more details.

## Lua Language

### Where to start

The NodeMCU firmware implements Lua 5.1 over the Espressif SDK for its ESP8266 SoC and the IoT modules based on this.

- The official lua.org **[Lua Language specification](http://www.lua.org/manual/5.1/manual.html)** gives a terse but complete language specification.
- Its [FAQ](http://www.lua.org/faq.html) provides information on Lua availability and licensing issues.
- The **[unofficial Lua FAQ](http://www.luafaq.org/)** provides a lot of useful Q and A content, and is extremely useful for those learning Lua as a second language.
- The [Lua User's Wiki](http://lua-users.org/wiki/) gives useful example source and relevant discussion. In particular, its [Lua Learning Lua](http://lua-users.org/wiki/Learning) section is a good place to start learning Lua.
- The best book to learn Lua is *Programming in Lua- by Roberto Ierusalimschy, one of the creators of Lua. It's first edition is available free [online](http://www.lua.org/pil/contents.html) . The second edition was aimed at Lua 5.1, but is out of print. The third edition is still in print and available in paperback. It contains a lot more material and clearly identifies Lua 5.1 vs Lua 5.2 differences. **This third edition is widely available for purchase and probably the best value for money**. References of the format [PiL **n.m**] refer to section **n.m** in this edition.
- The Espressif ESP8266 architecture is closed source, but the Espressif SDK itself is continually being updated so the best way to get the documentation for this is to [google Espressif IoT SDK Programming Guide](https://www.google.co.uk/search?q=Espressif+IoT+SDK+Programming+Guide) or to look at the Espressif [downloads forum](http://bbs.espressif.com/viewforum.php?f=5).
 
### How is NodeMCU Lua different to standard Lua?

Whilst the Lua standard distribution includes a stand-alone Lua interpreter, Lua itself is primarily an *extension language* that makes no assumptions about a "main" program: Lua works embedded in a host application to provide a powerful, lightweight scripting language for use within the application. This host application can then invoke functions to execute a piece of Lua code, can write and read Lua variables, and can register C functions to be called by Lua code. Through the use of C functions, Lua can be augmented to cope with a wide range of different domains, thus creating customized programming languages sharing a syntactical framework.

The ESP8266 was designed and is fabricated in China by [Espressif Systems](http://espressif.com/new-sdk-release/). Espressif have also developed and released a companion software development kit (SDK) to enable developers to build practical IoT applications for the ESP8266. The SDK is made freely available to developers in the form of binary libraries and SDK documentation. However this is in a *closed format*, with no developer access to the source files, so anyone developing ESP8266 applications must rely solely on the SDK API (and the somewhat Spartan SDK API documentation). (Note that for the ESP32, Espressif have moved to an open-source approach for its ESP-IDF.)

The NodeMCU Lua firmware is an ESP8266 application and must therefore be layered over the ESP8266 SDK. However, the hooks and features of Lua enable it to be seamlessly integrated without losing any of the standard Lua language features. The firmware has replaced some standard Lua modules that don't align well with the SDK structure with ESP8266-specific versions. For example, the standard  `io` and `os` libraries don't work, but have been largely replaced by the NodeMCU `node` and `file` libraries. The `debug` and `math` libraries have also been omitted to reduce the runtime footprint (`modulo` can be done via `%`, `power` via `^`). Note that the `io.write()` function described in Lua's [Simple I/O Model](https://www.lua.org/pil/21.1.html) is not replaced by the `file` library. To write to the same serial port that the `print(string)` function uses by default, use `uart.write(0,string)`.

NodeMCU Lua is based on [eLua](http://www.eluaproject.net/overview), a fully featured implementation of Lua 5.1 that has been optimized for embedded system development and execution to provide a scripting framework that can be used to deliver useful applications within the limited RAM and Flash memory resources of embedded processors such as the ESP8266. One of the main changes introduced in the eLua fork is to use read-only tables and constants wherever practical for library modules. On a typical build this approach reduces the RAM footprint by some 20-25KB and this makes a Lua implementation for the ESP8266 feasible. This technique is called LTR and this is documented in detail in an eLua technical paper: [Lua Tiny RAM](http://www.eluaproject.net/doc/master/en_arch_ltr.html).

The main impacts of the ESP8266 SDK and together with its hardware resource limitations are not in the Lua language implementation itself, but in how *application programmers must approach developing and structuring their applications*. As discussed in detail below, the SDK is non-preemptive and event driven. Tasks can be associated with given events by using the SDK API to registering callback functions to the corresponding events. Events are queued internally within the SDK, and it then calls the associated tasks one at a time, with each task returning control to the SDK on completion. *The SDK states that if any tasks run for more than 15 mSec, then services such as WiFi can fail.*

The NodeMCU libraries act as C wrappers around registered Lua callback functions to enable these to be used as SDK tasks. **You must therefore use an Event-driven programming style in writing your ESP8266 Lua programs**. Most programmers are used to writing in a procedural style where there is a clear single flow of execution, and the program interfaces to operating system services by a set of synchronous API calls to do network I/O, etc. Whilst the logic of each individual task is procedural, this is not how you code up ESP8266 applications.

## ESP8266 Specifics

### How is coding for the ESP8266 the same as standard Lua?

- This is a fully featured Lua 5.1 implementation so all standard Lua language constructs and data types work.
- The main standard Lua libraries -- `core`, `coroutine`, `string` and `table` are implemented.

### How is coding for the ESP8266 different to standard Lua?

The ESP8266 uses a combination of on-chip RAM and off-chip Flash memory connected using a dedicated SPI interface. Code can be executed directly from Flash-mapped address space. In fact the ESP hardware actually executes code in RAM, and in the case of Flash-mapped addresses it executes this code from a RAM-based L1 cache which maps onto the Flash addresses. If the addressed line is in the cache then the code runs at full clock speed, but if not then the hardware transparently handles the address fault by first copying the code from Flash to RAM. This is largely transparent in terms of programming ESP8266 applications, though the faulting access runs at SRAM speeds and this code runs perhaps 13× slower than already cached code. The Lua firmware largely runs out of Flash, but even so, both the RAM and the Flash memory are *very- limited when compared to systems that most application programmers use.

Over the last two years, both the Espressif non-OS SDK developers and the NodeMCU team have made a range of improvements and optimisations to increase the amount of RAM available to developers, from a typical 15Kb or so with Version 0.9 builds to some 45Kb with the current firmware Version 2.x builds. See the [ESP8266 Non-OS SDK API Reference](https://espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf) for more details on the SDK.

The early ESP8266 modules were typically configured with 512Kb Flash. Fitting a fully featured Lua build with a number of optional libraries and still enough usable Flash to hold a Lua application needs a careful selection of libraries and features. The current NodeMCU firmware will fit comfortably in a 1Mb Flash and still have ample remaining Flash memory to support Lua IoT applications.

The NodeMCU firmware makes any unused Flash memory available as a [SPI Flash File System (SPIFFS)](https://github.com/pellepl/spiffs) through the `file` library. The SPIFFS file system is designed for SPI NOR flash devices on embedded targets, and is optimised for static wear levelling and low RAM footprint. For further details, see the link. How much Flash is available as SPIFFS file space depends on the number of modules included in the specific firmware build.

The firmware has a wide range of libraries available to support common hardware options. Including any library will increase both the code and RAM size of the build, so our recommended practice is for application developers to choose a custom build that only includes the library that are needed for your application and hardware variants. The developers that don't want to bother with setting up their own build environment can use Marcel Stör's excellent [Cloud build service](http://nodemcu-build.com) instead.

There are also further tailoring options available, for example you can choose to have a firmware build which uses 32-bit integer arithmetic instead of floating point. Our integer builds have a smaller Flash footprint and execute faster, but working in integer also has a number of pitfalls, so our general recommendation is to use floating point builds.

Unlike Arduino or ESP8266 development, where each application change requires the flashing of a new copy of the firmware, in the case of Lua the firmware is normally flashed once, and all application development is done by updating files on the SPIFFS file system. In this respect, Lua development on the ESP8266 is far more like developing applications on a more traditional PC. The firmware will only be reflashed if the developer wants to add or update one or more of the hardware-related libraries. 

Those developers who are used to dealing in MB or GB of RAM and file systems can easily run out of memory resources, but with care and using some of the techniques discussed below can go a long way to mitigate this.

The ESP8266 runs the SDK over the native hardware, so there is no underlying operating system to capture errors and to provide graceful failure modes. Hence system or application errors can easily "PANIC" the system causing it to reboot. Error handling has been kept simple to save on the limited code space, and this exacerbates this tendency. Running out of a system resource such as RAM will invariably cause a messy failure and system reboot.

Note that in the 3 years since the firmware was first developed, Espressif has developed and released a new RTOS alternative to the non-OS SDK, and and the latest version of the SDK API reference recommends using RTOS. Unfortunately, the richer RTOS has a significantly larger RAM footprint. Whilst our port to the ESP-32 (with its significantly larger RAM) uses the [ESP-IDF](https://github.com/espressif/esp-idf) which is based on RTOS, the ESP8266 RTOS versions don't have enough free RAM for a RTOS-based NodeMCU firmware build to have sufficient free RAM to write usable applications. 

There is currently no `debug` library support. So you have to use 1980s-style "binary-chop" to locate errors and use print statement diagnostics though the system's UART interface. (This omission was largely because of the Flash memory footprint of this library, but there is no reason in principle why we couldn't make this library available in the near future as a custom build option).

The LTR implementation means that you can't extend standard libraries as easily as you can in normal Lua, so for example an attempt to define `function table.pack()` will cause a runtime error because you can't write to the global `table`. Standard sand-boxing techniques can be used to achieve the same effect by using metatable based inheritance, but if you choose this option, then you need to be aware of the potential runtime and RAM impacts of this approach.

There are standard libraries to provide access to the various hardware options supported by the hardware: WiFi, GPIO, One-wire, I²C, SPI, ADC, PWM, UART, etc.

The runtime system runs in interactive-mode. In this mode it first executes any `init.lua` script. It then "listens" to the serial port for input Lua chunks, and executes them once syntactically complete. 

There is no batch support, although automated embedded processing is normally achieved by setting up the necessary event triggers in the [`init.lua`](../upload/#initlua) script.

The various libraries (`net`, `tmr`, `wifi`, etc.) use the SDK callback mechanism to bind Lua processing to individual events (for example a timer alarm firing). Developers should make full use of these events to keep Lua execution sequences short.

Non-Lua processing (e.g. network functions) will usually only take place once the current Lua chunk has completed execution. So any network calls should be viewed at an asynchronous request. A common coding mistake is to assume that they are synchronous, that is if two `socket:send()` are on consecutive lines in a Lua programme, then the first has completed by the time the second is executed. This is wrong. A `socket:send()` request simply queues the send task for dispatch by the SDK. This task can't start to process until the Lua code has returned to is calling C function to allow this running task to exit. Stacking up such requests in a single Lua task function burns scarce RAM and can trigger a PANIC. This is true for timer, network, and other callbacks. It is even the case for actions such as requesting a system restart, as can be seen by the following example which will print twenty "not quite yet" messages before restarting.

```lua
node.restart(); for i = 1, 20 do print("not quite yet -- ",i); end
```
    
You, therefore, **have to** implement ESP8266 Lua applications using an event driven approach. You have to understand which SDK API requests schedule asynchronous processing, and which define event actions through Lua callbacks. Yes, such an event-driven approach makes it difficult to develop procedurally structured applications, but it is well suited to developing the sorts of application that you will typically want to implement on an IoT device.

### So how does the SDK event / tasking system work in Lua?

-  The SDK uses a small number of Interrupt Service Routines (ISRs) to handle short time critical hardware interrupt related processing. These are very short duration and can interrupt a running task for up to 10µSec. (Modifying these ISRs or adding new ones is not a viable options for most developers.)
-  All other service and application processing is split into code execution blocks, known as **tasks**. The individual tasks are executed one at a time and run to completion. No task can never pre-empt another.
-  Runnable tasks are queued in one of three priority queues and the SDK contains a simple scheduler which executes queued tasks FIFO within priority. The high priority queue is used for hardware-related task, the middle for timer and event-driven tasks and the low priority queue for all other tasks.
-  It is important to keep task times as short as practical so that the overall system can work smoothly and responsively. The general recommendation is to keep medium priority tasks under 2mSec and low priority tasks under 15 mSec in duration. This is a guideline, and your application _might_ work stably if you exceed this, but you might also start to experience intermittent problems because of internal timeout within the WiFi and network services, etc..
-  If tasks take longer than 500mSec then the watchdog timer will reset the processor. This watchdog can be reset at an application level using the [`tmr.wdclr()`](modules/tmr/#tmrwdclr) function, but this should be avoided.
-  Application tasks can disable interrupts to prevent an ISR interrupting a time-critical code section,  The SDK guideline is that system ISRs might overrun if such critical code section last more than 10µSec. This means that such disabling can only be done within hardware-related library modules, written in C; it is not available at a Lua application level.
-  The SDK provide a C API for interfacing to it; this includes a set of functions for declaring application functions (written in C) as callbacks to associate application tasks with specific hardware and timer events, and their execution will be interleaved with the SDKs Wifi and Network processing tasks.

In essence, the NodeMCU firmware is a C application which exploits the ability of Lua to execute as a embedded language and runtime to mirror this structure at a Lua scripting level. All of the complexities of, and interface to, the SDK and the hardware are wrapped in firmware libraries which translate the appropriate calls into the corresponding Lua API.

- The SDK invokes a startup hook within the firmware on boot-up. This firmware code initialises the Lua environment and then attempts to execute the Lua module `init.lua` from the SPIFFS file system. This `init.lua`  module can then be used to do any application initialisation required and to call the necessary timer alarms or library calls to bind and callback routines to implement the tasks needed in response to any system events.
- By default, the Lua runtime also 'listens' to UART 0, the serial port, in interactive mode and will execute any Lua commands input through this serial port. Using the serial port in this way is the most common method of developing and debugging Lua applications on the ESP8266/
- The Lua libraries provide a set of functions for declaring application functions (written in Lua) as callbacks (which are stored in the [Lua registry](#so-how-is-the-lua-registry-used-and-why-is-this-important)) to associate application tasks with specific hardware and timer events. These are also non-preemptive at an applications level. The Lua libraries work in consort with the SDK to queue pending events and invoke any registered Lua callback routines, which then run to completion uninterrupted. For example the Lua [`mytimer:alarm(interval, repeat, callback)`](modules/tmr/#tmralarm) calls a function in the `tmr` library which registers a C function for this alarm using the SDK, and when this C alarm callback function is called it then in turn invokes the Lua callback.
- Excessively long-running Lua functions (or Lua code chunks executed at the interactive prompt through UART 0) can cause other system functions and services to timeout, or to allocate scarce RAM resources to buffer queued data, which can then trigger either the watchdog timer or memory exhaustion, both of which will ultimately cause the system to reboot.
- Just like their C counterparts, Lua tasks initiated by timer, network, GPIO and other callbacks run non pre-emptively to completion before the next task can run, and this includes SDK tasks. Printing to the default serial port is done by the Lua runtime libraries, but SDK services including even a reboot request are run as individual tasks. This is why in the previous example printout out twenty copies of "not quite yet --" before completing and return control the SDK which then allows the reboot to occur. 

This event-driven approach is very different to a conventional procedural applications written in Lua, and different from how you develop C sketches and applications for the Arduino architectures. _There is little point in constructing poll loops in your NodeMCU Lua code since almost always the event that you are polling will not be delivered by the SDK until after your Lua code returns control to the SDK._  The most robust and efficient approach to coding ESP8266 Lua applications is to embrace this event model paradigm, and to decompose your application into atomic tasks that are threaded by events which themselves initiate callback functions. Each event task is established by a callback in an API call in an earlier task. 

Understanding how the system executes your code can help you structure it better and improve both performance and memory usage. 

- _If you are not using timers and other callback, then you are using the wrong approach._

- _If you are using poll loops, then you are using the wrong approach._

- _If you are executing more an a few hundred lines of Lua per callback, then you are using the wrong approach._

### So what Lua library functions enable the registration of Lua callbacks?

SDK Callbacks include:

| Lua Module | Functions which define or remove callbacks |
|------------|--------------------------------------------|
| tmr        | `register([id,] interval, mode, function())` |
| node       | `task.post([task_priority], function)`, `output(function(str), serial_debug)` | 
| wifi       | `startsmart(chan, function())`, `sta.getap(function(table))` |
| net.server | `sk:listen(port,[ip],function(socket))`  |
| net        | `sk:on(event, function(socket, [, data]))`, `sk:send(string, function(sent))`, `sk:dns(domain, function(socket,ip))` |
| gpio       | `trig(pin, type, function(level))`  |
| mqtt       | `client:m:on(event, function(conn[, topic, data])`   |
| uart       | `uart.on(event, cnt, [function(data)], [run_input])`  |

For a comprehensive list refer to the module documentation on this site.

### So what are the different ways of declaring variables and how is NodeMCU different here?

The following is all standard Lua and is explained in detail in PiL etc., but it is worth summarising here because understanding this is of particular importance in the NodeMCU environment. 

All variables in Lua can be classed as globals, locals or upvalues. But by default any variable that is referenced and not previously declared as `local` is **global** and this variable will persist in the global table until it is explicitly deleted. If you want to see what global variables are in scope then try

```Lua
for k,v in pairs(_G) do print(k,v) end
```

Local variables are 'lexically scoped', and you may declare any variables as local within nested blocks or functions without affecting the enclosing scope. Because locals are lexically scoped you can also refer to local variables in an outer scope and these are still accessible within the inner scope. Such variables are know as **upvalues**.

Lua variable can be assigned two broad types of data: **values** such as numbers, booleans, and strings and **references** such as functions, tables and userdata. You can see the difference here  when you assign the contents of a variable `a` to `b`. In the case of a value then it is simply copied into `b`. In the case of a reference, both `a` and `b` now refer to the *same object*, and no copying of content takes place. This process of referencing can have some counter-intuitive consequences. For example, in the following code by the time it exists, the variable `tmr2func` is out of scope. However a reference to the function has now been stored in the Lua registry by the alarm API call, so it and any upvalues that it uses will persist until it is eventually entirely dereferenced (e.g. by `tmr2:unregister()`).

```Lua 
do
  local tmr2func = function() ds.convert_T(true); tmr1:start() end
  tmr2:alarm(300000, tmr.ALARM_AUTO, tmr2func)
end
```

You need to understand the difference between when a function is compiled, when it is bound as a closure and when it is invoked at runtime. The closure is normally bound once pretty much immediately after compile, but this isn't necessarily the case. Consider the following example from my MCP23008 module below.

```Lua 
-- Bind the read and write functions for commonly accessed registers
for reg, regAddr in pairs {
  IODOR = 0x00,
  GPPU = 0x06, -- Pull-up resistors register for MCP23008
  GPIO = 0x09,
  OLAT = 0x0A,
} do
  dev['write' .. reg] = function(o, dataByte)
    write(MCP23008addr, regAddr, dataByte)
  end
  dev['read' .. reg] = function(o)
    return read(MCP23008addr, regAddr)
  end
end
``` 

This loop is compiled once when the module is required.  The opcode vectors for the read and write functions are created during the compile, along with a header which defines how many upvalues and locals are used by each function. However, these two functions are then bound _four_ times as different functions (e.g. `mcp23008.writeIODOR()`) and each closure inherits its own copies of the upvalues it uses so the `regAddr` for this function is `0x00`). The upvalue list is created when the closure is created and through some Lua magic, even if the outer routine that initially declared them is no longer in scope and has been GCed (Garbage Collected), the Lua RTS ensures that any upvalue will still persist whilst the closure persists.

On the other hand the storage for any locals is allocated each time the routine is called, and this can be many times in a running application.

The Lua runtime uses hashed key access internally to retrieve keyed data from a table. On the other hand locals and upvalues are stored as a contiguous vector and are accessed directly by an index, which is a lot faster. In NodeMCU Lua accesses to Firmware-based tables is particularly slow, which is why you will often see statements like the following at the beginning of modules. *Using locals and upvalues this way is both a lot faster at runtime and generates less bytecode instructions for their access.*

```Lua 
local i2c = i2c
local i2c_start, i2c_stop, i2c_address, i2c_read, i2c_write, i2c_TRANSMITTER, i2c_RECEIVER =
i2c.start, i2c.stop, i2c.address, i2c.read, i2c.write, i2c.TRANSMITTER, i2c.RECEIVER
``` 

### So how is context passed between Lua event tasks?

It is important to understand that a single Lua function is associated with / bound to any event callback task. This function is executed from within the relevant NodeMCU library C code using a `lua_call()`. Even system initialisation which executes the `dofile("init.lua")` is really a special case of this. Each function can invoke other functions and so on, but it must ultimately return control to the C library code which then returns control the SDK, terminating the task. 

By their very nature Lua `local` variables only exist within the context of an executing Lua function, and so locals are unreferenced on exit and any local data (unless also a reference type such as a function, table, or user data which is also referenced elsewhere) can therefore be garbage collected between these `lua_call()` actions. 

So context can only be passed between event routines by one of the following mechanisms:

- **Globals** are by nature globally accessible. Any global will persist until explicitly dereferenced by assigning `nil` to it. Globals can be readily enumerated, e.g. by a `for k,v in pairs(_G) do`, so their use is transparent.
- The **File system** is a special case of persistent global, so there is no reason in principle why it and the files it contains can't be used to pass context. However the ESP8266 file system uses flash memory and even with the SPIFFS file system still has a limited write cycle lifetime, so it is best to avoid using the file system to store frequently changing content except as a mechanism of last resort.
- The **Lua Registry**. This is a normally hidden table used by the library modules to store callback functions and other Lua data types. The GC treats the registry as in scope and hence any content referenced in the registry will not be garbage collected. 
- **Upvalues**. These are a standard feature of Lua as described above that is fully implemented in NodeMCU. When a function is declared within an outer function, all of the local variables within the outer scope are available to the inner function. Ierusalimschy's paper, [Closures in Lua](http://www.cs.tufts.edu/~nr/cs257/archive/roberto-ierusalimschy/closures-draft.pdf), gives a lot more detail for those that want to dig deeper.

### So how is the Lua Registry used and why is this important?

All Lua callbacks are called by C wrapper functions within the NodeMCU libraries that are themselves callbacks that have been activated by the SDK as a result of a given event. Such C wrapper functions themselves frequently need to store state for passing between calls or to other wrapper C functions. The Lua registry is a special Lua table which is used for this purpose, except that it is hidden from direct Lua access, but using a standard Lua table for this store enables standard garbage collection algorithms to operate on its content. Any content that needs to be saved is created with a unique key. The upvalues for functions that are global or referenced in the Lua Registry will persist between event routines, and hence any upvalues used by them will also persist and can be used for passing context.

If you are running out of memory, then you might not be correctly clearing down Registry entries. One example is as above where you are setting up timers but not unregistering them. Another occurs in the following code fragment. The `on()` function passes the socket to the connection callback as it's first argument `sck`. This is local variable in the callback function, and it also references the same socket as the upvalue `srv`. So functionally `srv` and `sck` are interchangeable. So why pass it as an argument?  Normally garbage collecting a socket will automatically unregister any of its callbacks, but if you use a socket as an upvalue in the callback, the socket is now referenced through the Register, and now it won't be GCed because it is referenced. Catch-22 and a programming error, not a bug. 

Example of wrong upvalue usage in the callback:

```Lua
srv:on("connection", function(sck, c)
  svr:send(reply) -- should be 'sck' instead of 'srv'
end)
```

Examples of correct callback implementations can be found in the [net socket documentation](modules/net.md#netsocketon).

One way to check the registry is to use the construct `for k,v in pairs(debug.getregistry()) do print (k,v) end` to track the registry size. If this is growing then you've got a leak.

### How do I track globals

- See the Unofficial Lua FAQ: [Detecting Undefined Variables](http://lua-users.org/wiki/DetectingUndefinedVariables).
- My approach is to avoid using them unless I have a _very_ good reason to justify this. I track them statically by running a `luac -p -l XXX.lua | grep GLOBAL` filter on any new modules and replace any accidental globals by local or upvalued local declarations. 
- On NodeMCU, _G's metatable is _G, so you can create any globals that you need and then 'close the barn door' by assigning
`_G.__newindex=function(g,k,v) error ("attempting to set global "..k.." to "..v) end` and any attempt to create new globals with now throw an error and give you a traceback of where this has happened.

### Why is it importance to understand how upvalues are implemented when programming for the ESP8266?

The use of upvalues is a core Lua feature. This is explained in detail in PiL. Any Lua routines defined within an outer scope my use them. This can include routines directly or indirectly referenced in the globals table, **_G**, or in the Lua Registry. 

The number of upvalues associated with a given routine is calculated during compile and a stack vector is allocated for them when the closure is bound to hold these references. Each upvalues is classed as open or closed. All upvalues are initially open which means that the upvalue references back to the outer function's register set. However, upvalues must be able to outlive the scope of the outer routine where they are declared as a local variable. The runtime VM does this by adding extra checks when executing a function return to scan any defined closures within its scope for back references and allocate memory to hold the upvalue and points the upvalue's reference to this. This is known as a closed upvalue. 

This processing is a mature part of the Lua 5.x runtime system, and for normal Lua applications development this "behind-the-scenes" magic ensures that upvalues just work as any programmer might expect. Sufficient garbage collector metadata is also stored so that these hidden values will be garbage collected correctly *when properly dereferenced*.

One further complication is that some library functions don't implicitly dereference expired callback references and as a result their upvalues may not be garbage collected and this application error can be be manifested as a memory leak. So using upvalues can cause more frequent and difficult to diagnose PANICs during testing. So my general recommendation is still to stick to globals during initial development, and explicitly dereference resources by setting them to `nil` when you have done with them.

### Can I encapsulate actions such as sending an email in a Lua function?

Think about the implications of these last few answers.

An action such as composing and sending an email involves a message dialogue with a mail server over TCP. This in turn requires calling multiple API calls to the SDK and your Lua code must return control to the C calling library for this to be scheduled, otherwise these requests will just queue up, you'll run out of RAM and your application will PANIC. Hence it is simply **impossible** to write a Lua module so that you can do something like:

```lua
-- prepare message
status = mail.send(to, subject, body)
-- move on to next phase of processing.
```

But you could code up a event-driven task to do this and pass it a callback to be executed on completion of the mail send, something along the lines of the following. Note that since this involves a lot of asynchronous processing and which therefore won't take place until you've returned control to the calling library C code, you will typically execute this as the last step in a function and therefore this is best done as a tailcall [PiL 6.3].

```lua
-- prepare message
local ms = require("mail_sender")
return ms.send(to, subject, body, function(status)
  loadfile("process_next.lua")(status)
end)
```

Building an application on the ESP8266 is a bit like threading pearls onto a necklace. Each pearl is an event task which must be small enough to run within its RAM resources and the string is the variable context that links the pearls together.

### When and why should I avoid using tmr.delay()?

If you are used coding in a procedural paradigm then it is understandable that you consider using [`tmr.delay()`](modules/tmr.md#tmrdelay) to time sequence your application. However as discussed in the previous section, with NodeMCU Lua you are coding in an event-driven paradigm. 

If you look at the `app/modules/tmr.c` code for this function, then you will see that it executes a low level  `ets_delay_us(delay)`. This function isn't part of the NodeMCU code or the SDK; it's actually part of the xtensa-lx106 boot ROM, and is a simple timing loop which polls against the internal CPU clock. `tmr.delay()` is really intended to be used where you need to have more precise timing control on an external hardware I/O (e.g. lifting a GPIO pin high for 20  μSec). It does this with interrupts enabled, because so there is no guarantee that the delay will be as requested, and the Lua RTS itself may inject operations such as GC, so if you do this level of precise control then you should encode your application as a C library. 

It will achieve no functional purpose in pretty much every other usecase, as any other system code-based activity will be blocked from execution; at worst it will break your application and create hard-to-diagnose timeout errors. We therefore deprecate its general use.

### How do I avoid a PANIC loop in init.lua?

Most of us have fallen into the trap of creating an `init.lua` that has a bug in it, which then causes the system to reboot and hence gets stuck in a reboot loop. If you haven't then you probably will do so at least once.

When this happens, the only robust solution is to reflash the firmware.

The simplest way to avoid having to do this is to keep the `init.lua` as simple as possible -- say configure the wifi and then start your app using a one-time `tmr.alarm()` after a 2-3 sec delay. This delay is long enough to issue a `file.remove("init.lua")` through the serial port and recover control that way.

Another trick is to poll a spare GPIO input pin in your startup. I do this on my boards by taking this GPIO plus Vcc to a jumper on the board, so that I can set the jumper to jump into debug mode or reprovision the software.

Also it is always best to test any new `init.lua` by creating it as `init_test.lua`, say, and manually issuing a `dofile("init_test.lua")` through the serial port, and then only rename it when you are certain it is working as you require.

See ["Uploading code" → init.lua](upload.md#initlua) for a very detaild example.

## Compiling and Debugging

We recommend that you install Lua 5.1 on your development host. This often is useful for debugging Lua fragments on your PC. You also use it for compile validation.

You can also build `luac.cross` on your development host if you have Lua locally installed. This runs on your host and has all of the features of standard `luac`, except that the output code file will run under NodeMCU as an _lc_ file.

## Techniques for Reducing RAM and SPIFFS footprint

### How do I minimise the footprint of an application?

Perhaps the simplest aspect of reducing the footprint of an application is to get its scope correct. The ESP8266 is an IoT device and not a general purpose system. It is typically used to attach real-world monitors, controls, etc. to an intranet and is therefore designed to implement functions that have limited scope. We commonly come across developers who are trying to treat the ESP8266 as a general purpose device and can't understand why their application can't run. 

The simplest and safest way to use IoT devices is to control them through a dedicated general purpose system on the same network. This could be a low cost system such as a [RaspberryPi (RPi)](https://www.raspberrypi.org/) server, running your custom code or an open source home automation (HA) application. Such systems have orders of magnitude more capacity than the ESP8266, for example the RPi has 2GB RAM and its SD card can be up to 32GB in capacity, and it can support the full range of USB-attached disk drives and other devices. It also runs a fully featured Linux OS, and has a rich selection of applications pre configured for it. There are plenty of alternative systems available in this under $50 price range, as well as proprietary HA systems which can cost 10-50 times more.

Using a tiered approach where all user access to the ESP8266 is passed through a controlling server means that the end-user interface (or smartphone connector), together with all of the associated validation and security can be implemented on a system designed to have the capacity to do this. This means that you can limit the scope of your ESP8266 application to a limited set of functions being sent to or responding to requests from this system.

_If you are trying to implement a user-interface or HTTP webserver in your ESP8266 then you are really abusing its intended purpose. When it comes to scoping your ESP8266 applications, the adage **K**eep **I**t **S**imple **S**tupid truly applies._

### How do I minimise the footprint of an application on the file system

- It is possible to write Lua code in a very compact format which is very dense in terms of functionality per KB of source code.
- However if you do this then you will also find it extremely difficult to debug or maintain your application.
- A good compromise is to use a tool such as [LuaSrcDiet](http://luaforge.net/projects/luasrcdiet/), which you can use to compact production code for downloading to the ESP8266:
  - Keep a master repository of your code on your PC or a cloud-based versioning repository such as [GitHub](https://github.com/)
  - Lay it out and comment it for ease of maintenance and debugging 
  - Use a package such as [Esplorer](https://github.com/4refr0nt/ESPlorer) to download modules that you are debugging and to test them.
  - Once the code is tested and stable, then compress it using LuaSrcDiet before downloading to the ESP8266. Doing this will reduce the code footprint on the SPIFFS by 2-3x. Also note that LuaSrcDiet has a mode which achieves perhaps 95% of the possible code compaction but which still preserves line numbering. This means that any line number-based error messages will still be usable.
-  Standard Lua compiled code includes a lot of debug information which almost doubles its RAM size. [node.stripdebug()](modules/node.md#nodestripdebug) can be used to change this default setting either to increase the debug information for a given module or to remove line number information to save a little more space. Using `node.compile()` to pre-compile any production code will remove all compiled code including error line info and so is not recommended except for stable production code where line numbers are not needed.


### How do I minimise the footprint of running application?

The Lua Garbage collector is very aggressive at scanning and recovering dead resources. It uses an incremental mark-and-sweep strategy which means that any data which is not ultimately referenced back to the Globals table, the Lua registry or in-scope local variables in the current Lua code will be collected.

Setting any variable to `nil` dereferences the previous context of that variable. (Note that reference-based variables such as tables, strings and functions can have multiple variables referencing the same object, but once the last reference has been set to `nil`, the collector will recover the storage.

Unlike other compile-on-load languages such as PHP, Lua compiled code is treated the same way as any other variable type when it comes to garbage collection and can be collected when fully dereferenced, so that the code-space can be reused.

The default garbage collection mode is very aggressive and results in a GC sweep after every allocation. See [node.egc.setmode()](modules/node/#nodeegcsetmode) for how to turn this down. `node.egc.setmode(node.egc.ON_MEM_LIMIT, 4096)` is a good compromise of performance and having enough free headboard.

Lua execution is intrinsically divided into separate event tasks with each bound to a Lua callback. This, when coupled with the strong dispose on dereference feature, means that it is very easy to structure your application using an classic technique which dates back to the 1950s known as Overlays.

Various approaches can be use to implement this. One is described by DP Whittaker in his [Massive memory optimization: flash functions](http://www.esp8266.com/viewtopic.php?f=19&t=1940) topic. Another is to use *volatile modules*. There are standard Lua templates for creating modules, but the `require()` library function creates a reference for the loaded module in the `package.loaded` table, and this reference prevents the module from being garbage collected. To make a module volatile, you should remove this reference to the loaded module by setting its corresponding entry in `package.loaded` to `nil`. You can't do this in the outermost level of the module (since the reference is only created once execution has returned from the module code), but you can do it in any module function, and typically an initialisation function for the module, as in the following example:

```lua
local s = net.createServer(net.TCP)
s:listen(80, function(c) require("connector").init(c) end)
```

`connector.lua` would be a standard module pattern except that the `M.init()` routine must include the lines

```lua
local M, module = {}, ......
function M.init(csocket)
  package.loaded[module] = nil...
end

return M
```
This approach ensures that the module can be fully dereferenced on completion. OK, in this case, this also means that the module has to be reloaded on each TCP connection to port 80; however, loading a compiled module from SPIFFS only takes a few mSec, so surely this is an acceptable overhead if it enables you to break down your application into RAM-sized chunks. Note that `require()` will automatically search for `connector.lc` followed by `connector.lua`, so the code will work for both source and compiled variants. 
- Whilst the general practice is for a module to return a table, [PiL 15.1] suggests that it is sometimes appropriate to return a single function instead as this avoids the memory overhead of an additional table. This pattern would look as follows:

```lua
local s = net.createServer(net.TCP)
s:listen(80, function(c) require("connector")(c) end)
```
  
```lua
local module = _ -- this is a situation where using an upvalue is essential!
return function(csocket)
  package.loaded[module] = nil
  module = nil...
end
```

Also note that you should **not** normally code this up listener call as the following because the RAM now has to accommodate both the module which creates the server _and_ the connector logic.

```lua
...
local s = net.createServer(net.TCP)
local connector = require("connector") -- don't do this unless you've got the RAM available!
s:listen(80, connector)
```

### How do I reduce the size of my compiled code?

Note that there are two methods of saving compiled Lua to SPIFFS:

- The first is to use `node.compile()` on the `.lua` source file, which generates the equivalent bytecode `.lc` file. This approach strips out all the debug line and variable information.
- The second is to use `loadfile()` to load the source file into memory, followed by `string.dump()` to convert it in-memory to a serialised load format which can then be written back to a `.lc` file. The amount of debug saved will depend on the [node.stripdebug()](modules/node.md#nodestripdebug) settings.

The memory footprint of the bytecode created by method (3) is the same as when executing source files directly, but the footprint of bytecode created by method (2) is typically 10% smaller than a dump with the stripdebug level of 3 or 60% smaller than a dump with a  stripdebug level of 1, because the debug information is almost as large as the code itself.

In general consider method (2) if you have stable production code that you want to run in as low a RAM footprint as possible. Yes, method (3) can be used if you are still debugging, but you will probably be changing this code quite frequently, so it is easier to stick with `.lua` files for code that you are still developing.

Note that if you use `require("XXX")` to load your code then this will automatically search for `XXX.lc` then `XXX.lua` so you don't need to include the conditional logic to load the bytecode version if it exists, falling back to the source version otherwise.

### How do I get a feel for how much memory my functions use?

You should get an overall understanding of the VM model if you want to make good use of the limited resources available to Lua applications. An essential reference here is [A No Frills Introduction to Lua 5.1 VM Instructions](http://luaforge.net/docman/83/98/ANoFrillsIntroToLua51VMInstructions.pdf) . This explain how the code generator works, how much memory overhead is involved with each table, function, string etc..

You can't easily get a bytecode listing of your ESP8266 code; however there are two broad options for doing this:

- **Generate a bytecode listing on your development PC**. The Lua 5.1 code generator is basically the same on the PC and on the ESP8266, so whilst it isn't identical, using the standard Lua batch compiler `luac` against your source on your PC with the `-l -s` option will give you a good idea of what your code will generate. The main difference between these two variants is the size_t for ESP8266 is 4 bytes rather than the 8 bytes size_t found on modern 64bit development PCs; and the eLua variants generate different access references for ROM data types. If you want to see what the `string.dump()` version generates then drop the `-s` option to retain the debug information. You can also build `luac.cross` with this firmware and this generate lc code for the target ESP architecture.
- **Upload your `.lc` files to the PC and disassemble them there**. There are a number of Lua code disassemblers which can list off the compiled code that your application modules will generate, `if` you have a script to upload files from your ESP8266 to your development PC. I use [ChunkSpy](http://luaforge.net/projects/chunkspy/)  which can be downloaded [here](http://files.luaforge.net/releases/chunkspy/chunkspy/ChunkSpy-0.9.8/ChunkSpy-0.9.8.zip) , but you will need to apply the following patch so that ChunkSpy understands eLua data types:

```diff
 --- a/ChunkSpy-0.9.8/5.1/ChunkSpy.lua   2015-05-04 12:39:01.267975498 +0100
 +++ b/ChunkSpy-0.9.8/5.1/ChunkSpy.lua   2015-05-04 12:35:59.623983095 +0100
 @@ -2193,6 +2193,9 @@
            config.AUTO_DETECT = true
          elseif a == "--brief" then
            config.DISPLAY_BRIEF = true
 +        elseif a == "--elua" then
 +          config.LUA_TNUMBER = 5
 +          config.LUA_TSTRING = 6
          elseif a == "--interact" then
            perform = ChunkSpy_Interact
```

Your other great friend is to use `node.heap()` regularly through your code.

Use these tools and play with coding approaches to see how many instructions each typical line of code takes in your coding style. The Lua Wiki gives some general optimisation tips, but in general just remember that these focus on optimising for execution speed and you will be interested mainly in optimising for code and variable space as these are what consumes precious RAM.

### What is the cost of using functions?

Functions have fixed overheads, so in general the more that you group your application code into larger functions, then the less RAM used will be used overall. The main caveat here is that if you are starting to do "copy and paste" coding across functions then you are wasting resources. So of course you should still use functions to structure your code and encapsulate common repeated processing, but just bear in mind that each function definition has a relatively high overhead for its header record and stack frame. _So try to avoid overusing functions. If there are less than a dozen or so lines in the function then you should consider putting this code inline if it makes sense to do so._

### What other resources are available?

Install `lua` and `luac` on your development PC. This is freely available for Windows, Mac and Linux distributions, but we strongly suggest that you use Lua 5.1 to maintain source compatibility with ESP8266 code. This will allow you not only to unit test some modules on your PC in a rich development environment, but you can also use `luac` to generate a bytecode listing of your code and to validate new code syntactically before downloading to the ESP8266. This will also allow you to develop server-side applications and embedded applications in a common language. 

## Firmware and Lua app development

### How to reduce the size of the firmware?

We recommend that you use a tailored firmware build; one which only includes the modules that you plan to use in developing any Lua application. Once you have the ability to make and flash custom builds, the you also have the option of moving time sensitive or logic intensive code into your own custom module. Doing this can save a large amount of RAM as C code can be run directly from Flash memory. See [Building the firmware](../build/) for more details and options.

