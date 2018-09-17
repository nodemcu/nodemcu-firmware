# Lua Flash Store (LFS)

## Background

Lua was originally designed as a general purpose embedded extension language for use in applications run on a conventional computer such as a PC, where the processor is mounted on a motherboard together with multiple Gb of RAM and a lot of other chips providing CPU and I/O support to connect to other devices.

ESP8266 modules are on a very different scale: they cost a few dollars; they are postage stamp-sized and only mount two main components, an ESP [SoC](https://en.wikipedia.org/wiki/System_on_a_chip) and a flash memory chip.  The SoC includes limited on-chip RAM, but also provides hardware support to map part of the external flash memory into a separate memory address region so that firmware can be executed directly out of this flash memory &mdash; a type of [modified Harvard architecture](https://en.wikipedia.org/wiki/Modified_Harvard_architecture) found on many [IoT](https://en.wikipedia.org/wiki/Internet_of_things) devices.  Even so, Lua's design goals of speed, portability, small kernel size, extensibility and ease-of-use have made it a good choice for embedded use on an IoT platform, but with one major limitaton: the standard Lua core runtime system (**RTS**) assumes that both Lua data _and_ code are stored in RAM; this isn't a material constraint with a conventional computer, but it can be if your system only has some 48Kb RAM available for application use.

The Lua Flash Store (**LFS**) patch modifies the Lua RTS to support a modified Harvard architecture by allowing the Lua code and its associated constant data to be executed directly out of flash-memory (just as the NoceMCU firmware is itself executed). This now allows NodeMCU Lua developers to create Lua applications with up to 256Kb Lua code and read-only (**RO**) constants executing out of flash, with all of the RAM is available for read-write (**RW**) data.

Unfortunately, the ESP architecture provides very restricted write operations to flash memory (writing to NAND flash involves bulk erasing complete 4Kb memory pages, before overwriting each erased page with any new content).  Whilst it is possible to develop a R/W file system within this constraint (as SPIFFS demonstrates), this makes impractical to modify Lua code pages on the fly. Hence the LFS patch works within a reflash-and-restart paradigm for reloading the LFS, and does this by adding two API new calls: one to reflash the LFS and restart the processor, and one to access LFS stored functions.  The patch also addresses all of the technical issues 'under the hood' to make this magic happen.

The remainder of this paper is for those who want to understand a little of how this magic happens, and gives more details on the technical issues that were addressed in order to implement the patch.

If you're just interested in learning how to quickly get started with LFS then please read the respective chapters in the [Getting Started](./getting-started.md) overview.

## Using LFS

### Selecting the firmware

Power developers might want to use Docker or their own build environment as per our [Building the firmware](https://nodemcu.readthedocs.io/en/master/en/build/) documentation, and so `app/include/user_config.h` has now been updated to include the necessary documentation on how to select the configuration options to make an LFS firmware build.

However, most Lua developers seem to prefer the convenience of our [Cloud Build Service](https://nodemcu-build.com/), so we have added extra LFS menu options to facilitate building LFS images:

Variable | Option
---------|------------
LFS size | (none, 32, 64, 96 or 128Kb) The default is none. The default is none, in which case LFS is disabled. Selecting a numeric value enables LFS with the LFS region sized at this value.
SPIFFS base | If you have a 4Mb flash module then I suggest you choose the 1024Kb option as this will preserve the SPIFFS even if you reflash with a larger firmware image; otherwise leave this at the default 0. 
SPIFFS size | (default or various multiples of 64Kb) Choose the size that you need.  Larger FS require more time to format on first boot.

You must choose an explicit (non-default) LFS size to enable the use of LFS.  Most developers find it more useful to work with a fixed SPIFFS size matched to their application requirements.

### Choosing your development life-cycle

 The build environment for generating the firmware images is Linux-based, but you can still develop NodeMCU applications on pretty much any platform including Windows and MacOS, as you can use our cloud build service to generate these images. Unfortunately LFS images must be built off-ESP on a host platform, so you must be able to run the `luac.cross` cross compiler on your development machine to build LFS images.

-  For Windows 10 developers, one method of achieving this is to install the [Windows Subsystem for Linux](https://en.wikipedia.org/wiki/Windows_Subsystem_for_Linux).  The default installation uses the GNU `bash` shell and includes the core GNU utilities. WSL extends the NT kernel to support the direct execution of Linux ELF images, and it can directly run the `luac.cross` and `spiffsimg` that are build as part of the firmware. You will also need the `esptool.py` tool but `python.org` already provides Python releases for Windows.  Of course all Windows developers can use the Cygwin environment as this runs on all Windows versions and it also takes up less than ½Gb HDD (WSL takes up around 5Gb).

-  Linux users can just use these tools natively.  Windows users can also to do this in a linux VM or use our standard Docker image.  Another alternaive is to get yourself a Raspberry Pi or equivalent SBC and use a package like [DietPi](http://www.dietpi.com/) which makes it easy to install the OS, a Webserver and Samba and make the RPi look like a NAS to your PC.  It is also straightforward to write a script to automatically recompile a Samba folder after updates and to make the LFS image available on the webservice so that your ESP modules can update themselves OTA using the new `HTTP_OTA.lua` example.

-  In principle, only the environment component needed to support applicatin development is `luac.cross`, built by the `app/lua/lua_cross` make.  (Some developers might also use the  `spiffsimg` exectable, made in the `tools/spifsimg` subdirectory).  Both of these components use the host toolchain (that is the compiler and associated utilities), rather than the Xtensa cross-compiler toolchain, so it is therefore straightforward to make under any environment which provides POSIX runtime support, including WSL, MacOS and Cygwin.

Most Lua developers seem to start with the [ESPlorer](https://github.com/4refr0nt/ESPlorer) tool, a 'simple to use' IDE that enables beginning Lua developers to get started.  ESPlorer can be slow cumbersome for larger ESP application, and it requires a direct UART connection.  So many experienced Lua developers switch to a rapid development cycle where they use a development machine to maintain your master Lua source. Going this route will allow you use your favourite program editor and source control, with one of various techniques for compiling the lua on-host and downloading the compiled code to the ESP:

-  If you use a fixed SPIFFS image (I find 128Kb is enough for most of my applications) and are developing on a UART-attached ESP module, then you can also recompile any LC files and LFS image, then rebuild a SPIFFS file system image before loading it onto the ESP using `esptool.py`; if you script this you will find that this cycle takes less than a minute. You can either embed the LFS.img in the SPIFFS.  You can also use the `luac.cross -a` option to build an absolute address format image that you can directly flash into the LFS region within the firmware.

-  If you only need to update the Lua components, then you can work over-the-air (OTA).  For example see my 
[HTTP_OTA.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/HTTP_OTA.lua), which pulls a new LFS image from a webservice and reloads it into the LFS region.  This only takes seconds, so I often use this in preference to UART-attached loading.

-  Another option would be to include the FTP and Telnet modules in the base LFS image and to use telnet and FTP to update your system.  (Given that a 64Kb LFS can store thousands of lines of Lua, doing this isn't much of an issue.)

My current practice is to use a small bootstrap `init.lua` file in SPIFFS to connect to WiFi, and also load the `_init` module from LFS to do all of the actual application initialisation.  There is a few sec delay whilst connecting to the Wifi, and this delay also acts as a "just in case" when I am developing, as it is enough to allow me to paste a `file.remove('init.lua')` into the UART if my test applicaiton is stuck into a panic loop, or set up a different development path for debugging.

Under rare circumstances, for example a power fail during the flashing process, the flash can be left in a part-written state following a `flashreload()`.  The Lua RTS start-up sequence will detect this and take the failsafe opton of resetting the LFS to empty, and if this happens then the LFS `_init` function will be unavailable.  Your `init.lua` should therefore not assume that the LFS contains any modules (such as `_init`), and should contain logic to detect if LFS reset has occurred and if necessary reload the LFS again. Calling `node.flashindex("_init")()` directly will result in a panic loop in these circumstances.  Therefore first check that `node.flashindex("_init")` returns a function or protect the call, `pcall(node.flashindex("_init"))`, and decode the error status to validate that initialisation was successful.

No doubt some standard usecase / templates will be developed by the community over the next six months.

### Programming Techniques and approachs

I have found that moving code into LFS has changed my coding style, as I tend to use larger modules and I don't worry about in-memory code size. This make it a lot easier to adopt a clearer coding style, so my ESP Lua code now looks more similar to host-based Lua code.  Lua code can still be loaded from SPIFFS, so you still have the option to keep code under test  in SPIFFS, and only move modules into LFS once they are stable.

#### Accessing LFS functions and loading LFS modules

See [lua_examples/lfs/_init.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/_init.lua) for the code that I use in my `_init` module to do create a simple access API for LFS.  There are two parts to this.

The first sets up a table in the global variable `LFS` with the `__index` and `__newindex` metamethods. The main purpose of the `__index()` is to resolve any names against the LFS using a `node.flashindex()` call, so that `LFS.someFunc(params)` does exactly what you would expect it to do: this will call `someFunc` with the specified parameters, if it exists in in the LFS.  The LFS properties `_time`, `_config` and `_list` can be used to access the other LFS metadata that you need.  See the code to understand what they do, but `LFS._list` is the array of all module names in the LFS.  The `__newindex` method makes `LFS` readonly.

The second part uses standard Lua functionality to add the LFS to the require [package.loaders](http://pgl.yoyo.org/luai/i/package.loaders) list. (Read the link if you want more detail).  There are four standard loaders, which the require loader searches in turn.  NodeMCU only uses the second of these (the Lua loader from the file system), and since loaders 1,3 and 4 aren't used, we can simply replace the 1st or the 3rd by code to use `node.flashindex()` to return the LFS module.  The supplied `_init` puts the LFS loader at entry 3, so if the module is in both SPIFFS and LFS, then the SPIFFS version will be loaded. One result of this has burnt me during development: if there is an out of date version in SPIFFS, then it will still get loaded instead of the one if LFS.

If you want to swap this search order so that the LFS is searched first, then SET `package.loaders[1] = loader_flash` in your `_init` code.  If you need to swap the search order temporarily for development or debugging, then do this after you've run the `_init` code:

```Lua
do local pl = package.loaders; pl[1],pl[3] = pl[3],pl[1]; end
```

#### Moving common string constants into LFS

LFS is mainly used to store compiled modules, but it also includes its own string table and any strings loaded into this can be used in your Lua application without taking any space in RAM. Hence, you might also want to preload any other frequently used strings into LFS as this will both save RAM use and reduced the Lua Garbage Collector (**LGC**) overheads.

The new debug function `debug.getstrings()` can help you determine what strings are worth adding to LFS.  It takes an optional string argument `'RAM'` (the default) or `'ROM'`, and returns a list of the strings in the corresponding table. So the following example can be used to get a listing of the strings in RAM.
```Lua
do
  local a=debug.getstrings'RAM'
  for i =1, #a do a[i] = ('%q'):format(a[i]) end
  print ('local preload='..table.concat(a,','))
end
```

You can do this at the interactive prompt or call it as a debug function during a running application in order to generate this string list, (but note that calling this still creates the overhead of an array in RAM, so you do need to have enough "head room" to do the call).

You can then create a file, say `LFS_dummy_strings.lua`, and insert these `local preload` lines into it.  By including this file in your `luac.cross` compile, then the cross compiler will also include all strings referenced in this dummy module in the generated ROM string table.  Note that you don''t need to call this module; it's inclusion in the LFS build is enough to add the strings to the ROM table. Once in the ROM table, then you can use them subsequently in your application without incurring any RAM or LGC overhead.

A useful starting point may be found in [lua_examples/lfs/dummy_strings.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/dummy_strings.lua); this saves about 4Kb of RAM by moving a lot of common compiler and Lua VM strings into ROM.

Another good use of this technique is when you have resources such as CSS, HTML and JS fragments that you want to output over the internet.  Instead of having lots of small resource files, you can just use string assignments in an LFS module and this will keep these constants in LFS instead.
 

## Technical issues

Whilst memory capacity isn't a material constraint on most conventional machines, the Lua RTS still includes some features to minimise overall memory usage. In particular:

-  The more resource intensive data types are know as _collectable objects_, and the RTS includes a LGC which regularly scans these collectable resources to determine which are no longer in use, so that their associated memory can be reclaimed and reused.

-  The Lua RTS also treats strings and compiled function code as collectable objects, so that these can also be LGCed when no longer referenced

The compiled code, as executed by Lua RTS, internally comprises one or more function prototypes (which use a `Proto` structure type) plus their associated vectors (constants, instructions and meta data for debug). Most of these compiled constant types are basic (e.g. numbers) and the only collectable constant data type are strings. The other collectable types such as arrays are actually created at runtime by executing Lua compiled instructions to build each resource dynamically.

When any Lua file is loaded without LFS into an ESP application, the RTS loads the corresponding compiled version into RAM. Each compiled function has its own Proto structure hierarchy, but this hierarchy is not exposed directly to the running application; instead the compiler generates `CLOSURE` instruction which is executed at runtime to bind the `Proto` to a Lua function value thus creating a [closure](https://en.wikipedia.org/wiki/Closure_(computer_programming)). Since this occurs at runtime, any `Proto` can be bound to multiple closures. A Lua closure can also have multiple RW [Upvalues](https://www.lua.org/pil/27.3.3.html) bound to it, and so function value is a Lua RW object in that it is referring to something that can contain RW state, even though the `Proto` hierarchy itself is intrinsically RO.

Whilst advanced ESP Lua programmers can use overlay techniques to ensure that only active functions are loaded into RAM and thus increase the effective application size, this adds to runtime and program complexity. Moving Lua "program" resources into ESP Flash addressable memory typically at least doubles the effective RAM available, and removes the need to complicate applications code by implementing overlaying.

Any RO resources that are relocated to a flash address space:

-  Must not be collected. Also RW references to RO resources must be robustly handled by the LGC.
-  Cannot reference to any volatile RW data elements (though RW resources can refer to RO resources).

All strings in Lua are [interned](https://en.wikipedia.org/wiki/String_interning), so that only one copy of any string is kept in memory, and most string manipulation uses the address of this single copy as a unique reference. This uniqueness and the LGC of strings is facilitated by using a global string table that is hooked into the Lua global state.  Within standard Lua VM, any new string is first resolved against RAM string table, so that only the string-misses are added to the string table. 

The LFS patch adds a second RO string table in flash and this contains all strings used in the LFS Protos. Maintaining integrity across the two string tables is simple and low-cost, with LFS resolution process extended across both the RAM and ROM string tables. Hence any strings already in the ROM string table already have a unique string reference avoiding the need to add an additional entry in the RAM table. This both significantly reduces the size of the RAM string table, and removes a lot of strings from the LCG scanning.

Note that my early development implementations of the LFS build process allowed on-target ESP builds, but I found that the Lua compiler was too resource hungry for usable application sizes, and  it was impractical to get this approach to scale. So we abandoned this approach and moved the LFS build process onto the development host machine by embedding this into `luac.cross`. This approach also avoids all of the update integrity issues involved in building a new LFS which might require RO resources already referenced in the RW ones.

A LFS image can be loaded in the LFS store by one of two mechanisms:

-  The image can be build on the host and then copied into SPIFFS. Calling the `node.flashreload()` API with this filename will load the image, and then schedule a restart to leave the ESP in normal application mode, but with an updated flash block.  This sequence is essentially atomic. Once called, and the format of the LFS image has been valiated, then the only exit is the reboot.

-  The second option is to build the LFS image using the `-a` option to base it at the correct absolute address of the LFS store for a given firmware image.  The LFS can then be flashed to the ESP along with the firmware image.

The LFS store is a fixed size for any given firmware build (configurable by the a pplication developer through `user_config.h`) and is at a build-specific base address within the `ICACHE_FLASH` address space.  This is used to store the ROM string table and the set of `Proto` hierarchies corresponding to a list of Lua files in the loaded image.

A separate `node.flashindex()` function creates a new Lua closure based on a module loaded into LFS and more specfically its flash-based prototype; whilst this access function is not transparent at a coding level, this is no different functionally than already having to handle `lua` and `lc` files and the existing range of load functions (`load`,`loadfile`, `loadstring`). Either way, creating a closure on flash-based prototype is _fast_ in terms of runtime. (It is basically a single instruction rather than a compile, and it has minimal RAM impact.)

### Implementation details

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

Such LFS images are created by `luac.cross` using the `-f` option, and this builds a flash image using the list of modules provided but with a master "main" function of the form:

```Lua
local n = ...,1518283691  -- The Unix Time of the compile
if n == "module1" then return module1 end
if n == "module2" then return module2 end
-- and so on
if n == "moduleN" then return module2 end
return 1518283691,"module1","module2", --[[ ... ]] ""moduleN"
```

Note that you can't actually code this Lua because the modules are in separate compilation units, but the compiler being a compiler can just emit the compiled code directly. (See `app/lua/luac_cross/luac.c` for the details.)

The deep cross-copy of the compiled `Proto` hierarchy is also complicated because current hosts are typically 64bit whereas the ESPs are 32bit, so the structures need repacking. (See `app/lua/luac_cross/luac.c` for the details.)

This patch moves the `luac.cross` build into the overall application make hierarchy and so it is now simply a part of the NodeMCU make. The old Lua script has been removed from the `tools` directory, together with the need to have Lua pre-installed on the host.

The LFS image is by default position independent, so is independent of the actual NodeMCU target image. You just have to copy it to the target file system and execute a `flashreload` and this copies the image from SPIFSS to the correct flash location, relocating all address to the correct base. (See `app/lua/lflash.c` for the details.)  This process is fast.  

A `luac.cross -a` option also allows absolute address images to be built for direct flashing the LFS store onto the module during provisioning.

### Impact of the Lua Garbage Collector

The LGC applies to what the Lua VM classifies as collectable objects (strings, tables, functions, userdata, threads -- known collectively as `GCObjects`). A simple two "colour" LGC was used in previous Lua versions, but Lua 5.1 introduced the Dijkstra's 3-colour (*white*, *grey*, *black*) variant that enabled the LGC to operate in an incremental mode. This permits smaller LGC steps interspersed by LGC pause, and is very useful for larger scale Lua implementations. Whilst this is probably not really needed for IoT devices, NodeMCU follows this standard Lua 5.1 implementation, albeit with the `elua` EGC changes.

In fact, two *white* flavours are used to support incremental working (so this 3-colour algorithm really uses 4). All newly allocated collectable objects are marked as the current *white*, and a link in `GCObject` header enables scanning through all such Lua objects. Collectable objects can be referenced directly or indirectly via one of the Lua application's *roots*: the global environment, the Lua registry and the stack. 

The standard LGC algorithm is quite complex and assumes that all GCObjects are RW so that a flag byte within each object can be updated during the mark and sweep processing. LFS introduces GCObjects that are stored in RO memory and are therefore truly RO.   
The LFS patch therefore modifies the LGC processing to avoid such updates to GCObjects in RO memory, whilst still maintaining overall object integrity, as any attempt to update their content during LGC will result in the firmware crashing with a memory exception; the remainder of this section provides further detail on how this was achieved.  The LGC operates two broad phases: **mark** and **sweep**

-  The **mark** phase walks collectable objects by a recursive walk starting at at the LGC roots. (This is referred to as _traverse_.) Any object that is visited in this walk has its colour flipped from *white* to *grey* to denote that it is in use, and it is relinked into a grey list. The grey list is iteratively processed, removing one grey object at a time. Such objects can reference other objects (e.g. a table has many keys and values which can also be collectable objects), so each one is then also traversed and all objects reachable from it are marked, as above. After an object has been traversed, it's turned from grey to black. The LGC will walks all RW collectable objects, traversing the dependents of each in turn.  As RW objects can now refer to RO ones, the traverse routines has additinal tests to skip trying to mark any RO LFS references.

-  The white flavour is flipped just before entering the **sweep** phase. This phase then loops over all collectable objects. Any objects found with previous white are no longer in use, and so can be freed. The 'current' white are kept; this prevents any new objects created during a paused sweep from being accidentally collected before being marked, but this means that it takes two sweeps to free all unused objects. There are other subtleties introduced in this 3-colour algorithm such as barriers and back-tracking to maintain integrity of the LGC, and these also needed extra rules to handle RO GCObjects correclty, but detailed explanation of these is really outside the scope of this paper. 

As well as standard collectable GCOobjets:

-  Standard Lua has the concept of **fixed** objects. (E.g. the main thread). These won't be collected by the LGC, but they may refer to objects that aren't fixed, so the LGC still has to walk through an fixed objects.

-  eLua added the the concept of **readonly** objects, which confusingly are a hybrid RW/RO implementation, where the underlying string resource is stored as a program constant in flash memory but the `TSstring` structure which points to this is still kept in RAM and can by GCed, except that in this case the LGC does not free the RO string constant itself.

-  LFS introduces a third variant **flash** object for `LUA_TPROTO` and `LUA_TSTRING` types. Flash objects can only refer to other flash objects and are entirely located in the LFS area in flash memory.

The LGC already processed the _fixed_ and _readonly_ object, albeit as special cases. In the case of _flash_ GCObjects, the `mark` flag is in read-only memory and therefore the LGC clearly can't use this as a RW flag in its mark and sweep processing. So the LGC skips any marking operations for flash objects. Likewise, where all other GCObjects are linked into one of a number of sweeplists using the object's `gclist` field. In the case of flash objects, the compiler presets the `mark` and `gclist` fields with the fixed and readonly mark bits set, and the list pointer to `NULL` during the compile process.

As far as the LGC algorithm is concerned, encountering any _flash_ object in a sweep is a dead end, so that branch of the walk of the GCObject hierarchy can be terminated on encountering a flash object. This in practice all _flash_ objects are entirely removed from the LGC process, without compromising collection of RW resources.

### General comments

- **Reboot implementation**. Whilst the application initiated LFS reload might seem an overhead, it typically only adds a few seconds per reboot. 

- **LGC reduction**. Since the cost of LGC is directly related to the size of the LGC sweep lists, moving RO resources into LFS memory removes them from the LGC scope and therefore reduces LGC runtime accordingly.

- **Typical Usecase**.  The rebuilding of a store is an occasional step in the development cycle. (Say up to 10-20 times a day in a typical intensive development process). Modules and source files under development can also be executed from SPIFFS in `.lua` format. The developer is free to reorder the `package.loaders` and load any SPIFFS files in preference to Flash ones. And if stable code is moved into Flash, then there is little to be gained in storing development Lua code in SPIFFS in `lc` compiled format.

- **Flash caching coherency**.  The ESP chipset employs hardware enabled caching of the `ICACHE_FLASH` address space, and writing to the flash does not flush this cache.  However, in this restart model, the CPU is always restarted before any updates are read programmatically, so this (lack of) coherence isn't an issue.

- **Failsafe reversion**. Since the entire image is precompiled and validated before loading into LFS, the chances of failure during reload are small. The loader uses the Flash NAND rules to write the flash header flag in two parts: one at start of the load and again at the end. If on reboot, the flag in on incostent state, then the LFS is cleared and disabled until the next reload.

