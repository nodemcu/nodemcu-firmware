## Lua Compact Debug (LCD)

LCD (Lua Compact Debug) was developed in Sept 2015 by Terry Ellison as a patch to the Lua system to decrease the RAM usage of Lua scripts. This makes it possible to run larger Lua scripts on systems with limited RAM.  Its use is most typically for eLua-type applications, and in this version it targets the **NodeMCU** implementation for the ESP8266 chipsets.

This section gives a full description of LCD. If you are writing **NodeMCU** Lua modules, then this paper will be of interest to you, as it shows how to use LCD in an easy to configure way.   *Note that the default `user_config.h` has enabled LCD at a level 2 stripdebug since mid-2016*.

### Motivation

The main issue that led me to write this patch is the relatively high Lua memory consumption of its embedded debug information, as this typically results in a 60% memory increase for most Lua code.  This information is generated when any Lua source is complied because the Lua parser uses this as meta information during the compilation process.  It is then retained by default for use in generating debug information.  The only standard method of removing this information is to use the “strip” option when precompiling source using a standard eLua **luac.cross** on the host, or (in the case of NodeMCU) using the `node.compile()` function on the target environment.

Most application developers that are new to embedded development simply live with this overhead, because either they aren't familiar with these advanced techniques, or they want to keep the source line information in error messages for debugging.

