# File LFS module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-11-30 | [vsky279](https://github.com/vsky279) | [vsky279](https://github.com/vsky279) | [file_lfs.lua](../../lua_modules/file_lfs/file_lfs.lua)|

Provides access to arbitrary files stored in LFS.

An arbitrary file can be stored in LFS and still can be accessed using `file` functions. This module is an overlay over `file` base functions providing access to such LFS files.

The module tries to be transparent as much as possible so it makes no difference where LFS file or standard SPIFFS file is accessed. LFS file is read only. If the file is open for writing a standard SPIFFS file is opened instead.
Both basic and object model can be used to access LFS file (see [file](../../docs/modules/file.md) module documentation).

## `resource.lua` file

Files to be stored in LFS needs to be preprocessed, i.e. a Lua file with its contents needs to be generated. This file called `resource.lua` is then included in the LFS image. A Lua script [`make_resource.lua`](../../lua_modules/file_lfs/make_resource.lua) can be used to generate `resource.lua` script.

A structure of the `resource.lua` file is simple
```Lua
local arg = ...
if arg == "index.html" then return "<!DOCTYPE html><html><body>Hi, there!</body></html>" end
if arg == "favicon.ico" then return ""\000\000\000\000\000\000..." end
```

## `make_resource.lua` script

Lua script to be run on PC to generate `resource.lua` file.

### Syntax
```bash
 ./make_resource.lua [-o outputfile] file1 [file2] 
 ```

### Example
Create `resource.lua` file with all files in the `resource` directory
```bash
./make_resource resource/*
```

## Basic usage
```Lua
file = require("file_lfs")

f = file.open("index.html") -- let's assume the above resource.lua file is embedded in LFS
print(f:readline())
-- prints: <!DOCTYPE html><html><body>Hi, there!</body></html>
f:close()

f = file.open("init.lua")
-- init.lua file is not stored in LFS (does not have entry in resource.lua stored in LFS) -> SPIFFS files is opened instead
print(f:readline())
f:close()
```

Methods implemented - basically all `file` module functions are available though only some of them work with LFS files. The other functions are just passed through to the base `file` functions.

## file_lfs.open()

Opens a LFS file if it is included in LFS in the `resource.lua` file. If not standard [`file.open()`](../../docs/modules/file.md#fileopen-fileobjopen) function is called.
LFS file is opened only when "r" access is requested.

#### Syntax
`file.open(filename, mode)`

#### Parameters
- `filename` file to be opened
- `mode`:
    - "r": read mode (the default)
    - "w": write mode
    - "a": append mode
    - "r+": update mode, all previous data is preserved
    - "w+": update mode, all previous data is erased
    - "a+": append update mode, previous data is preserved, writing is only allowed at the end of file

#### Returns
LFS file object (Lua table) or SPIFFS file object if file opened ok. `nil` if file not opened, or not exists (read modes).

## file_lfs.exists()

Checks whether the LFS or SPIFFS file exists.

#### Syntax
`file.exists(filename)`

#### Parameters
- `filename` file to check

#### Returns
true of the file exists (even if 0 bytes in size), and false if it does not exist


## file.read(), file.obj:read()

Read content from the open file. It has the same parameters and return values as [`file.read()` / `file.obj:read()`](../../docs/modules/file.md#fileopen-fileobjopen#fileread-fileobjread)

#### Syntax
`file.read([n_or_char])`

`fd:read([n_or_char])`

#### Parameters
- `n_or_char`:
	- if nothing passed in, then read up to `FILE_READ_CHUNK` bytes or the entire file (whichever is smaller).
	- if passed a number `n`, then read up to `n` bytes or the entire file (whichever is smaller).
	- if passed a string containing the single character `char`, then read until `char` appears next in the file, `FILE_READ_CHUNK` bytes have been read, or EOF is reached.

#### Returns
File content as a string, or nil when EOF


## file.readline(), file.obj:readline()

Read the next line from the open file. Lines are defined as zero or more bytes ending with a EOL ('\n') byte. If the next line is longer than 1024, this function only returns the first 1024 bytes.
It has the same parameters and return values as [`file.readline()` / `file.obj:readline()`](../../docs/modules/file.md#fileopen-fileobjopen#filereadline-fileobjreadline)


#### Syntax
`file.readline()`

`fd:readline()`

#### Parameters
none

#### Returns
File content in string, line by line, including EOL('\n'). Return `nil` when EOF.


## file.seek(), file.obj:seek()

Sets and gets the file position, measured from the beginning of the file, to the position given by offset plus a base specified by the string whence.
It has the same parameters and return values as [`file.seek()` / `file.obj:seek()`](../../docs/modules/file.md#fileopen-fileobjopen#fileseek-fileobjseek)

#### Syntax
`file.seek([whence [, offset]])`

`fd:seek([whence [, offset]])`

#### Parameters
- `whence`
	- "set": base is position 0 (beginning of the file)
	- "cur": base is current position (default value)
	- "end": base is end of file
- `offset` default 0

If no parameters are given, the function simply returns the current file offset.

#### Returns
the resulting file position, or `nil` on error
