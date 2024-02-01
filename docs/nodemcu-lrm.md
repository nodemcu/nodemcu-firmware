# NodeMCU Reference Manual

## Introduction

NodeMCU firmware is an IoT project ("the Project") which implements a Lua-based runtime for SoC modules based on the Espressif ESP8266 and ESP32 architectures.  This NodeMCU Reference Manual (**NRM**) specifically addresses how the NodeMCU Lua implementation relates to standard Lua as described in the two versions of the Lua language that we currently support:

-  The [Lua 5.1 Reference Manual](https://www.lua.org/manual/5.1/) and
-  The [Lua 5.3 Reference Manual](https://www.lua.org/manual/5.3/) (**LRM**)

Developers using the NodeMCU environment should familiarise themselves with the 5.3 LRM.

The Project provides a wide range of standard library modules written in both C and Lua to support many ESP hardware modules and chips, and these are documented in separate sections in our [online documentation](index.md).

The NRM supplements LRM content and module documentation by focusing on a complete description of the _differences_ between NodeMCU Lua and standard Lua 5.3 in use. It adopts the same structure and style as the LRM.  As NodeMCU provides a full implementation of the Lua language there is little content herein relating to Lua itself.  However, what NodeMCU does is to offer a number of enhancements that enable resources to be allocated in constant program memory &mdash; resources in standard Lua that are allocated in RAM; where this does impact is in the coding of C library modules and the APIs used to do this.  Hence the bulk of the differences relate to these APIs.

One of our goals in introducing Lua 5.3 support was to maintain the continuity of our existing C modules by ensuring that they can be successfully compiled and executed in both the Lua 5.1 and 5.3 environments.  This goal was achieved by a combination of:
-  enabling relevant compatibility options for standard Lua libraries;
-  back porting some Lua 5.3 API enhancements back into our Lua 5.1 implementation, and 
-  making some small changes to the module source to ensure that incompatible API use is avoided.

Further details are given in the [Lua compatibility](#lua-compatibility) section below.  Notwithstanding this, the Project has now deprecated Lua 5.1 and will soon be moving this version into frozen support.

As well as providing the ability to building runtime firmware environments for ESP chipsets, the Project also offers a `luac.cross` cross compiler that can be built for common platforms such as Windows 10 and Linux, and this enables developers to compile source modules into a binary file format for download to ESP targets and loading from those targets.
 
## Basic Concepts

The NodeNCU runtime offers a full implementation of the [LRM §2](https://www.lua.org/manual/5.3/manual.html#2) core concepts, with the following adjustments:

### Values and Types

-  The firmware is compiled to use 32-bit integers and single-precision (32-bit) floats by default.  Address pointers are also 32-bit, and this allows all Lua variables to be encoded in RAM as an 8-byte "`TValue`" (compared to the 12-byte `TValue` used in Lua 5.1).
-  C modules can statically declare read-only variant of the `Table` structure type known as a "`ROTable`".  There are some limitations to the types for ROTable keys and value, in order to ensure that these are consistent with static declaration.  ROTables are stored in code space (in flash memory on the ESPs), and hence do not take up RAM resources.  However these are still represented by the Lua type _table_ and a Lua application can treat them during execution the same as any other read-only table.  Any attempt to write to a `ROTable` or to set its metatable will throw an error.
-  NodeMCU also introduces a concept known as **Lua Flash Store (LFS)**.  This enables Lua code (and any string constants used in this code) to be compiled and stored in code space, and hence without using RAM resources.  Such LFS functions are still represented by the type _function_ and can be executed just like any other Lua function.

### Environments and the Global Environment

These are implemented as per the LRM.  Note that the Lua 5.1 and Lua 5.3 language implementations are different and can introduce breaking incompatibilities when moving between versions, but this is a Lua issue rather than a NodeMCU one.

### Garbage Collection

All LFS functions, any string constants used in these functions, and any ROTables are stored in static code space.  These are ignored by the Lua Garbage collector (LGC).  The LGC only scans RAM-based resources and recovers unused ones.  The NodeMCU LGC has slightly modified configuration settings that increase its aggressiveness as heap usage approaches RAM capacity.

### Coroutines

The firmware includes the full coroutine implementation, but note that there are some slight differences between the standard Lua 5.1 and Lua 5.3 C API implementations. (See [Feature breaks](#feature-breaks) below.)

## Lua Language

The NodeNCU runtime offers a full implementation of the Lua language as defined in [LRM §3](https://www.lua.org/manual/5.3/manual.html#3) and its subsections.

## The Application Program Interface

[LRM §4](https://www.lua.org/manual/5.3/manual.html#4) describes the C API for Lua.  This is used by NodeMCU modules written in the C source language to interface to the the Lua runtime.  The header file `lua.h` is used to define all API functions, together with their related types and constants.  This section 4 forms the primary reference, though the NodeMCU makes minor changes as detailed below to support LFS and ROTable resources.

### Error Handling in C

[LRM §4.6](https://www.lua.org/manual/5.3/manual.html#4.6) describes how errors are handled within the runtime. 

The normal practice within the runtime and C modules is to throw any detected errors -- that is to unroll the call stack until the error is acquired by a routine that has declared an error handler.  Such an environment can be established by  [lua_pcall](https://www.lua.org/manual/5.3/manual.html#lua_pcall) and related API functions within C and by the Lua function [pcall](https://www.lua.org/manual/5.3/manual.html#pdf-pcall); this is known as a _protected environment_.  Errors which occur outside any protected environment are not caught by the Lua application and by default trigger a "panic". By default NodeMCU captures the error traceback and posts a new SDK task to print the error before restarting the processor.

The NodeMCU runtime implements a non-blocking threaded model that is similar to that of `node.js`, and hence most Lua execution is initiated from event-triggered callback (CB) routines within C library modules.  NodeMCU enables full recovery of error diagnostics from otherwise unprotected Lua execution by adding an additional auxiliary library function [`luaL_pcallx`](#luaL_pcallx).  All event-driven Lua callbacks within our library modules use `luaL_pcallx` instead of `lua_call`.  This has the same behaviour if no uncaught error occurs.  However, in the case of an error that would have previously resulted in a panic, a new SDK task is posted with the error traceback as an upvalue to process this error information.

The default action is to  print a full traceback and then trigger processor restart, that is a similar outcome as before but with recovery of the full error traceback.  However the `node.onerror()` library function is available to override this default action; for example developers might wish to print the error without restarting the processor, so that the circumstances which triggered the error can be investigated.

### Additional API Functions and Types

These are available for developers coding new library modules in C.  Note that if you compare the standard and NodeMCU versions of `lua.h` you will find a small number of entries not listed below.  This is because the Lua 5.1 and Lua 5.3 variants are incompatible owing to architectural differences.  However, `laubxlib.h` includes equivalent wrapper version-compatible functions that may be used safely for both versions.

#### ROTable and ROTable_entry

Extra structure types used in `LROT` macros to declare static RO tables.  See detailed section below.

#### lua_createrotable

` void (lua_createrotable) (lua_State *L, ROTable *t, const ROTable_entry *e, ROTable *mt);`         [-0, +1, -]

Create a RAM based `ROTable` pointing to the `ROTable_entry` vector `e`, and metatable `mt`.

#### lua_debugbreak and ASSERT

`  void (lua_debugbreak)(void);`

`lua_debugbreak()` and `ASSERT(condition)` are available for development debugging.  If `DEVELOPMENT_USE_GDB` is defined then these will respectively trigger a debugger break and evaluate a conditional assert prologue on the same.  If not, then these are effectively ignored and generate no executable code.

#### lua_dump

` int lua_dump (lua_State *L, lua_Writer writer, void *data, int strip);`         [-0, +0, –]

Dumps function at the top of the stack function as a binary chunk as per LRM.  However the last argument `strip` is now an integer is in the range -1..2, rather a boolean as per standard Lua:

-  -1, use the current default strip level (which can be set by [`lua_stripdebug`](#lua_stripdebug))
-  0, keep all debug info
-  1, discard Local and Upvalue debug info; keep line number info
-  2, discard Local, Upvalue and line number debug info

The internal NodeMCU `Proto` encoding of debug line number information is typically 15× more compact than in standard Lua; the intermediate `strip=1` argument allows the removal of most of the debug information whilst retaining the ability to produce a proper line number traceback on error. 

#### lua_freeheap

` int lua_freeheap (void);`         [-0, +0, –]

returns the amount of free heap available to the Lua memory allocator.

#### lua_gc

`  int lua_gc (lua_State *L, int what, int data);`         [-0, +0, m]

provides an option for 
- `what`:
    - `LUA_GCSETMEMLIMIT`  sets the available heap threshold (in bytes) at which aggressive sweeping starts.

#### lua_getlfsconfig

`  void lua_getlfsconfig (lua_State *L, int *conf);`         [-0, +0, -]

if `conf` is not `NULL`, then this returns an int[5] summary of the LFS configuration.  The first 3 items are the mapped and flash address of the LFS region, and its allocated size.  If the LFS is loaded then the 4th is the current size used and the 5th (for Lua 5.3 only) the date-timestamp of loaded LFS image. 

#### lua_getstate

`  lua_State * lua_getstate();`         [-0, +0, -]

returns the main thread `lua_State` record.  Used in CBs to initialise `L` for subsequent API call use.

#### lua_pushrotable
`  void lua_pushrotable (lua_State *L, ROTable *p);`         [-0, +1, -]

Pushes a ROTable onto the stack.

#### lua_stripdebug
`  int lua_stripdebug (lua_State *L, int level);`         [-1, +0, e]

This function has two modes.  A value is popped off the stack.
-  If this value is `nil`, then the default strip level is set to given `level` if this is in the range 0 to 2. Returns the current default level.
-  If this value is a Lua function (in RAM rather than in LFS), then the prototype hierarchy within the function is stripped of debug information to the specified level. Returns an _approximate_ estimate of the heap freed (this can be a lot higher if some strings can be garbage collected).

#### lua_writestring

`  void lua_writestring(const char *s, size_t l); /* macro */`

Writes a string `s` of length `l` to `stdout`.  Note that any output redirection will be applied to the string.
 
#### lua_writestringerror

`  void lua_writestringerror(const char *s, void *p).  /* macro */`

Writes an error with CString format specifier `s` and parameter `p` to `stderr`.  Note on the ESP devices this error will always be sent to `UART0`; output redirection will not be applied.

### The Debug Interface

#### lua_pushstringsarray
`  int lua_pushstringsarray (lua_State *L, int opt);`         [-0, +1, m]

Pushes an array onto the stack containing all strings in the specified strings table. If `opt` = 0 then the RAM table is used, else if `opt` = 1 and LFS is loaded then the LFS  table is used, else `nil` is pushed onto the stack.

Returns a status boolean; true if a table has been pushed.

### Auxiliary Library Functions and Types 

Note that the LRM defines an auxiliary library which contains a set of functions that assist in coding convenience and economy.  These are strictly built on top of the basic API, and are defined in `lauxlib.h`.  By convention all auxiliary functions have the prefix `luaL_` so module code should only contain `lua_XXXX()` and `luaL_XXXX()` data declarations and functions.  And since the `lauxlib.h` itself incudes `lua.h`, all C modules should only need to `#include "lauxlib.h"` in their include preambles.

NodeMCU adds some extra auxiliary functions above those defined in the LRM.

#### luaL_lfsreload

`  int  lua_lfsreload (lua_State *L);`         [-1, +1, -]

This function pops the LFS image name from the stack, and if it exists and contains the correct image header then it reloads LFS with the specified image file, and immediately restarts with the new LFS image loaded, so control it not returned to the calling function. If the image is missing or the header is invalid then an error message is pushed onto the stack and control is returned.  Note that if the image has a valid header but its contents are invalid then the result is undetermined.

#### luaL_pcallx

`  int luaL_pcallx (lua_State *L, int narg, int nresults);`         [-(nargs + 1), +(nresults|1), –]

Calls a function in protected mode and providing a full traceback on error.

Both `nargs` and `nresults` have the same meaning as in [lua_call](https://www.lua.org/manual/5.3/manual.html#lua_call). If there are no errors during the call, then `luaL_pcallx` behaves exactly like `lua_call`. However, if there is any error, `lua_pcallx` has already established an traceback error handler for the call that catches the error. It cleans up the stack and returns the negative error code.

Any caught error is posted to a separate NodeMCU task which which calls the error reporter as defined in the registry entry `onerror` with the traceback text as its argument.  The default action is to print the error and then set a 1 sec one-shot timer to restart the CPU.  (One second is enough time to allow the error to be sent over the network if redirection to a telnet session is in place.)  If the `onerror` entry is set to `print` for example, then the error is simply printed without restarting the CPU.

Note that the Lua runtime does not call the error handler if the error is an out-of memory one, so in this case the out-of-memory error is posted to the error reporter without a traceback.

#### luaL_posttask

`  int luaL_posttask (lua_State* L, int prio);`         [-1, +0, e]

Posts a task to execute the function popped from the stack at the specified user task priority
-  `prio` one of:
   -  `LUA_TASK_LOW`
   -  `LUA_TASK_MEDIUM`
   -  `LUA_TASK_HIGH`

Note that the function is invoked with the priority as its parameter.

#### luaL_pushlfsmodule

`  int luaL_pushlfsmodule ((lua_State *L);`         [-1, +1, -]

This function pops a module name from the stack. If this is a string, LFS is loaded and it contains the named module then its closure is pushed onto the stack as a function value, otherwise `nil` is pushed.

Returns the type of the pushed value.

#### luaL_pushlfsmodules

`  int luaL_pushlfsmodules (lua_State *L);`         [-0, +1, m]

If LFS is loaded then an array of the names of all of the modules in LFS is pushed onto the stack as a function value, otherwise `nil` is pushed.

Returns the type of the pushed value.

#### luaL_pushlfsdts

`  int luaL_pushlfsdts (lua_State *L);`         [-0, +1, m-]

If LFS is loaded then the Unix-style date-timestamp for the compile time of the image is pushed onto the stack as a integer value, otherwise `nil` is pushed.  Note that the primary use of this stamp is to act as a unique identifier for the image version.

Returns the type of the pushed value.

#### luaL_reref
`  void (luaL_reref) (lua_State *L, int t, int *ref);`         [-1, +0, m]

Variant of [luaL_ref](https://www.lua.org/manual/5.3/manual.html#luaL_ref).  If `*ref` is a valid reference in the table at index t, then this is replaced by the object at the top of the stack (and pops the object), otherwise it creates and returns a new reference using the `luaL_ref` algorithm.

#### luaL_rometatable

`  int (luaL_rometatable) (lua_State *L, const char* tname, const ROTable *p);`         [-0, +1, e]

Equivalent to `luaL_newmetatable()` for ROTable metatables.  Adds key / ROTable entry to the registry  `[tname] = p`, rather than using a new RAM table.

#### luaL_unref2

`  luaL_unref2(l,t,r)`

This macro executes `luaL_unref(L, t, r)` and then assigns `r = LUA_NOREF`.

#### luaL_totoggle

`  bool luaL_totoggle(lua_State *L, int idx)`

There are several ways of indicating a configuration toggle value:
  - The modern Lua way, with a boolean (`true`/`false`)
  - The "classic" Lua way, with `1`/`nil`
  - The "C" way, with `1`/`0`

When implementing C modules for NodeMCU and needing to indicate an on/off setting, the preference is to do it as a boolean. In the interest of ease of use on the other hand, it is however nice to also support the other styles. The `luaL_totoggle` function provides just that.

### Declaring modules and ROTables in NodeMCU

All NodeMCU C library modules should include the standard header "`module.h`".  This internally includes `lnodemcu.h` and these together provide the macros to enable declaration of NodeMCU modules and ROTables within them.  All ROtable support macros are either prefixed by `LRO_` (Lua Read Only) or in the case of table entries `LROT_`. 

#### NODEMCU_MODULE

` NODEMCU_MODULE(sectionname, libraryname, map, initfunc)`

This macro enables the module to be statically declared and linked in the `ROM` ROTable.  The global environment's metafield `__index` is set to the ROTable `ROM` hence any entries in the ROM table are resolved as read-only entries in the global environment.
-  `sectionname`. This is the linker section for the module and by convention this is the uppercased library name (e.g. `FILE`).  Behind the scenes `_module_selected` is appended to this section name if corresponding "use module" macro (e.g. `LUA_USE_MODULES_FILE`) is defined in the configuration.  Only the modules sections `*_module_selected` are linked into the firmware image and those not selected are ignored.
-  `libraryname`.  This is the name of the module (e.g. `file`) and is the key for the entry in the `ROM` ROTable.
-  `map`.  This is the ROTable defining the functions and constants for the module, and this is the corresponding value for the entry in the `ROM` ROTable.
-  `initfunc`.  If this is not NULL, it should be a valid C function and is call during Lua initialisation to carry out one-time initialisation of the module.

#### LROT_BEGIN and LROT_END

`  LROT_BEGIN(rt,mt,flags)`
`  LROT_END(rt,mt,flags)`
These macros start and end a ROTable definition.  The three parameters must be the same in both declarations.
-  `rt`.  ROTable name.
-  `mt`.  ROTable's metatable.  This should be of the form `LROT_TABLEREF(tablename)` if the metatable is used and `NULL` otherwise.
-  `flags`. The Lua VM table access routines use a `flags` field to short-circuit where the access needs to honour metamethods during access.  In the case of a static ROTable this flag bit mask must be declared statically during compile rather than cached dynamically at runtime.  Hence if the table is a metatable and it includes the metamethods `__index`, `__newindex`, `__gc`, `__mode`, `__len` or `__eq`, then the mask field should or-in the corresponding mask for example if `__index` is used then the flags should include `LROT_MASK_INDEX`.  Note that index and GC are a very common combination, so `LROT_MASK_GC_INDEX` is also defined to be `(LROT_MASK_GC | LROT_MASK_INDEX)`.

#### LROT_xxxx_ENTRY

ROTables only support static declaration of string keys and value types: C function, Lightweight userdata, Numeric, ROTable.  These are entries are declared by means of the  `LROT_FUNCENTRY`, `LROT_LUDENTRY`, `LROT_NUMENTRY`, `LROT_INTENTRY`, `LROT_FLOATENTRY` and `LROT_TABENTRY` macros.  All take two parameters: the name of the key and the value. For Lua 5.1 builds `LROT_NUMENTRY` and `LROT_INTENTRY` both generate a numeric `TValue`, but in the case of Lua 5.3 these are separate numeric subtypes so these macros generate the appropriate subtype.

Note that ROTable entries can be declared in any order except that keys starting with "`_`" must be declared at the head of the list.  This is a pragmatic constraint for runtime efficiency.  A lookaside cache is used to optimise key searches and results in a direct table probe in over 95% of ROTable accesses.  A table miss (that is the key doesn't exist) still requires a full scan of the list, and the main source of table misses are scans for metafield values.  Forcing these to be at the head of the ROTable allows the scan to abort on reading the first non-"`_`" key.

ROTables can still support other key and value types by using an index metamethod to point at an C index access function.  For example this technique is used in the `utf8` library to return `utf8.charpattern`

```C
LROT_BEGIN(utf8_meta, NULL, LROT_MASK_INDEX)
  LROT_FUNCENTRY( __index, utf8_lookup )
LROT_END(utf8_meta, NULL, LROT_MASK_INDEX)

LROT_BEGIN(utf8, LROT_TABLEREF(utf8_meta, 0)
  LROT_FUNCENTRY( offset, byteoffset )
  LROT_FUNCENTRY( codepoint, codepoint )
  LROT_FUNCENTRY( char, utfchar )
  LROT_FUNCENTRY( len, utflen )
  LROT_FUNCENTRY( codes, iter_codes )
LROT_END(utf8, LROT_TABLEREF(utf8_meta), 0)
```

Any reference to `utf8.charpattern` will call the `__index` method function (`utf8_lookup()`); this returns the UTF8 character pattern if the name equals `"charpattern"` and `nil` otherwise.  Hence `utf8` works as standard even though ROTables don't natively support a string value type.

### Standard Libraries

-  Basic Lua functions, coroutine support, Lua module support, string and table manipulation are as per the standard Lua implementation.  However, note that there are some breaking changes in the standard Lua string implementation as discussed in the LRM, e.g. the `\z` end-of-line separator; no string functions exhibit a CString behaviour (that is treat `"\0"` as a special character).
-  The modulus operator is implemented for string data types so `str % var` is a synonym for `string.format(str, var)` and `str % tbl` is a synonym for `string.format(str, table.unpack(tbl))`.  This python-like formatting functionality is a very common extension to the string library, but is awkward to implement with `string` being a `ROTable`.
-  The `string.dump()` `strip` parameter can take integer values 1,2,3 (the [`lua_stripdebug`](#lua_stripdebug) strip parameter + 1).  `false` is synonymous to `1`, `true` to `3` and omitted takes the default strip level.
-  The `string` library does not offer locale support. 
-  The 5.3 `math` library is expanded compared to the 5.1 one, and specifically:
    - Included: ` abs`, ` acos`, ` asin`, ` atan`, ` ceil`, ` cos`, ` deg`, ` exp`, ` tointeger`, ` floor`, ` fmod`, ` ult`, ` log`, ` max`, ` min`, ` modf`, ` rad`, ` random`, ` randomseed`, ` sin`, ` sqrt`, ` tan` and ` type`
    -  Not implemented: ` atan2`, ` cosh`, ` sinh`, ` tanh`, ` pow`, ` frexp`, ` ldexp` and ` log10`
 -  Input, output OS Facilities (the `io` and `os` libraries) are not implement for firmware builds because of the minimal OS supported offered by the embedded run-time.  The separately documented `file` and `node` libraries provide functionally similar analogues.  The host execution environment implemented by `luac.cross` does support the `io` and `os` libraries.
-  The full `debug` library is implemented less the `debug.debug()` function.
    -  An extra function `debug.getstrings(type)` has been added; `type` is one of `'ROM'`, or `'RAM'` (the default). Returns a sorted array of the strings returned from the [`lua_getstrings`](#lua_getstrings) function.

## Lua compatibility

Standard Lua has a number of breaking incompatibilities that require conditional code to enable modules using these features to be compiled against both Lua 5.1 and Lua 5.3.  See [Lua 5.2 §8](https://www.lua.org/manual/5.2/manual.html#8) and [Lua 5.3 §8](https://www.lua.org/manual/5.3/manual.html#8) for incompatibilities with Lua 5.1.

A key strategy in our NodeMCU migration to Lua 5.3 is that all NodeMCU application modules must be compilable and work under both Lua versions.  This has been achieved by three mechanisms
-  The standard Lua build has conditionals to enable improved compatibility with earlier versions.  In general the Lua 5.1 compatibilities have been enabled in the Lua 5.3 builds.
-  Regressing new Lua 5.3 features into the Lua 5.1 API.
-  For a limited number of features the Project accepts that the two versions APIs are incompatible and hence modules should either avoid their use or use `#if LUA_VERSION_NUM == 501` conditional compilation.

The following subsections detail how NodeMCU Lua versions deviate from standard Lua in order to achieve these objectives.

### Enabling Compatibility modes

The following Compatibility modes are enabled:
-  `LUA_COMPAT_APIINTCASTS`.  These `ìnt`cast are still used within NodeMCU modules.
-  `LUA_COMPAT_UNPACK`.  This retains `ROM.unpack` as a global synonym for `table.unpack` 
-  `LUA_COMPAT_LOADERS`.  This keeps `package.loaders` as a synonym for `package.seachers`
-  `LUA_COMPAT_LOADSTRING`.  This keeps `loadstring(s)` as a synonym for `load(s)`.

### New Lua 5.3 features back-ported into the Lua 5.1 API

-  Table access routines in Lua 5.3 are now type `int` rather than `void` and return the type of the value pushed onto the stack.  The 5.1 routines `lua_getfield`, `lua_getglobal`, `lua_geti`, `lua_gettable`, `lua_rawget`, `lua_rawgeti`, `lua_rawgetp` and `luaL_getmetatable` have been updated to mirror this behaviour and return the type of the value pushed onto the stack.
-  There is a general numeric comparison API function `lua_compare()` with macros for `lua_equal()` and `lua_lessthan()` whereas 5.1 only support the `==` and `<` tests through separate API calls.  5.1 has been update to mirror the 5.3. implementation.
-  Lua 5.3 includes a `lua_absindex(L, idx)` which converts ToS relative (e.g. `-1`) indices to stack base relative and hence independent of further push/pop operations.  This makes using down-stack indexes a lot simpler. 5.1 has been updated to mirror this 5.3 function.
 
### Feature breaks

-  `\0` is now a valid pattern character in search patterns, and the `%z` pattern is no longer supported. We suggest that modules either limit searching to non-null strings or accept that the source will require version variants.

-  The stack pseudo-index `LUA_GLOBALSINDEX` has been removed. Modules must either get the global environment from the registry entry `LUA_RIDX_GLOBALS` or use the `lua_getglobal` API call.  All current global references in the NodeMCU library modules now use `lua_getglobal` where necessary.

-  Shared userdata and table upvalues.  Lua 5.3 now supports the sharing of GCObjects such as userdata and tables as upvalue between C functions using `lua_getupvalue` and `lua_setupvalue`.  This has changed from 5.1 and we suggest that modules either avoid using these API calls or accept that the source will require version variants.

-  The environment support has changed from Lua 5.1 to 5.3.  We suggest that modules either avoid using these API calls or accept that the source will require version variants.

  -  The coroutine yield / resume API support has changed in Lua 5.3 to support yield and resume across C function calls.  We suggest that modules either avoid using these API calls or accept that the source will require version variants. 