The standard Lua compiler generates fixed 4 byte instructions which are interpreted by the Lua VM during execution. The debug information consists of a map from instruction count to source line number (4 bytes per instruction) and two tables keyed by the names of each local and upvalue. These tables contain metadata on these variables used in the function. This information can be accessed to enable symbolic debugging of Lua source (which isn't supported on **NodeMCU** platforms anyway), and the line number information is also used to generate error messages.

This overhead is sufficient large on limited RAM systems to replace this scheme by making two changes which optimize for space rather than time:

-  The encoding scheme used in this patch typically uses 1 byte per source line instead of 4 bytes per instruction, and this represents a 10 to 20-fold reduction in the size of this vector.  The access time during compile is still **O(1)**, and **O(N)** during runtime error handling, where **N** is number of non-blank lines in the function.  In practice this might add a few microseconds to the time take to generate the error message for typical embedded functions.

-  The line number, local and upvalue information is needed during the compilation of a given compilation unit (a source file or source string), but its only use after this is for debugging and so can be discarded. (This is what the `luac -s` option and `node.compile()` do).  The line number information if available is used in error reporting.  An extra API call has therefore been added to discarded this debug information on completion of the compilation.

To minimise the impact within the C source code for the Lua system, an extra system define **LUA_OPTIMIZE_DEBUG** can be set in the `user_config.h` file to configure a given firmware build. This define sets the default value for all compiles and can take one of four values:

1.  (or not defined) use the default Lua scheme.
2.  Use compact line encoding scheme and retain all debug information.
3.  Use compact line encoding scheme and only retain line number debug information.
4.  Discard all debug information on completion of compile.

Building the firmware with the 0 option compiles to the pre-patch version. Options 1-3 generate the `strip_debug()` function, which allows this default value to be set at runtime.

_Note that options 2 and 3 can also change the default behaviour of the `loadstring()` function in that any functions declared within the string cannot inherited any outer locals within the parent hierarchy as upvalues if these have been stripped of the locals and upvalues information._

### Details

There are various API calls which compile and load Lua source code. During compilation each variable name is parsed, and is then resolved in the following order:

-  Against the list of local variables declared so far in the current scope is scanned for a match.

-  Against the local variable lists for each of the lexically parent functions are then scanned for a match, and if found the variable is tagged as an _upvalue_.

-  If unmatched against either of these local scopes then the variable defaults to being a global reference.

The parser and code generator must therefore access the line mapping, upvalues, and locals information tables maintained in each function Prototype header during source compilation.  This scoping scheme works because function compilation is recursive: if function A contains the definition of function B which contains the definition of function C, then the compilation of A is paused to compile B and this is in turn paused to compile C; then B complete and then A completes.

The variable meta information is stored in standard Lua tables which are allocated using the standard Lua doubling algorithm and hence they can contain a lot of unused space.  The parser therefore calls `close_func()` once compilation of a function has been completed to trim these vectors to the final sizes.

The patch makes the following if `LUA_OPTIMIZE_DEBUG` > 0. (The existing functionality is preserved if this define is zero or undefined.)

-  It adds an extra API call: `stripdebug([level[, function]])`  as discussed below.

-  It extends the trim logic in `close_func()` to replace this trim action by deleting the information according to the current default debug optimization level.

-  The `lineinfo` vector associated with each function is replaced by a `packedlineinfo` string using a run length encoding scheme that uses a repeat of an optional line number delta (this is omitted if the line offset is zero) and a count of the number of instruction generated for that source line.  This scheme uses roughly an **M** byte vector where **M** is the number of non-blank source lines, as opposed to a **4N** byte vector where **N** is the number of VM instruction.  This vector is built sequentially during code generation so it is this patch conditionally replaces the current map with an algorithm to generate the packed version on the fly.

The `stripdebug([level[, function]])` call is processed as follows:

-  If both arguments are omitted then the function returns the current default strip level.

-  If the function parameter is omitted, then the level is used as the default setting for future compiles.  The level must be 1-3 corresponding to the above debug optimization settings.  Hence if `stripdebug(3)` is included in **init.lua**, then all debug information will be stripped out of subsequently compiled functions. 

-  The function parameter if present is parsed in the same way as the function argument in `setfenv()` (except that the integer 0 level is not permitted, and this function tree corresponding to this scope is walked to implement this debug optimization level.

The `packedlineinfo` encoding scheme is as follows:

-  It comprises a repeat of (optional) line delta + VM instruction count (IC) for that line starting from a base line number of zero.  The line deltas are optional because line deltas of +1 are assumed as default and therefore not emitted.

-  ICs are stored as a single byte with the high bit set to zero.  Sequences longer than 126 instructions for a single sequence are rare, but can be are encoded using a multi byte sequence using 0 line deltas, e.g. 126 (0) 24 for a line generating 150 VM instructions.  The high bit is always unset, and note that this scheme reserves the code 0x7F as discussed below.

-  Line deltas are stored with the high bit set and are variable (little-endian) in length.  Since deltas are always delimited by an IC which has the top bit unset, the length of each delta can be determined from these delimiters.  Deltas are stored as signed ones-compliment with the sign bit in the second bit of low order byte, that is in the format (in binary) `1snnnnnnn [1nnnnnnn]*`, with `s` denoting the sign and `n…n` the value element using the following map.  This means that a single byte is used encode line deltas in the range -63 … 65; two bytes used to encode line deltas in the range -8191 … 8193, etc..
```C
  value = (sign == 1) ? -delta : delta - 2
```

-  This approach has no arbitrary limits, in that it can accommodate any line delta or IC count. Though in practice, most deltas are omitted and multi-byte sequences are rarely generated.

-  The codes 0x00 and 0x7F are reserved in this scheme.  This is because Lua allocates such growing vectors on a size-doubling basis.  The line info vector is always null terminated so that the standard **strlen()** function can be used to determine its length.  Any unused bytes between the last IC and the terminating null are filled with 0x7F.

The current mapping scheme has **O(1)** access, but with a code-space overhead of some 140%. This alternative approach has been designed to be space optimized rather than time optimized.  It requires the actual IC to line number map to be computed by linearly enumerating the string from the low instruction end during execution, resulting in an **O(N)** access cost, where **N** is the number of bytes in the encoded vector.  However, code generation builds this information incrementally, and so only appends to it (or occasionally updates the last element's line number), and the patch adds a couple of fields to the parser `FuncState` record to enable efficient **O(1)**  access during compilation.

### Testing

Essentially testing any eLua compiler or runtime changes are a total pain, because eLua is designed to be build against a **newlib**-based ELF.  Newlib uses a stripped down set of headers and libraries that are intended for embedded use (rather than being ran over a standard operating system).  Gdb support is effectively non-existent, so I found it just easier first to develop this code on a standard Lua build running under Linux (and therefore with full gdb support), and then port the patch to NodeMCU once tested and working.

I tested my patch in standard Lua built with "make generic" and against the [Lua 5.1 suite](http://lua-users.org/lists/lua-l/2006-03/msg00723.html). The test suite was an excellent testing tool, and it revealed a number of cases that exposed logic flaws in my approach, resulting from Lua's approach of not carrying out inline status testing by instead implementing a throw / catch strategy.  In fact I realised that I had to redesign the vector generation algorithm to handle this robustly.

As with all eLua builds the patch assumes Lua will not be executing in a multithreaded environment with OS threads running different lua_States. (This is also the case for the NodeMCU firmware). It executes the full test suite cleanly as maximum test levels and I also added some specific tests to cover new **stripdebug** usecases.

Once this testing was completed, I then ported the patch to the NodeMCU build.  This was pretty straight forward as this code is essentially independent of the NodeMCU functional changes. The only real issue as to ensure that the NodeMCU `c_strlen()` calls replaced the standard `strlen()`, etc.

I then built both `luac.cross` and firmware images with the patch disable to ensure binary compatibility with the non-patched version and then with the patch enabled at optimization level 3.

In use there is little noticeable difference other than the code size during development are pretty much the same as when running with `node.compile()` stripped code.  The new option 2 (retaining packed line info only) has such a minimal size impact that its worth using this all the time.  I've also added a separate patch to NodeMCU (which this assumes) so that errors now generate a full traceback.

### How to enable LCD

Enabling LCD is simple: all you need is a patched version and define `LUA_OPTIMIZE_DEBUG` at the default level that you want in `app/include/user_config.h` and do a normal make. 

Without this define enabled, the unpatched version is generated.

Note that since `node.compile()` strips all debug information, old **.lc** files generated by this command will still run under the patched firmware, but binary files which retain debug information will not work across patched and non-patched versions.

Other than optionally including a `node.stripdebug(N)` or whatever in your **init.lua**, the patch is otherwise transparent at an application level.
