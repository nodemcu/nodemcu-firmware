Whilst the Lua Virtual Machine (LVM) can compile Lua source dynamically and this can prove
very flexible during development, you will use less RAM resources if you precompile
your sources before execution.

## Compiling Lua directly on your ESP8266

-  The standard [string.dump \(function)](https://www.lua.org/manual/5.1/manual.html#pdf-string.dump) returns a string containing the binary code for the specified function and you can write this to a SPIFFS file.

-  [`node.compile()`](modules/node/#nodecompile) wraps this 'load and dump to file' operation into a single atomic library call.

The issue with both of these approaches is that compilation is RAM-intensive and hence
you will find that you will need to break your application into a lot of small and
compilable modules in order to avoid hitting RAM constraints.  This can be mitigated
by doing all compiles immediately after a [node.restart()`](modules/node/#noderestart).

## Compiling Lua on your PC for Uploading

If you install `lua` on your development PC or Laptop then you can use the standard Lua
compiler to syntax check any Lua source before downloading it to the ESP8266 module.  However,
the NodeMCU compiler output uses different data types (e.g. it supports ROMtables) so the
compiled output from standard `luac` cannot run on the ESP8266.

Compiling source on one platform for use on another (e.g. Intel 64-bit Windows to ESP8266) is
known as _cross-compilation_ and the NodeMCU firmware build now automatically generates
a `luac.cross` image as standard in the firmware root directory; this can be used to
compile and to syntax-check Lua source on the Development machine for execution under
NodeMCU Lua on the ESP8266.

`luac.cross` will translate Lua source files into binary files that can be later loaded
and executed by the LVM.  Such binary files, which normally have the `.lc` (lua code)
extension are loaded directly by the LVM without the RAM overhead of compilation.

Each `luac.cross` execution produces a single output file containing the bytecodes
for all source files given in the output file `luac.out`, but you would normally
change this with the `-o` option. If you wish you can mix Lua source files (and
even Lua binary files) on the command line. You can use '-' to indicate the
standard input as a source file and '--' to signal the end of options (that is, all
remaining arguments will be treated as files even if they start with '-').

`luac.cross` supports the standard `luac` options `-l`, `-o`, `-p`, `-s` and `-v`,
as well as the `-h` option which produces the current help overview.

NodeMCU also implements some major extensions to support the use of the
[Lua Flash Store (LFS)](lfs.md)), in that it can produce an LFS image file which
is loaded as an overlay into the firmware in flash memory; the LVM can access and
execute this code directly from flash without needing to store code in RAM.  This
mode is enabled by specifying the `-f`option.

`luac.cross` supports two separate image formats:

-  **Compact relocatable**. This is selected by the `-f` option. Here the compiler compresses the compiled binary so that image is small for downloading over Wifi/WAN (e.g. a full 64Kb LFS image is compressed down to a 22Kb file.) The LVM processes such image in two passes with the integrity of the image validated on the first, and the LFS itself gets updated on the second.  The LVM also checks that the image will fit in the allocated LFS region before loading, but you can also use the `-m` option to throw a compile error if the image is too large, for example `-m 0x10000` will raise an error if the image will not load into a 64Kb regions.

-  **Absolute**. This is selected by the `-a <baseAddr>` option. Here the compiler fixes all addresses relative to the base address specified. This allows an LFS absolute image to be loaded directly into the ESP flash using a tool such as  `esptool.py`.

These two modes target two separate use cases: the compact relocatable format
facilitates simple OTA updates to an LFS based Lua application; the absolute format
facilitates factory installation of LFS based applications.

Also note that the `app/lua/luac_cross` make and Makefile can be executed to build
just the `luac.cross` image.  You must first ensure that the following options in
`app/include/user_config.h` are matched to your target configuration:

```c
//#define LUA_NUMBER_INTEGRAL       // uncomment if you want an integer build
//#define LUA_FLASH_STORE 0x10000   // uncomment if you LFS support
```

Developers have successfully built this on Linux (including docker builds), MacOS, Win10/WSL and WinX/Cygwin.
