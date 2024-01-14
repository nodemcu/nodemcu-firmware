# Programming in NodeMCU

The standard Lua runtime offers support for both Lua modules that can define multiple Lua functions and properties in an encapsulating table as described in the [Lua 5.3 Reference Manual](https://www.lua.org/manual/5.3/) \("**LRM**") and specifically in [LRM Section 6.3](https://www.lua.org/manual/5.3/manual.html#6.3).  Lua also provides a C API to allow developers to implement modules in compiled C.

NodeMCU developers are also able to develop and incorporate their own C modules into their own firmware build using this standard API, although we encourage developers to download the standard [Lua Reference Manual](https://www.lua.org/manual/) and also buy of copy of Roberto Ierusalimschy's Programming in Lua edition 4 \("**PiL**").  The NodeMCU implementation extends standard Lua as documented in the [NodeMCU Reference Manual](nodemcu-lrm.md) \("**NRM**").

Those developers who wish to develop or to modify existing C modules should have access to the LRM, PiL and NRM and familiarise themselves with these references.  These are the primary references; and this document does not repeat this content, but rather provide some NodeMCU-specific information to supplement it.

From a perspective of developing C modules, there is very little difference from that of developing modules in standard Lua.  All of the standard Lua library modules (`bit`, `coroutine`, `debug`, `math`, `string`, `table`, `utf8`) use the C API for Lua and the NodeMCU versions have been updated to use NRM extensions. so their source code is available for browsing and using as style template (see the corresponding `lXXXlib.c` file in GitHub [NodeMCU lua53](../components/lua/lua-5.3) folder).

The main functional change is that NodeMCU supports a read-only subclass of the `Table` type, known as a **`ROTable`**, which can be statically declared within the module source using static `const` declarations.  There are also limitations on the valid types for ROTable keys and value in order to ensure that these are consistent with static declaration; and hence ROTables are stored in code space (and therefore in flash memory on the IoT device). Hence unlike standard Lua tables, ROTables do not take up RAM resources. 

Also unlike standard Lua, two global ROTables are used for the registration of C modules.  Again, static declaration macros plus linker "magic" (use of make filters plus linker _section_ directives) result in the marshalling of these ROTables during the make process, and because this is all ROTable based, the integration of modules into the firmware builds and their access from executing Lua applications depends on code space rather than RAM-based data structures. 

Note that dynamic loading of C modules is not supported within the ESP SDK, so any library registration must be compiled into the source used in the firmware build.  Our approach is simple, flexible and avoids the RAM overheads of the standard Lua approach.  The special ROTable **`ROM`** is core to this approach.  The global environment table has an `__index` metamethod referencing this ROM table.  Hence, any non-raw lookups against the global table will also resolve against ROM.  All base Lua functions (such as `print`) and any C libraries (written to NodeMCU standards) have an entry in the ROM table and hence have global visibility.  This approach does not prevent developers use of standard Lua mechanisms, but rather it offers a simple low RAM use alternative.

The `NODEMCU_MODULE` macro is used in each module to register it in an entry in the **`ROM`** ROTable.  It also adds a entry in the second (hidden) **`ROMentry`** ROTable.
-  All `ROM` entries will resolve globally
-  The Lua runtime scans the `ROMentry` ROTable during its start up, and it will execute any non-NULL `CFunction` values in this table.  This enables C modules to hook in any one-time start-up functions if they are needed.

For a module to be included in the build, it has to be enabled in the sdkconfig file (e.g. via running `make menuconfig`). Some modules are enabled by default. Between compile time macros based on the sdkconfig and linker processing only the enabled modules are actually included into the firmware.

This macro + linker approach renders the need for `luaL_reg` declarations and use of `luaL_openlib()` unnecessary, and these are not permitted in project-adopted `components/modules` files.

Hence a NodeMCU C library module typically has a standard layout that parallels that of the standard Lua library modules and uses the same C API to access the Lua runtime:

-  A `#ìnclude` block to resolve access to external resources.  All modules will include entries for `"module.h"`, and `"lauxlib.h"`.  They should _not_ reference any other `lXXX.h` includes from the Lua source directory as these are private to the Lua runtime. These may be followed by C standard runtime includes, external application libraries and any SDK resource headers needed.

-  The only external interface to a C module should be via the Lua runtime and its `NODEMCU_MODULE` hooks.  Therefore all functions and resources should be declared `static` and be private to the module. These can be ordered as the developer wishes, subject of course to the need for appropriate forward declarations to comply with C scoping rules.

-  Module methods will typically employ a Lua standard `static int somefunc (lua_State *L) { ... }` template.
-  ROTables are typically declared at the end of the module to minimise the need for forward references and use the `LROT` macros described in the NRM.  Note that ROTables only support static declaration of string keys and the value types: C function, Lightweight userdata, Numeric, ROTable.  ROTables can also have ROTable metatables.
   -  Whilst the ROTable search algorithm is a simply linear scan of the ROTable entries, the runtime also maintains a LRU cache of ROTable accesses, so typically over 95% of ROTable accesses bypass the linear scan and do a direct access to the appropriate entry.
   -  ROTables are also reasonable lightweight and well integrated into the Lua runtime, so the normal metamethod processing works well.  This means that developers can use the `__index` method to implement other key and value typed entries through an index function.
-  NodeMCU modules are intended to be compilable against both our Lua 5.1 and Lua 5.3 runtimes.  The NRM discusses the implications and constraints here.  However note that:
   -  We have back-ported many new Lua 5.3 features into the NodeMCU Lua 5.1 API, so in general you can use the 5.3 API to code your modules.  Again the NRM notes the exceptions where you will either need variant code or to decide to limit yourself to the the 5.3 runtime. In this last case the simplest approach is to `#if LUA_VERSION_NUM != 503` to disable the 5.3 content so that 5.1 build can compile and link.   Note that all modules currently in the `components/modules` folder will compile against and execute within both the Lua 5.1 and the 5.3 environments.
   -  Lua 5.3 uses a 32-bit representation for all numerics with separate subtypes for integer (stored as a 32 bit signed integer) and float (stored as 32bit single precision float).  This achieves the same RAM storage density as Lua 5.1 integer builds without the loss of use of floating point when convenient.  We have therefore decided that there is no benefit in having a separate Integer 5.3 build variant.
 -  We recommend that developers make use of the full set of `luaL_` API calls to minimise code verbosity.  We have also added a couple of registry access optimisations that both simply and improve runtime performance when using the Lua registry for callback support.
    - `luaL_reref()` replaces an existing registry reference in place (or creates a new one if needed).  Less code and faster execution than a `luaL_unref()` plus `luaL_ref()` construct.
    - `luaL_unref2()` does the unref and set the static int hook to `LUA_NOREF`.

Rather than include simple examples of module templates, we suggest that you review the modules in our GitHub repository, such as the [`utf8`](../components/lua/lua-5.3/lutf8lib.c) library.  Note that whilst all of the existing modules in `components/modules` folder compile and work, we plan to do a clean up of the core modules to ensure that they conform to best practice.
