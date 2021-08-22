## Background and Objectives

The NodeMCU firmware was historically based on the Lua 5.1.4 core with the `eLua` patch and other NodeMCU specific enhancements and optimisations ("**Lua51**").  This paper discusses the rebaselining of NodeMCU to the latest production Lua version 5.3.5 ("**Lua53**").  Our goals in this upgrade were:

-  NodeMCU now offers a current Lua version, 5.3.5 which is as functionally complete as practical.

-  Lua53 adopts a minimum change strategy against the standard Lua source code base, that is changes to the VM and runtime system will only be made where there is a compelling reasons for any change, for example Lua53 preserves some valuable NodeMCU enhancements previously introduced to Lua51, for example the addition of Lua VM support for constant Lua program and data being executable directly from Flash ROM in order to free up RAM for application use to mitigate the RAM limitations of ESP-class IoT devices

-  NodeMCU provides a clear and stable migration path for both existing hardware libraries and ESP Lua applications being migrated from Lua51 to Lua53.

-  The Lua53 implementation provides a common code base for ESP8266 and ESP32 architectures.  (The Lua51 implementation was forked with variant code bases for the two architectures.)

## Specific Design Decisions

-  The NodeMCU C module API is built on the standard Lua C API that is common across the Lua51 and Lua53 build environments but with limited changes needs to reflect our IoT changes.  Note that standard Lua 5.3 introduced some core functional and C API changes v.v 5.1; however, the use the standard Lua 5.3 compatibility modes largely hides these changes, though modules can make use of the `LUA_VERSION_NUM` define should version-specific code variants be needed.

-  The historic NodeMCU C module API (following `eLua` precedent and) added some extensions that somewhat compromised the orthogonal design principles of the standard Lua API; these are that modules should only access the Lua runtime via the `lua_XXX` macros and and calls exported through the `lua.h` header (or the wrapped helper `luaL_XXXX` versions exported through the `lauxlib.h` header).  Such inconsistencies will be removed from the existing NodeMCU API and modules, so that all modules can be compiled and executed within either the Lua51 or the Lua53 environment.

