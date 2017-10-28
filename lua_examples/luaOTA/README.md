## ESP8266 Lua OTA

Espressif use an optional update approach for their firmware know as OTA (over the air).  
This module offers an equivalent facility for Lua applications developers, and enables 
module development and production updates by carrying out automatic synchronisation 
with a named provisioning service at reboot.

### Overview

This `luaOTA` provisioning service uses a different approach to 
[enduser setup](https://nodemcu.readthedocs.io/en/dev/en/modules/enduser-setup/).
The basic concept here is that the ESP modules are configured with a pre-imaged file 
system that includes a number of files in the luaOTA namespace.  (SPIFFS doesn't 
implement a directory hierarchy as such, but instead simply treats the conventional
directory separator as a character in the filename.  Nonetheless, the "luaOTA/"
prefix serves to separate the lc files in the luaOTA namespace.) 

-  `luaOTA/check.lc`  This module should always be first executed at startup.
-  `luaOTA/_init.lc` 
-  `luaOTA/_doTick.lc`
-  `luaOTA/_provision.lc`

A fifth file `luaOTA/config.json` contains a JSON parameterised local configuration that
can be initially create by and subsequently updated by the provisioning process. Most
importantly this configuration contains the TCP address of the provisioning service, and
a shared secret that is used to sign any records exchanged between the ESP client and
the provisioning service. 

Under this approach, `init.lua` is still required but it is reduced to a one-line lua
call which invokes the `luaOTA` module by a `require "luaOTA.check"` statement.

The `config.json` file which provides the minimum configuration parameters to connect to
the WiFi and provisioning server, however these can by overridden through the UART by
first doing a `tmr.stop(0)` and then a manual initialisation as described in the
[init.lua](#initlua) section below.

`luaOTA` configures the wifi and connects to the required sid in STA mode using the
local configuration. The ESP's IP address is allocated using DHCP unless the optional
three static IP parameters have been configured. It then attempts to establish a
connection to the named provisioning service. If this is absent, a timeout occurs or the
service returns a "no update" status, then module does a full clean up of all the
`luaOTA` resources (if the `leave` parameter is false, then the wifi stack is then also
shutdown.), and it then transfers control by a `node.task.post()` to the configured
application module and function.

If `luaOTA` does establish a connection to IP address:port of the provisioning service,
it then issues a "getupdate" request using its CPU ID and a configuration parameter
block as context. This update dialogue uses a simple JSON protocol(described below) that
enables the provision server either to respond with a "no update", or to start a
dialogue to reprovision the ESP8266's SPIFFS.

In the case of "no update", `luaOTA` is by design ephemeral, that is it shuts down the
net services and does a full resource clean up. Hence the presence of the provisioning
service is entirely optional and it doesn't needed to be online during normal operation,
as `luaOTA` will fall back to transferring control to the main Lua application.

In the case of an active update, **the ESP is restarted** so resource cleanup on
completion is not an issue. The provisioning dialogue is signed, so the host
provisioning service and the protocol are trusted, with the provisioning service driving
the process.  This greatly simplifies the `luaOTA` client coding as this is a simple 
responder, which actions simple commands such as: 
- download a file, 
- download and compile file,
- upload a file
- rename (or delete) a file

with the ESP being rebooted on completion of the updates to the SPIFFS.  Hence in 
practice the ESP boots into one one two modes: 
- _normal execution_ or 
- _OTA update_ followed by reboot and normal execution.

Note that even though NodeMCU follows the Lua convention of using the `lua` and `lc`
extensions respectively for source files that need to be compiled, and for pre-compiled
files, the Lua loader itself only uses the presence of a binary header to determine the
file mode. Hence if the `init.lua` file contains pre-compiled content, and similarly all
loaded modules use pre-compiled lc files, then the ESP can run in production mode
_without needing to invoke the compiler at all_.

The simplest strategy for the host provisioning service is to maintain a reference
source directory on the host (per ESP module). The Lua developer can maintain this under
**git** or equivalent and make any changes there, so that synchronisation of the ESP
will be done automatically on reboot.

### init.lua

This is typically includes a single line: 
```Lua
require "LuaOTA.check"
```
however if the configuration is incomplete then this can be aborted as manual process 
by entering the manual command through the UART
```Lua
tmr.stop(0); require "luaOTA.check":_init {ssid ="SOMESID" --[[etc. ]]}
```
where the parameters to the `_init` method are: 

-  `ssid` and `spwd`.  The SSID of the Wifi service to connect to, together with its 
password.
-  `server` and `port`.  The name or IP address and port of the provisioning server.
-  `secret`.  A site-specific secret shared with the provisioning server for MD5-based 
signing of the protocol messages.
-  `leave`.  If true the STA service is left connected otherwise the wifi is shutdown
-  `espip`,`gw`,`nm`,`ns`.  These parameters are omitted if the ESP is using a DHCP 
service for IP configuration, otherwise you need to specify the ESP's IP, gateway, 
netmask and default nameserver.

If the global `DEBUG` is set, then LuaOTA will also dump out some diagnostic debug.

### luaOTA.check

This only has one public method: `_init` which can be called with the above parameters.
However the require wrapper in the check module also posts a call to `self:_init()` as a
new task.  This new task function includes a guard to prevent a double call in the case
where the binding require includes an explicit call to `_init()` 

Any provisioning changes results in a reboot, so the only normal "callback" is to invoke
the application entry method as defined in `config.json` using a `node.task.post()`


### luaOTAserver.lua

This is often tailored to specific project requirements, but a simple example of a
provisioning server is included which provides the corresponding server-side
functionality. This example is coded in Lua and can run on any development PC or server
that supports Lua 5.1 - 5.3 and the common modules `socket`, `lfs`, `md5` and `cjson`.
It can be easily be used as the basis of one for your specific project needs.

Note that even though this file is included in the `luaOTA` subdirectory within Lua
examples, this is designed to run on the host and should not be included in the
ESP SPIFFS.

## Implementation Notes

-  The NodeMCu build must include the following modules: `wifi`, `net`, `file`, `tmr`,
`crypto` and`sjason`. 

-  This implementation follow ephemeral practices, that it is coded to ensure that all
resources used are collected by the Lua GC, and hence the available heap on 
application start is the same as if luaOTA had not been called.

-  Methods in the `check` file are static and inherit self as an upvalue.

-  In order to run comfortably within ESP resources, luaOTA executes its main 
functionality as a number of overlay methods. These are loaded dynamically (and largely
transparently) by an `__index` metamethod.

   -  Methods starting with a "_" are call-once and return the function reference

   -  All others are also entered in the self table so that successive calls will use 
the preloaded function. The convention is that any dynamic function is called in object
form so they are loaded and executed with self as the first parameter, and hence are
called using the object form self:someFunc() to get the context as a parameter.

   -  Some common routines are also defined as closures within the dynamic methods

-  This coding also makes a lot of use of tailcalls (See PiL 6.3) to keep the stack size 
   to a minimum. 

-  The update process uses a master timer in `tmr` slot 0.  The index form is used here 
in preference to the object form because of the reduced memory footprint. This also
allows the developer to abort the process early in the boot sequence by issuing a
`tmr.stop(0)` through UART0.

-  The command protocol is unencrypted and uses JSON encoding, but all exchanges are 
signed by a 6 char signature taken extracted from a MD5 based digest across the JSON
string. Any command which fails the signature causes the update to be aborted. Commands
are therefore regarded as trusted, and this simplifies the client module on the ESP.

-  The process can support both source and compiled code provisioning, but the latter 
is recommended as this permits a compile-free runtime for production use, and hence 
minimises the memory use and fragmentation that occurs as a consequence of compilation.

-  In earlier versions of the provisioning service example, I included `luaSrcDiet` but 
this changes the line numbering which I found real pain for debugging, so I now just
include a simple filter to remove "--" comments and leading and trailing whitespace if
the source includes a `--SAFETRIM` flag. This typically reduced the size of lua files
transferred by ~30% and this also means that developers have no excuse for not properly
commenting their code!

-  The chip ID is included in the configuration identification response to permit the 
provisioning service to support different variants for different ESP8266 chips.

-  The (optional update & reboot) operate model also has the side effect that the 
`LuaOTA` client can reprovision itself.

-  Though the simplest approach is always to do a `luaOTA.check` immediately on reboot, 
there are other strategies that could be applied, for example to test a gpio pin or a 
flag in RTC memory or even have the application call the require directly (assuming that 
there's enough free RAM for it to run and this way avoid the connection delay to the 
WiFi.

## Discussion on RAM usage

`luaOTA` also itself serves as a worked example of how to write ESP-friendly 
applications.

-  The functionality is divided into autoloaded processing chunks using a self 
autoloader, so that `self:somefunction()`  calls can load new code from flash in
a way that is simple and largely transparent to the application.  The autoloader 
preferentially loads the `lc` compiled code variant if available.

-  The local environment is maintained in a self array, to keep scoping explicit.  Note
that since loaded code cannot inherit upvalues, then `self` must be passed to the 
function using an object constructor `self:self:somefunction()`, but where the function
can have a self argument then the alternative is to use an upvalue binding.  See the 
`tmr` alarm call at the end of `_init.lua` as an example:
```Lua
  tmr.alarm(0, 500, tmr.ALARM_AUTO, self:_doTick())
```
-  The `self:_doTick()` is evaluated before the alarm API call.  This autoloads 
`luaOTA/_doTick.lc` which stores `self` as a local and returns a function which takes 
no arguments; it is this last returned function that is used as the timer callback,
and when this is called it can still access self as an upvalue.

-  This code makes a lot of use of locals and upvalues as these are both fast and use 
less memory footprint than globals or table entries.

-  The lua GC will mark and sweep to reclaim any unreferenced resources: tables, 
strings, functions, userdata.  So if your code at the end of a processing phase leaves
no variables (directly or indirectly) in _G or the Lua registry, then all of the 
resources that were loaded to carry out your application will be recovered by the GC. 
In this case heap at the end of a "no provisioning" path is less than 1Kb smaller than
if luaOTA had not been called and this is an artifact of how the lua_registry system 
adopts a lazy reuse of registry entries.

-  If you find that an enumeration of `debug.getregistry()` includes function references
or tables other than ROMtables, then you have not been tidying up by doing the 
appropriate closes or unregister calls.  Any such stuck resources can result in a 
stuck cascade due to upvalues being preserved in the function closure or entries in a 
table.
