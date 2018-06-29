# Lua Flash Store (LFS)

## Background

An IoT device such as the ESP8266 has very different processor characteristics from the CPU in a typical PC:

-  Conventional CPUs have a lot of RAM, typically more than 1 Gb, that is used to store both code and data.  IoT processors like the ESP variants use a [modified Harvard architecture](https://en.wikipedia.org/wiki/Modified_Harvard_architecture) where code can also be executed out of flash memory that is mapped into a address region separate from the limited RAM.

-  Conventional CPU motherboards include RAM and a lot of support chips. ESP modules are postage stamp-sized and typically comprise one ESP [SoC](https://en.wikipedia.org/wiki/System_on_a_chip) and a flash memory chip used to store firmware and a limited file system.

Lua was originally designed as a general embeddable extension language for applications that would typically run on systems such as a PC, but its design goals of speed, portability, small kernel size, extensibility and ease-of-use also make Lua a good choice for embedded use on an IoT platform.  Our NodeMCU firmware implementation was therefore constrained by the standard Lua core runtime system (**RTS**) that assumes a conventional CPU architecture with both Lua code and data in RAM; however ESP8266 modules only have approximately 48Kb RAM for application use, even though the firmware itself executes out of the larger flash-based program memory.

This Lua Flash Store (**LFS**) patch modifies the NodeMCU Lua RTS to allow Lua code and its associated constant data to be executed directly out of flash-memory, just as the firmware itself is executed. This now enables NodeMCU Lua developers to create Lua applications with up to 256Kb Lua code and read-only (**RO**) constants executing out of flash, so that all of the RAM is available for read-write (**RW**) data.

Though the ESP architecture does allow RW operations to flash, these are constrained by the write limitations of NAND flash architecture, as writing involves the block erasing of 4Kb pages and then overwriting each pach with new content.  Whilst it is possible (as with SPIFFS) to develop R/W file systems working within this constraint, memory-mapped read access to flash is cached through a RAM cache in order to accelerate code execution, and this makes it practically impossible to modify executable code pages on the fly. Hence the LFS patch must work within a reflash-and-restart paradigm for reloading the LFS.

The LFS patch does this by adding two API new calls to the `node` module: one to reflash the LFS and restart the processor, and one to access the LFS store once loaded.  Under the hood, it also addresses all of the technical issues to make this magic happen.

The remainder of this paper is split into two sections:

-  The first section provides an overview the issues that a Lua developer needs to understand at an application level to use LFS effectively.

-  The second gives more details on the technical issues that were addressed in order to implement the patch. This is a good overview for those that are interested, but many application programmers won't care how the magic happens, just that it does.


## Using LFS

### Selecting the firmware

Power developers might want to use Docker or their own build environment as per our [Building the firmware](https://nodemcu.readthedocs.io/en/master/en/build/) documentation, and so `app/include/user_config.h` has now been updated to include the necessary documentation on how to select the configuration options to make an LFS firmware build.

However, most Lua developers seem to prefer the convenience of our [Cloud Build Service](https://nodemcu-build.com/), so we will add two extra menu options to facilitate building LFS images:

Variable | Option
---------|------------
LFS size | (none, 32Kb, 64Kb, 94Kb) The default is none. Selecting a numeric value builds in the corresponding LFS.
SPIFFS size | (default or a multiple of 64Kb) The cloud build will base the SPIFFS at 1Mb if an explicit size is specified.

You must choose an explicit (non-default) LFS size to enable the use of LFS.  Whilst you can use a default (maximal) SPIFFS configuration, most developers find it more useful to work with a fixed SPIFFS that has been sized to match their application reqirements.

### Choosing your development lifecycle

 The build environment for generating the firmware images is Linux-based, but as you can use our cloud build service to generate these, you can develop NodeMCU applications on pretty much any platform including Windows and MacOS. Unfortunately LFS images must be built off-ESP on a host platform, so you must be able to run the `luac.cross` cross compiler on your development machine to build LFS images.

-  For Windows 10 developers, the easiest method of achieving this is to install the [Windows Subsystem for Linux](https://en.wikipedia.org/wiki/Windows_Subsystem_for_Linux).  Most WSL users install the Ubuntu Bash shell as well; note that this is just a shell and some core GNU utilities (somewhat similar to Cygwin) rather than a full Ubuntu OS, as WSL extends the NT kernel to support the direct execution of Linux ELF images. WSL can directly run the `luac.cross` and `spiffsimg` that are build as part of the firmware. You will also need the `esptool.py` tool but `python.org` already provides Python releases for Windows.

-  Linux users can just use these tools natively.

-  In principle, only the build environment components needed to support `luac.cross` and `spiffsimg` are the `app/lua/lua_cross` and `tools/spifsimg` subdirectory makes. It should be straight forward to get these working under any environment which provides POSIX runtime support, including MacOS and Cygwin (for windows versions prior to Win10), but suitable developer effort is required to generate suitable executables; any volunteers?

Most Lua developers seem to start with the [ESPlorer](https://github.com/4refr0nt/ESPlorer) tool, a 'simple to use' IDE that enables beginning Lua developers to get started.  However, ESPlorer relies on a UART connection and this can be slow and cumbersome, and it doesn't scale well for larger ESP application.

So many experienced Lua developers switch to a rapid development cycle where they use a development machine to maintain your master Lua source. Going this route will allow you use your favourite program editor and source control, with one of various techniques for compiling the lua on-host and downloading the compiled code to the ESP:

-  If you use a fixed SPIFFS image (I find 128Kb is enough for most of my applications), then you can script recompiling your LC files, rebuilding a SPIFFS image and loading it onto the ESP using `esptool.py` in less than 60 sec. You can either embed the LFS.img in the SPIFFS, or you can use the `luac.cross -a` option to directly load the new LFS image into the LFS region within the firmware.

-  I now have an LFS aware version of my LuaOTA provisioning system (see `lua_examples/luaOTA`).  This handles all of the incremental compiling and LFS reloads transparently. This is typically integrated into the ESP application.

-  Another option would be to include the FTP and Telnet modules in the base LFS image and to use telnet and FTP to update your system.  (Given that a 64Kb LFS can store thousands of lines of Lua, doing this isn't much of an issue.)

My current practice is to use a small bootstrap `init.lua` file in SPIFFS to load the `_init` module from LFS, and this does all of the actual application initialisation. My `init.lua`:

-  Is really a Lua binary (`.lc`) file renamed to a `.lua` extension.  Using a binary init file avoids loading the Lua compiler.  This works because even though the firmware looks for `init.lua`, the file extension itself is a just a convention; it is treated as a Lua binary if it has the correct Lua binary header.

-  Includes a 1 sec delay before connecting to the Wifi.  This is a "just in case" when I am developing.  This is enough to allow me to paste a `file.remove'init.lua'` into the UART if I want to do different development paths.

No doubt some standard usecase / templates will be developed by the community over the next six months.

### Programming Techniques and approachs

I have found that moving code into LFS has changed my coding style, as I tend to use larger modules and I don't worry about in-memory code size. This facilitates a more 'keep it simple stupid' coding style, so my ESP Lua code now looks more similar to host-based Lua code. I still prefer to keep the module that I am currently testing in SPIFFS, and only move modules into LFS once they are stable. However if you use `require` to load modules then this can all be handled by the require loader.

Here is the code fragment that I use in my `_init` module to do this magic:

```Lua
do
  local index = node.flashindex
  -- Setup the LFS object
  local lfs_t = {
    __index = function(_, name)
        local fn_ut, ba, ma, size, modules = index(name)
        if not ba then
          return fn_ut
        elseif name == '_time' then
          return fn_ut
        elseif name == '_config' then
          local fs_ma, fs_size = file.fscfg()
          return {ba, ma, fs_ma, size, fs_size}
        elseif name == '_list' then
          return modules
        else
          return nil
        end
            end,
    __newindex = function(_, name, value)
        error("LFS is readonly. Invalid write to LFS." .. name, 2)
      end
  }
  rawset(getfenv(),'LFS', setmetatable(lfs_t,lfs_t))
  -- And add LFS to the require path list
  local function loader_flash(module)
    local fn, ba = index(module)
    return ba and "Module not in LFS" or fn
  end
  package.loaders[3] = loader_flash

end
```

Once this has been executed, if you have a function module `func1` in LFS, then `LFS.func1(x,y,z)` just works as you would expect.  The LFS properties `_time`, `_config` and `_list` can be used to access the other LFS metadata that you need.

Of course, if you use Lua modules to build your application then `require "some_module"` will automatically path in and load your modules from LFS.  Note that SPIFFS is still ahead of LFS in this search list, so if you have a dev version in SPIFFS, say, then this will be loaded first. However, if you want to want to swap this search order so that the LFS is searched first, then set `package.loaders[1] = loader_flash` in your `_init` code.  If you need to swap the search order temporarily for development or debugging, then do this after you've run the `_init` code:

```Lua
do local pl = package.loaders; pl[1],pl[3] = pl[3],pl[1]; end
```

Whilst LFS is primarily used to store compiled modules, it also includes its own string table and any strings loaded into this can be used in your Lua application without taking any space in RAM. Hence, you might also want to preload any other frequently used strings into LFS as this will both save RAM use and reduced the Lua-custom Garbage Collector (**LGC**) overheads.

The patch adds an extra debug function `getstrings()` function to help you determine what strings are worth adding to LFS.  This takes an optional string argument `'RAM'` (the default) or `'ROM'`, and returns a list of the strings in the corresponding table. So the following example can be used to get a listing of the strings in RAM.  You can enter the following Lua at the interactive prompt or call it as a debug function during a running application in order to generate this string list.

```Lua
do
  local a=debug.getstrings'RAM'
  for i =1, #a do a[i] = ('%q'):format(a[i]) end
  print ('local preload='..table.concat(a,','))
end
```

If you then create a file, say `LFS_dummy_strings.lua`, and put these `local preload` lines in it, and include this file in your `luac.cross -f`, then the cross compiler will generate a ROM string table that includes all strings referenced in this dummy module. You never need to call this module; just it's inclusion in the LFS build is enough to add the strings to the ROM table. Once in the ROM table, then you can use them subsequently in your application without incurring any RAM or LGC overhead. The following example is a useful starting point, but if needed then you can add to this
for your application.

```Lua
local preload = "?.lc;?.lua", "@init.lua", "_G", "_LOADED", "_LOADLIB", "__add",
"__call", "__concat", "__div", "__eq", "__gc", "__index", "__le", "__len", "__lt",
"__mod", "__mode", "__mul", "__newindex", "__pow", "__sub", "__tostring", "__unm",
"collectgarbage", "cpath", "debug", "file", "file.obj", "file.vol", "flash",
"getstrings", "index", "ipairs", "list", "loaded", "loader", "loaders", "loadlib",
"module", "net.tcpserver", "net.tcpsocket", "net.udpsocket", "newproxy", "package",
"pairs", "path", "preload", "reload", "require", "seeall", "wdclr"
```

## Technical Issues

Whilst memory capacity isn't a material constraint on most conventional machines, the Lua RTS still embeds some features to minimise overall memory usage. In particular:

-  The more resource intensive data types are know as _collectable objects_, and the RTS includes a LGC which regularly scans these collectable resources to determine which are no longer in use, so that their associated memory can be reclaimed and reused.

-  The Lua RTS also treats strings and compiled function code as collectable objects, so that these can also be LGCed when no longer referenced

The compiled code, as executed by Lua RTS, internally comprises one or more function prototypes (which use a `Proto` structure type) plus their associated vectors (constants, instructions and meta data for debug). Most of these compiled constant types are basic (e.g. numbers) and the only collectable constant data type are strings. The other collectable types such as arrays are actually created at runtime by executing Lua compiled instructions to build each resource dynamically.

Currently, when any Lua file is loaded into an ESP application, the RTS loads the corresponding compiled version into RAM. Each compiled function has its own Proto structure hierarchy, but this hierarchy is not exposed directly to the running application; instead the compiler generate `CLOSURE` instruction which is executed at runtime to bind the Proto to a Lua function value thus creating a [closure](https://en.wikipedia.org/wiki/Closure_(computer_programming)). Since this occurs at runtime, any `Proto` can be bound to multiple closures. A Lua closure can have multiple RW [Upvalues](https://www.lua.org/pil/27.3.3.html) bound to it, and so function value is much like a Lua object in that it is refering to something that can contain RW state, even though the `Proto` hierarchy itself is intrinsically RO.

Whilst advanced ESP Lua programmers can use overlay techniques to ensure that only active functions are loaded into RAM and thus increase the effective application size, this adds to runtime and program complexity. Moving Lua "program" resources into ESP Flash addressable memory typically doubles the effective RAM available, and largley removes the need to complicate applications code to facilitate overlaying.

Any RO resources that are relocated to a flash address space:

-  Must not be collected. Also RW references to RO resources must be robustly handled by the LGC.
-  Cannot reference to any volatile RW data elements (though RW resources can refer to RO resources).

All strings in Lua are [interned](https://en.wikipedia.org/wiki/String_interning), so that only one copy of any string is kept in memory and most string manipulation uses the address of this single copy as a unique reference. This uniqueness and the LGC of strings is facilitated by using a global string table that is hooked into the Lua Global State. Under standard Lua, any new string is first resolved against RAM string table, with only the string-misses being added to the string table. The LFS patch adds a second RO string table in flash and which contains all strings used in LFS Protos. Maintaining integrity across the two RAM and RO string tables is simple and low-cost, with LFS resolution process extended across both the RAM and ROM string tables. Hence any strings already in the ROM string table will generate a unique string reference without the need to add an additional entry in the RAM table. This both significantly reduces the size of the RAM string table, and removes a lot of strings from the LCG scanning.

Note that early development implementations of the LFS build process allowed on-target ESP builds. Unfortunately, we found in practice that the Lua compiler was so resource hungry that it was impractical to get this to scale to usable application sizes, and we therefore abandoned this approach, moving the LFS build process onto the development host machine by embedding this into `luac.cross`. This approach avoids all of the update integrity issues involved in building a new LFS which might require RO resources already referenced in the RW ones.

Any LFS image can be loaded in the LFS store by one of two mechanisms:

-  The image can be build on the host and then copied into SPIFFS. Calling the `node.flashreload()` API with this filename will load the image, and then schedule a restart to leave the ESP in normal application mode, but with an updated flash block.  This sequence is essentially atomic. Once called, the only exit is the reboot.

-  The second option is to build the LFS image using the `-a` option to base it at the correct absolute address of the LFS store for a given firmware image.  The LFS can then be flashed to the ESP along with the firmware image.

The LFS store is a fixed size for any given firmware build (configurable by the a pplication developer through `user_config.h`) and is at a build-specific base address within the `ICACHE_FLASH` address space.  This is used to store the ROM string table and the set of `Proto` hierarchies corresponding to a list of Lua files in the loaded image.

A separate `node.flashindex()` function creates a new Lua closure based on a module loaded into LFS and more specfically its flash-based prototype; whilst this access function is not transparent at a coding level, this is no different functionally than already having to handle `lua` and `lc` files and the existing range of load functions (`load`,`loadfile`, `loadstring`). Either way, creating a closure on flash-based prototype is _fast_ in terms of runtime. (It is basically a single instruction rather than a compile, and it has minimal RAM impact.)

### Basic approach

This **LFS** patch uses two string tables: the standard Lua RAM-based table (`RWstrt`) and a second RO flash-based one (`ROstrt`). The `RWstrt` is searched first when resolving new string requests, and then the `ROstrt`. Any string not already in either table is then added to the `RWstrt`, so this means that the RAM-based string table only contains application strings that are not already defined in the `ROstrt`.

Any Lua file compiled into the LFS image includes its main function prototype and all the child resources that are linked in its `Proto` structure; so all of these resources are compiled into the LFS image with this entire hierarchy self-consistently within the flash memory.

```C
   TValue        *k;         Constants used by the function
   Instruction   *code       The Lua VM instuction codes
   struct Proto **p;         Functions defined inside the function
   int           *lineinfo;  Debug map from opcodes to source lines
   struct LocVar *locvars;   Debug information about local variables
   TString      **upvalues   Debug information about upvalue names
   TString       *source     String name associated with source file
 ```

Such LFS images are created by `luac.cross` using the `-f` option, and this builds a flash image based on the list of modules provided but with a master "main" function of the form:

```Lua
local n = ...,1518283691  -- The Unix Time of the compile
if n == "module1" then return module1 end
if n == "module2" then return module2 end
-- and so on
if n == "moduleN" then return module2 end
return 1518283691,"module1","module2", --[[ ... ]] ""moduleN"
```

You can't actually code this Lua because the modules are in separate compilation units, but the compiler being a compiler can just emit the compiled code directly. (See `app/lua/luac_cross/luac.c` for the details.)

The deep cross-copy of the `Proto` hierarchy is also complicated because current hosts are typically 64bit whereas the ESPs are 32bit, so the structures need repacking. (See `app/lua/luac_cross/luac.c` for the details.)

With this patch, the `luac.cross` build has been moved into the overall application hierarchy and is now simply a part of the NodeMCU make. The old Lua script has been removed from the `tools` directory, together with the need to have Lua preinstalled on the host.

The LFS image is by default position independent, so is independent of the actual NodeMCU target image. You just have to copy it to the target file system and execute a `reload` to copy this to the correct location, relocating all address to the correct base. (See `app/lua/lflash.c` for the details.) This process is fast.  However, -a `luac.cross -a` also allows absolute address images to be built for direct flashing into the LFS store during provisioning.

### Impact of the Lua Garbage Collector

The LGC applies to what the Lua VM classifies as collectable objects (strings, tables, functions, userdata, threads -- known collectively as `GCObjects`). A simple two "colour" LGC was used in previous Lua versions, but Lua 5.1 introduced the Dijkstra's 3-colour (*white*, *grey*, *black*) variant that enabled the LGC to operate in an incremental mode. This permits smaller LGC steps interspersed by LGC pause, and is very useful for larger scale Lua implementations. Whilst this is probably not really needed for IoT devices, NodeMCU follows this standard Lua 5.1 implementation, albeit with the `elua` EGC changes.

In fact, two *white* flavours are used to support incremental working (so this 3-colour algorithm really uses 4). All newly allocated collectable objects are marked as the current *white*, and include a link in their header to enable scanning through all such Lua objects. They may also be referenced directly or indirectly via one of the Lua application's *roots*: the global environment, the Lua registry and the stack. The LGC operates two broad phases: **mark** and **sweep**.

The LGC algorithm is quite complex and assumes that all GCObjects are RW so that a flag byte within each object can be updated during the mark and sweep processing. LFS introduces GCObjects that are actually stored in RO memory and are therefore truly RO.  Any attempt to update their content during LGC will result in the firmware crashing with a memory exception, so the LFS patch must therefore modify the LGC processing to avoid such potential updates whilst maintaining its integrity, and the remainder of this
section provides further detail on how this was achieved.

The **mark** phase walks collectable objects by a recursive walk starting at at the LGC roots. (This is referred to as _traverse_.) Any object that is visited in this walk has its colour flipped from *white* to *grey* to denote that it is in use, and it is relinked into a grey list. The grey list is iteratively processed, removing one grey object at a time. Such objects can reference other objects (e.g. a table has many keys and values which can also be collectable objects), so each one is then also traversed and all objects reachable from it are marked, as above. After an object has been traversed, it's turned from grey to black. The LGC will walks all RW collectable objects, traversing the dependents of each in turn.  As RW objects can now refer to RO ones, the traverse routines has additinal tests to skip trying to mark any RO LFS references.

The white flavour is flipped just before entering the **sweep** phase. This phase then loops over all collectable objects. Any objects found with previous white are no longer in user, and so can be freed. The 'current' white are kept; this prevents any new objected created during a paused sweep from being accidentally collected before being marked, but this means that it takes two sweeps to free all unused objects. There are other subtleties introduced in this 3-colour algorithm such as barriers and back-tracking to maintain integrity of the LGC, and these also needed extra rules to handle RO GCObjects correclty, but detailed explanation of these is really outside the scope of this paper.

As well as standard collectable GCOobjets:

-  Standard Lua has the concept of **fixed** objects. (E.g. the main thread). These won't be collected by the LGC, but they may refer to objects that aren't fixed, so the LGC still has to walk through an fixed objects.

-  eLua added the the concept of **readonly** objects, which confusingly are a hybrid RW/RO implementation, where the underlying string resource is stored as a program constant in flash memory but the `TSstring` structure which points to this is still kept in RAM and can by GCed, except that in this case the LGC does not free the RO string constant itself.

-  LFS introduces a third variant **flash** object for `LUA_TPROTO` and `LUA_TSTRING` types. Flash objects can only refer to other flash objects and are entirely located in the LFS area in flash memory.

The LGC already processed the _fixed_ and _readonly_ object, albeit as special cases. In the case of _flash_ GCObjects, the `mark` flag is in read-only memory and therefore the LGC clearly can't use this as a RW flag in its mark and sweep processing. So the LGC skips any marking operations for flash objects. Likewise, where all other GCObjects are linked into one of a number of sweeplists using the object's `gclist` field. In the case of flash objects, the compiler presets the `mark` and `gclist` fields with the fixed and readonly mark bits set, and the list pointer to `NULL` during the compile process.

As far as the LGC algorithm is concerned, encountering any _flash_ object in a sweep is a dead end, so that branch of the walk of the GCObject hierarchy can be terminated on encountering a flash object. This in practice all _flash_ objects are entirely removed from the LGC process, without compromising collection of RW resources.

### General comments

- **Reboot implementation**. Whilst the application initiated LFS reload might seem an overhead, it typically only adds a few seconds per reboot. We may also consider the future enhancement of the `esptool.py` to enable the inclusion of an LFS image into the unified application flash image.

- **LGC reduction**. Since the cost of LGC is directly related to the size of the LGC sweep lists, moving RO resources into LFS memory removes them from the LGC scope and therefore reduces LGC runtime accordingly.

- **Typical Usecase**.  The rebuilding of a store is an occasional step in the development cycle. (Say up to 10 times a day in a typical intensive development process). Modules and source files under development would typically be executed from SPIFFS in `.lua` format. The developer is free to reorder the `package.loaders` and load any SPIFFS files in preference to Flash ones. And if stable code is moved into Flash, then there is little to be gained in storing development Lua code in SPIFFS in `lc` compiled format.

- **Flash caching coherency**.  The ESP chipset employs hardware enabled caching of the `ICACHE_FLASH` address space, and writing to the flash does not flush this cache.  However, in this restart model, the CPU is always restarted before any updates are read programmatically, so this (lack of) coherence isn't an issue.

- **Failsafe reversion**. Since the entire image is precompiled, the chances of failure during reload are small. The loader uses the Flash NAND rules to write the flash header flag in two parts: one at start of the load and again at the end. If on reboot, the flag in on incostent state, then the LFS is cleared and disabled until the next reload.