-  Lua publishes a [Lua Reference Manual](https://www.lua.org/manual/5.3/)(**LRM**) for the Language specification, core libraries and C APIs for each Lua version.  The Lua53 implementation includes a supplemental [NodeMCU Reference Manual](nodemcu-lrm.md)(**NRM**) to document the NodeMCU extensions to the core libraries and C APIs.  As this API is unified across Lua51 and Lua53, this also provides a common reference that can also be used for developing both Lua51 and Lua53 modules.

-  The two Lua code bases are maintained within a common Git branch (in parallel NodeMCU sub-directories `app/lua` and `app/lua53`).  An optional make parameter `LUA=53` selects a Lua53 build based on `app/lua53`, thus generating a Lua 5.3 firmware image.  (At a later stage once Lua53 is proven and stable, we will swap the default to Lua53 and move the Lua51 tree into frozen support.)

-  Many of the important features of the `eLua` changes to Lua51 used by NodeMCU have now been incorporated into core Lua53 and can continue to be used 'out of the box'.   Other NodeMCU **LFS**, **ROTable** and **LCD** functionality has been rewritten for NodeMCU, so the Lua53 code base no longer uses the `eLua` patch.

-  Lua53 will ultimately support three build targets that correspond to the ESP8266, ESP32 and native host targets using a _common Lua53 source directory_.  The ESP build targets generate a firmware image for the corresponding ESP chip class, and the host target generates a host based `luac.cross` executable.  This last can either be built standalone or as a sub-make of the ESP builds.

-  The Lua53 host build `luac.cross` executable will continue to extend the standard functionality by adding support for LFS image compilation and include a Lua runtime execution environment that can be invoked with the `-e` option.  An optional make target also adds the [Lua Test Suite](https://www.lua.org/tests/) to this environment to enable use of this test suite.

-  Lua 5.3 introduces the concept of **subtypes** which are used for Numbers and Functions, and Lua53 follows this model adding an additional ROTables subtype for Tables.

   -  **Lua numbers** have separate **Integer** and **Floating point** subtypes.  There is therefore no advantage in having separate Integer and Floating point build variants.  Lua53 therefore ignores the `LUA_NUMBER_INTEGRAL` build option.  However it will provide option to use 32 or 64 bit numeric values with floating point numbers being stored as single or double precision respectively as well as the current hybrid where integers are 32-bit and `double` for floating point.  Hence 32-bit integer only applications will have similar memory use and runtime performance as existing Lua51 Integer builds.

   -  **Lua tables** have separate **Table** and **ROTable** subtypes.  The Lua53 implementation of this subtyping has been backported to Lua51 where these were previously separate types, so that the table access API is the same for both Lua51 and Lua53.

   -  **Lua Functions** have separate **Lua**, **C** and **lightweight C** subtypes, with this last being a special case where the C function has no upvals and so it doesn't need an associated `Closure` structure.  It is also the same TValue type as our lightweight functions.

-  Lua 5.3 also introduces another small number of other but significant Lua language and  core library / API changes.  Our Lua53 implementation does not limit these, though we have enabled the appropriate compatibility modes to limit the impact at a Lua developer level.  These are discussed in further detail in the following compatibility sections.

-  Many standard OS services aren't available on a embedded IoT device, so Lua53 follows the Lua51 precedent by omitting the `os` library for target builds as the relevant functionally is largely replaced by the `node` library.

-  Flash based code execution can incur runtime performance impacts if not mitigated. Some small code changes were required as the current GCC toolchain for the Xtensa processors doesn't handle all of these flash access mitigations during code generation.  For example, the ESP CPU only supports aligned word access to flash memory, so 'hot' byte constant accesses have been encapsulated in an inline assembler macro to remove non-aligned exceptions at a cost of one extra Xtensa instruction per access; the remaining byte accesses to flash use a software exception handler. 

-  NodeMCU employs a single threaded event loop model (somewhat akin to Node.js), and this is supported by some task extensions to the C API that facilitate use of a callback mechanism.

## Detailed Implementation Notes

We follow two broad principles (1) everything other than the Lua source directory is common, and (2) only change the Lua core when there is a compelling reason.  The following sub-sections describe these issues / changes in detail.

### Build variants and includes

Lua53 supports three build targets which correspond to:

-  The ESP8266 (and derivative ESP8385) architectures use the Espressif non-OS SDK and its GCC Xtensa tool-chain.  The macros `LUA_USE_ESP8266` and `LUA_USE_ESP` are defined for this target.

-  The ESP32 architecture using the Espressif IDF and its GCC Xtensa toolchain.  The macros `LUA_USE_ESP32` and `LUA_USE_ESP` are defined for this target.

-  A host architecture using the standard host C toolchain.  The macro `LUA_USE_HOST` is defined for this target.  We currently support any POSIX environment that supports the GCC toolchain and Windows builds using native MSVC, WSL, Cygwin and MinGW.

`LUA_USE_HOST` and `LUA_USE_ESP` are in effect mutually exclusive: `LUA_USE_ESP` is defined for a target firmware build, and `LUA_USE_HOST` for a build of the host-based `luac.cross` executable for the host environment.  Note that `LUA_USE_HOST` also defines `LUA_CROSS_COMPILER` as used in Lua51.

Our Lua51 source has been migrated to use `newlib` conformant headers and C runtime calls.  An example of this is that the standard SDK headers supplied by Espressif include `"c_string.h"` and use `c_strcmp()` as the string comparison function.  NodeMCU source files use `<string.h>` and `strcmp()`.

_**Caution**: The NodeMCU source is _compliant_ rather than fully _conformant_ with the standard headers such as `<string.h>`; that is the current subset of these APIs used by the code will successfully compile, link and execute as an image, but code additions which attempt to use extra functions defined in the APIs might not; this at least minimises the need to change standard source code to compile and run on the ESP8266._

As with Lua51, Lua53 heavily customises the `linit.c`, `lua.c` and `luac.c` files because of the demands of an embedded runtime environment.  Given the amount of change, these files are stripped of the functionally dead code.

### TString types and implementation

The Lua 5.3 version includes a significant modification to the treatment of strings, by dividing them into two separate subtypes based on the string length (at `LUAI_MAXSHORTLEN`, 40 in the current implementation).  This decision reflects two empirical observations based on a broad range of practical Lua applications: the longer the string, the less likely the application is to recreate it independently; and the cost of ensuring uniqueness increases linearly with the length of the string.  Hence Lua53 now treats the string type differently:

-  **Short Strings** are stored uniquely using the `strt` and `ROstrt` string table as discussed below.  Two short TStrings are identical if and only if their addresses are the same.

-  **Long Strings** are created and copied by reference, but are not guaranteed to be stored uniquely.

Since short strings are stored uniquely, identity comparison is based comparing their TString address.  For long TStrings identify comparison is a little more complex:

-  They are identical if their addresses are the same
-  They are different if their lengths are different.
-  Failing these short circuits, a full `memcmp()` must be carried out.

Lua GC of both types is essentially the same, except that collection of long strings does not need to update the `strt`.

Note that for real applications, identical long strings are rarely generated by other than by copy-reference and hence in general the runtime savings benefits exceed the small chance of storage duplication.  Also note that this and other sub-typing is hidden at the Lua C API level and is handled privately inside the Lua VM implementation.

Whilst running Lua applications make heavy use of TStrings, the Lua VM itself makes little use of TStrings and typically pushes any string literals as CStrings.  The Lua53 VM introduced a key cache to avoid the runtime cost of hashing string and doing the `strt` lookup for this type of CString constant.  The NodeMCU implementation shares this key cache with ROTable field resolution.

Short strings in the standard Lua VM are bound at runtime into the the RAM-based `strt` string table.  The LFS implementation adds a second LFS-based readonly `ROstrt` string table that is created during LFS image load and then referenced on subsequent CPU restarts. The Lua VM and C API resolves each new short string first against the `strt`, then the `ROstrt` string table, before adding any unresolved strings into the `strt`.  Hence short strings are interned across these two `strt` and `ROstrt` string tables.  Any runtime reference to strings already in the LFS `ROstrt` therefore do not create additional entries in RAM.  So applications are free include dummy resource functions (such as `dummy_strings.lua` in `lua_examples/lfs`) to preload additional strings into `ROstrt` and avoid needing RAM for these string constants.  Such dummy functions don't need to called; simply inclusion in the LFS build is sufficient.

An LFS section below discusses further implementation details.

### ROTables

The ROTables concept was introduced in `eLua`, with the ROTable format designed to be compiled by being declarable with C source and so statically included in the firmware at built-time, rather taking up RAM.  This essential functionality has been preserved across both Lua51 and Lua53.  At an API level ROTables are handled as a table subtype within the Lua VM except that:
-  ROTables are declared statically in C code.
-  Only a subset of key and value types is supported for ROTables.
-  Attempting to write to a ROTable field will raise an error.
-  The C API provides a method to push a ROTable reference direct to the Lua stack, but other than this, the Lua API to read ROTables and Tables is the same.

We have now completely replaced the `eLua` implementation for Lua53, and this implementation has been back-ported to Lua51.  Tables are now declared using `LROT` macros with the `LROT_END()` macro also generating a `ROTable` structure, which is a variant of the standard `Table` header and linking to the `luaR_entry` vector declared using the various `LROT_XXXXENTRY()` macros.  This has new implementation has some major advantages:

-  ROTables are a separate subtype of Table, and so only minor code changes are needed within `ltable.c` to implement this, with the implementation now effectively hidden from the rest of the runtime and any library modules; this has enabled the removal of most of the ROTable code patches introduced by `eLua` into Lua51.

-  The `luaR_entry` vector is a linear list, so (unlike a standard RAM Table) any ROTable has no associate hash table for fast key lookup.  However we have introduced a unified ROTable key cache to provide direct access into ROTable entries with a typical hit rate over 99% (key cache misses still require a linear key scan), and so average ROTable access is perhaps 30% slower than RAM Table access, unlike the `eLua` implementation which was 30-50 times slower. 

-  The `ROTable` structure variants are not GC collectable and so one of its fields is tagged to allow the Lua GC to short-circuit GC sweeps across such RO nodes.  The `ROTable` structure variant also drops unused fields to save space, and again this is handled internally within `ltable.c`.

-  As all tables have a header record that includes a valid `flags` field, the `fasttm()` optimisations for common metamethods now work for both `ROTables` and `Tables`.

The same Lua51 ROTable functionality and limitations also apply to Lua53 in order to minimise migration impact for C module libraries:

-  ROTables can only have string keys and a limited set of Lua value types (Numeric, Light CFunc, Light UserData, ROTable, string and Nil). In Lua 5.3 `Integer` and `Float` are now separate numeric subtypes, so `LROT_INTENTRY()` takes an integer value. The new `LROT_FLOATENTRY()` is used for a non-integer values. This isn't a migration issue as none of the current NodeMCU modules use floating point constants in ROTables declared in them. (There is the only one currently used and that is `math.PI`.)

   -  For 5.1 builds, `LROT_FLOATENTRY()` is a synonym of `LROT_NUMENTRY()`.
   -  For 5.3 builds, `LROT_NUMENTRY()` is a synonym of  `LROT_INTENTRY()`.

-  Some ordering limitations apply: `luaR_entry` vectors can be unordered _except for any metafields_:  Any entry with a key name starting in "_" _must_ must be ordered and placed at the start of the vector.

-  The `LROT_BEGIN()` and  `LROT_END()` take the same three parameters. (These are ignored in the case of the `LROT_BEGIN()` macro, but by convention these are the same to facilitate the begin / end pairing).
    -  The first field is the table name.
    -  The second field is used to reference the ROTable's metatable (or `NULL` if it doesn't have one).
    -  The third field is 0 unless the table is a metatable, in which case it is a bit mask used to define the `fasttm()` `flags` field.  This must match any metafield entries for metafield lookup to work correctly.

### Proto Structures

Standard Lua 5.3 contains a new peep hole optimisation relating to closures: the Proto structure now contains one RW field pointing to the last closure created, and the GC adopts a lazy approach to recovering these closures. When a new closure is created, if the old one exists _and the upvals are the same_ then it is reused instead of creating a new one. This allows peephole optimisation of a usecase where a function closure is embedded in a do loop, so the higher cost closure creation is done once rather than `n` times.

This reduces runtime at the cost of RAM overhead.  However for RAM limited IoTs this change introduced two major issues: first, LFS relies on Protos being read-only and this RW `cache` field breaks this assumption; second closures can now exist past their lifetime, and this delays their GC.  Memory constrained NodeMCU applications rely on the fact that dead closed upvals can be GCed once the closure is complete.  This optimisation changes this behaviour.  Not good.

Lua53 **_removes_** this optimisation for all prototypes.

### Locale support

Standard Lua 5.3 introduces localisation support.  NodeMCU Lua53 disables this because IoT implementation doesn't have the appropriate OS support.

### Memory Optimisations

Some Lua structures have `double` fields which are align(8) by default.  There is no reason or performance benefit for doing align(8) on ESPs so `#pragma pack(4)` is used for the value struct.

Lua53 also reimplements the Lua51 LCD (Lua Compact Debug) patch. This replaces the `sizecode` `ìnt` vector giving line info with a packed byte array that is typically 15-30× smaller.  See the [LCD whitepaper](lcd.md) for more information on this algorithm.

### Unaligned exception avoidance

By default the GCC compiler emits a `l8ui` instruction to access byte fields on the ESP8266 and ESP32 Xtensa processors. This instruction will generate an unaligned fetch exception when this byte field is in Flash memory (as will accessing short fields).  These exceptions are handled  by emulating the instruction in software using an unaligned access handler; this allows execution to continue albeit with the runtime cost of handling the exception in software. We wish to avoid the performance hit of executing this handler for such exceptions.

`lobject.h` now defines a `GET_BYTE_FN(name,t,wo,bo)` macro. In the case of host targets this macro generates the normal field access, but in the case of Xtensa targets uses of this macro define an `static inline` access function for each field.  These functions at the default `-O2` optimisation level cause the code generator to emit a pair of `l32i.n` + `extui` instructions replacing the single `l8ui` instruction.  This has the cost of an extra instruction execution for accessing RAM data, but also removes the 200+ clock overhead of the software exception handler in the case of flash memory accesses.

There are 9 byte fields in the `GCObject`,`TString`, `Proto`, `ROTable` structures that can either be statically compiled as `const struct` into library code space or generated by the Lua cross compiler and loaded into the LFS region; the `GET_BYTE_FN` macro is used to create inline access functions for these fields, and read references of the form `(o)->tt` (for example) have been recoded using the access macro form `gettt(o)`.  There are 44 such changed access references in the source which together represent perhaps 99% of potential sources of this software exception within the Lua VM. 

The access macro hasn't been used where access is guarded by a conditional that implies the field in a RAM structure and therefore the `l8ui` instruction is executed correctly in hardware.  Another exclusion is in modules such as `lcode.c` which are only used in compilation, and where the addition runtime penalty is acceptable.

A wider review of `const char` initialisers and `-S` asm output from the compiler confirms that there are few other cases of character loads of constant data, largely because inline character constants such as '@' are loaded into a register as an immediate parameter to a `movi.n` instruction.  Ditto use of short fields.

### Modulus and division operation avoidance

The Lua runtime uses the modulus (`%`) and divide (`/`) operators in a number of computations. This isn't an issue for most uses where the divisor is an integer power of 2 since the **gcc** optimiser substitutes a fast machine code equivalent which typically executes 1-4 inline Xtensa instructions (ditto for many constant multiplies).  The compiler will also fold any used in constant expressions to avoid runtime evaluation. However the ESP Xtensa CPU doesn't implement modulus and divide operations in hardware, so these generate a call to a subroutine such as `_udivsi3()` which typically involves 500 instructions or so to evaluate.  A couple of frequent uses have been replaced.  (I have ensured that such uses are space delimited, so searching for " % " will locate these. `grep -P " (%|/) (?!(2|4|8|16))" app/lua53/*.[hc]` will list them off.)

### Key cache

Standard Lua 5.3 introduced a string key cache for constant CString to TString lookup.  In NodeMCU we also introduced a lookaside cache for ROTable fields access, and in practice this provides single probe access for over 99% of key hit accesses to ROTable entries.

In Lua53 these two caching functions (for CString and ROTable key lookup) have been unified into a common key cache to provide both caching functions with the runtime overhead of a single cache table in RAM.  Folding these two lookups into a single key cache isn't ideal, but given our limited RAM this allows the cache use to be rebalanced at runtime reflecting the relative use of CString and ROTable key lookups.

### Flash image generation and loading

The current Lua51 `app/lua` implementation has two variants for dumping and loading Lua bytecode: (1) `ldump.c` + `lundump.c`; (2) `lflashimg.c` + `lflash.c`. These have been unified into a single load / unload mechanism in Lua53.  However, this mechanism must facilitate sequential loading into flash storage, which is straight forward _if_ with some small changes to the standard internal ordering of the LC file format.  The reason for this is that any `Proto` can embed other Proto definitions internally, creating a Proto hierarchy.  The standard Lua dump algorithm dumps some Proto header components, then recurses into any sub-Protos before completing the wrapping Proto dump.  As a result each Proto's resources get interleaved with those of its subordinate Proto hierarchy.  This means that resources get to written to RAM non-serially, which is bad news for writing serially to the LFS region.

The NodeMCU Lua53 dump reorders the proto hierarchy tree walk, so that resources of the lowest protos in the hierarchy are loaded first:
```
dump_proto(p)
  foreach subp in p
    dump_proto(subp)
  dump proto content
end
```
This results in Proto references now being backwards references to Protos that have already been loaded, and this in turn enables the Proto resources to be allocated as a sequential contiguous allocation units, so the same code can be used for loading compiled Lua code into RAM and into LFS.

The standard Lua 5.3 dump format embeds string constants in each proto as a `len`+`byte string` definition. NodeMCU needs to separate the collection of strings into an `ROstrt` for LFS loading, and this requires an extra processing pass either on dump or load.  By doing a preliminary Proto scan to collect tracking the strings used then dumping these as a prologue makes the load process on the ESP a single pass and avoids any need for string resolution tables in the ESP's RAM. The extra memory resources needed for this two-pass dump aren't a material issue in a PC environment.

Changes to `lundump.c` facilitate the addition of LFS mode. Writing to flash uses a record oriented write-once API. Once the flash cache has been flushed when updating the LFS region, this data can be directly accesses using the memory-mapped RO flash window, the resources are written directly to Flash without any allocation in RAM.

Both the `dump.c` and `lundump.c` are compiled into both the ESP firmware and the host-based luac cross compiler.  Both the host and ESP targets use the same integer and float formats (e.g. 32 bit, 32-bit IEEE) which simplifies loading and unloading.  However one complication is that the host `luac.cross` application might be compiled on either a 32 or 64 bit environment and must therefore accommodate either 4 or 8 byte address constants.  This is not an issue with the compiled Lua format since this uses the grammatical structure of the file format to derive resource relationships, rather than offsets or pointers.

We also have a requirement to generate binary compatible absolute LFS images for linking into firmware builds.  The host mode is tweaked to achieve this.  In this case the write buffer function returns the correct absolute ESP address which are 32-bit; this doesn't cause any execution issue in luac since these addressed are never used for access within `luac.cross`.  On 64-bit execution environments, it also repacks the Proto and other record formats on copy by discarding the top 32-bits of any address reference.

#### Handling embedded integers in the dump format

A typical dump contains a lot of integer fields, not only for Integer constants, but also for repeat count and lengths.  Most of these integers are small, so rather than using a fixed 4-byte field in the file stream all integers are unsigned and represented by a big-endian multi-byte encoding, 7 bits per byte, with the high-bit used as a continuation flag.  This means that integers 0..127 encode in 1 byte, 128..32,767 in 2 etc.  This multi-byte scheme has minimal overhead but reduces the size of typical `.lc` and `.img` by 10% with minimal extra processing and less than the cost of reading that extra 10% of bytes from the file system.

A separate dump type is used for negative integer constants where the constant `-x` is stored as `-(x+1)`.  Note that endianness isn't an issue since the stream is processed byte-wise, but using big-endian simplifies the load algorithm.

#### Handling LFS-based strings

The dump function for a individual Protos hierarchy for loading as an `.lc` file follows the standard convention of embedding strings inline as a `\<len>\<byte sequence>`.  Any LFS image contains a dump of all of the strings used in the LFS image Protos as an "all-strings" prologue; the Protos are then dumped into the image with string references using an index into the all-strings header.  This approach enables a fast one-pass algorithm for loading the LFS image; it is also a compact encoding strategy as string references typically use 1 or 2 byte integer offset in the image file.

One complication here is that in the standard Lua runtime start-up adds a set of special fixed strings to the `strt` that are also tagged to prevent GC. This could cause problems with the LFS image if any of these constants is used in the code.  To remove this conflict the LFS image loader _always_ automatically includes these fixed strings in the `ROstrt`. (This also moves an extra ~2Kb string constants from RAM to Flash as a side-effect.)  These fixed strings are omitted from the "all-strings prologue", even though the code itself can still use them.  The `llex.c` and `ltm.c` initialisers loop over internal `char *` lists to register these fixed strings. NodeMCU adds a couple of access methods to `llex.c` and `ltm.c` to enable the dump and load functions to process these lists and resolve strings against them.

#### Handling LFS top level functions

Lua functions in standard Lua 5.1 are represented by two variant Closure headers (for C and Lua functions).  In the case of Lua functions with upvals, the internal Protos can validly be bound to multiple function instances. `eLua` and Lua 5.3 introduced the concept of lightweight C functions as a separate function subtype that doesn't require a Closure record.  Note that a function variable in a Lua exists as a TValue referencing either the C function address or a Closure record; this Closure is not the same as the CallInfo records which are chained to track the current call chain and stack usage.

Whilst lightweight C functions can be declared statically as TValues in ROTables, there isn't a corresponding mechanism for declaring a ROTable containing LFS functions. This is because a Lua function TValue can only be created at runtime by executing a `CLOSURE` opcode within the Lua VM.  The Lua51 implementation avoids this issue by generating a top level Lua dispatch function that does the equivalent of emitting `if name == "moduleN" then return moduleN end` for each entry, and this takes 4 Lua opcodes per module entry. This lookup has an O(N) cost which becomes non-trivial as N grows large, and so Lua51 has a somewhat arbitrary limit of 50 for the maximum number modules in a LFS image.

In the Lua53 LFS implementation, the undump loader appends a `ROTable` to the LFS region which contains a set of entries `"module name"= Proto_address`.  These table values are store as lightweight userdata pointer and as such are not directly accessible via Lua but the NodeMCU C function that does LFS lookup can still retrieve the required Proto address, execute the `CLOSURE` and return the corresponding TValue.  Since this approach uses the standard table access API, which is a lot more efficient than the 4×N opcode `if` chain implementation.

### Garbage collection

Lua51 includes the eLua emergency GC, plus the various EGC tuning parameters that seem to be rarely used. The default setting (which most users use) is `node.egc.ALWAYS` which triggers a full GC before every memory allocation so the VM spends maybe 90% of its time doing full GC sweeps.

Standard Lua 5.3 has adopted the eLua EGC but without the EGC tuning parameters. (I have raised a separate GitHub issue to discuss this.) We extend the EGC with the functional equivalent of the `ON_MEM_LIMIT` setting with a negative parameter, that is only trigger the EGC with less than a preset free heap left. The runtime spends far less time in the GC and code typically runs perhaps 5× faster.

### Panic Handling

Standard Lua includes a throw / catch framework for handling errors.  (This has been slightly modified to enable yielding to work across C API calls, but this can be modification can ignored for the discussion of Panic handling.)  All calls to Lua execution are handled by `ldo.c` through one of two mechanisms:

-  All protected calls are handled via `luaD_rawrunprotected()` which links its C stack frame into the  `struct lua_longjmp` chain updating the head pointer at `L->errorJmp`.  Any `luaD_throw()` will `longjmp` up to this entry in the C stack, hence as long as there is at least one protected call in the call chain, the C call stack can be properly unrolled to the correct frame.

-  If no protected calls are on the Lua call stack, then `L->errorJmp` will be null and there is no established C stack level to unroll to.  In this case the `luaD_throw()` will directly call the `at_panic()` handler.  Since there is no valid stack frame to unroll to and execution cannot safely continue, so the only safe next step is to abort, which in our case restarts the processor.

Any Lua calls directly initiated through `lua.c` interpreter loop or through `luac.cross` are protected.  However NodeMCU applications can also establish C callbacks that are called directly by the SDK / event dispatcher.  The current practice is that these invoke their associated Lua CB using an unprotected call and hence the only safe option is to restart the processor on error.  With Lua53 we have introduced a new `luaL_pcallx()` call variant as a NodeMCU extension; this is new call is designed to be used within library CBs that execute Lua CB functions, and it is argument compatible with `lua_call()`, except that in the case of caught errors it will also return a negative call status.   This function establishes an error handler to protect the called function, and this is invoked on error at the error's stack level to provide a stack trace.

This error handler posts a task to a panic error handler (with the error string as an upval) before returning control to the invoking routine.  If the Lua registry entry `onerror` exists and is set to a function, then the handler calls this with the error string as an argument otherwise it calls standard `print` function by default.  This function can return `false` in which case the handler exits, otherwise it restarts the processor.  The application can use `node.setonerror()` to override the default "always restart" action if wanted (for example to write an error to a logfile or to a network syslog before restarting).  Note that `print` returns `nil` and this has the effect of printing the full error traceback before restarting the processor.

Currently all (bar 1) of the cases of such Lua callbacks within the NodeMCU C modules used a simple `lua_call()`, with the result that any runtime error executed a panic on error and reboots the processor. These call have all been replaced by the `luaL_pcallx()` variant, so control is always returned to the C routine, and a later post task report the error and restarts the processor.  Note that substituting library uses of `lua_call()` by `luaL_pcallx()` does changes processing paths in the case of thrown errors.  If the library CB function immediately returns control to the SDK/event scheduler after the call, then this is the correct behaviour.  However, in a few cases, the routine performs post-call clean-up and this adapt the logic depending on the return status.

### The `luac.cross` execution environment

As with Lua51, the Lua53 host-build `luac.cross` executable extends the standard functionality by adding support for LFS image compilation and also includes a Lua runtime execution environment that can be invoked with the `-e` option.  This environment was added primarily to facilitate in host testing (albeit with some limitations) of the NodeMCU.

This execution environment also emulates LFS loading and execution using the `-F` option to load an LFS image before running the `-e` script.  On POSIX environments this allocates the LFS region using a kernel extension, a page-aligned allocator and it also uses the kernel API to turn off write access to this region except during the simulated write to flash operations.  In this way unintended writes to the LFS region throw a H/W exception in a manner parallel to the ESP environment.

### API Compatibility for NodeMCU modules

The Lua public API has largely been preserved across both Lua versions.  Having done a difference analysis of the two API and in particular the standard `lua.h` and `lauxlib.h` headers which contain the public API as documented in the LRM 5.1 and 5.3, these differences are grouped into the following categories:

-  NodeMCU features that were in Lua51 and have also been added to Lua53 as part of this migration.
-  Differences (additions / removals / API changes) that we are not using in our modules and which can therefore be effectively ignored for the purposes of migration.
-  Some very nice feature enhancements in Lua53, for example the table access functions are now type `int` instead of type `void` and return the type of the accessed TValue.  These features have been back-ported to Lua51 so that modules can be coded using Lua53 best practice and still work in a Lua51 runtime environment.
-  Source differences which can be encapsulated through common macros will be removed by updating module code to use this common macro set.  In a very small number of cases module functionality will be recoded to employ this common API base.

Both the LRM and PiL make quite clear that the public API for C modules is as documented in `lua.h` and all its definitions start with `lua_`. This API strives for economy and orthogonality. The supplementary functions provided by the auxiliary library (`lauxlib.c`)  access Lua services and functions through the `lua.h` interface and without other reference to the internals of Lua; this is exposed through `lauxlib.h` and all its definitions start with `luaL_`.  All Lua library modules (such as `lstrlib.c` which implements `string`) conform to this policy and only access the Lua runtime via the `lua.h` and `lauxlib.h` interfaces.

There are significant changes to internal APIs between Lua51 and Lua53 and as exposed in the other "private" headers within the Lua source directory, and so any code using these APIs may fail to work across the two versions.

Both Lua51 and Lua53 have a concept of Lua core files, and these set the `LUA_CORE` define. In order to enforce limited access to the 'private' internal APIs, `#ifdef LUA_CORE` guards have been added to all private Lua headers effectively hiding them from application library and other non-core access.

One thing that this analysis has underline is that the project has been lax in the past about how we allow our modules to be implemented.  All existing modules have now been updated to use only the public API.  If any new or changed module required any of the 'internal' Lua headers to compile, then it is implemented incorrectly.


### Lua Language and Libary Compatibility for NodeMCU Lua modules

For the immediate future we will be supporting both builds based on both language variants, so Lua module writers either:
 -  Avoid using Lua 5.3 language features and implement their module in the common subset (this is currently our preferred approach);
 -  Or explicitly state any language constraints and include a test for `_VERSION=='Lua.5.3'` (or 5.1) in the module startup and explicitly error if incompatible.

### Other Implementation Notes

-  **Use of Linker Magic**. Lua51 introduced a set of linker-aware macros to allow NodeMCU C library modules to be marshalled by the GNU linker for firmware builds; Lua53 target builds maintain these to ensuring cross-version compatibility. However, the lua53 `luac.cross` build does all library marshalling in `linit.c` and this removes the need to try to emulate this strategy on the diverse host toolchains that we support for compiling `luac.cross`.

-  **Host / ESP interoperability**.  Our strategy is to build the firmware for the ESP target and `luac.cross` in the same make process.  This requires the host to use a little endian ANSI floating point host architecture such as x68, AMD64 or ARM, so that the LC binary formats are compatible.  This ain't a material constraint in practice.

-  **Emergency GC.** The Lua VM takes a more aggressive stance than the standard Lua version on triggering a GC sweep on heap exhaustion.  This is because we run in a small RAM size environment. This means that any resource allocation within the Lua API can trigger a GC sweep which can call __GC metamethods which in turn can require to stack to be resized.
