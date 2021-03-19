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

A structure of the `resource.lua` file is simple. It returns a string, i.e. file content, depending on filename parameter passed to it. It returns table with list of files stored when called without any parameter.
```Lua
local arg = ...
if arg == "index.html" then return "<!DOCTYPE html><html><body>Hi, there!</body></html>" end
if arg == "favicon.ico" then return ""\000\000\000\000\000\000..." end
if arg == nil then return {"index.html", "favicon.ico"} end
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

## file_lfs.list()

Lists all files in the file system. It works almost in the same way as [`file.list()`](../../docs/modules/file.md#filelist)

#### Syntax
`file.list([pattern], [SPIFFs_only])`

#### Parameters
- `pattern` only files matching the Lua pattern will be returned
- `SPIFFs_only` if not `nil` LFS files won't be included in the result (LFS files are returned only if the parameter is `nil`)

#### Returns
a Lua table which contains all {file name: file size} pairs, if no pattern
given.  If a pattern is given, only those file names matching the pattern
(interpreted as a traditional [Lua pattern](https://www.lua.org/pil/20.2.html),
not, say, a UNIX shell glob) will be included in the resulting table.
`file.list` will throw any errors encountered during pattern matching.


## file.rename()

Renames a file. If a file is currently open, it will be closed first. It works almost in the same way as [`file.rename()`](../../docs/modules/file.md#filerename)

#### Syntax
`file.rename(oldname, newname)`

#### Parameters
- `oldname` old file name
- `newname` new file name

#### Returns
`true` on success, `false` when the file is stored in LFS (so read-only) or on error

## file_lfs.open()

Opens a LFS file included in LFS in the `resource.lua` file. If it cannot be found in LFS not standard [`file.open()`](../../docs/modules/file.md#fileopen) function is called.
LFS file is opened only when "r" access is requested.

#### Syntax
`file.open(filename, mode)`

#### Parameters
- `filename` file to be opened
- `mode`:
    - "r": read mode (the default). If file of the same name is present in SPIFFS then SPIFFS file is opened instead of LFS file.
    - "w": write mode - as LFS file is read-only a SPIFFS file of the same name is created and opened for writing.
    - "r+", "w+", "a", "a+": as LFS file is read-only and all these modes allow file updates the LFS file is copied to SPIFFS and then it is opened with correspondig open mode.

#### Returns
LFS file object (Lua table) or SPIFFS file object if file opened ok. `nil` if file not opened, or not exists (read modes).

## file.read(), file.obj:read()

Read content from the open file. It has the same parameters and returns values as [`file.read()` / `file.obj:read()`](../../docs/modules/file.md#fileread-fileobjread)

#### Syntax
`file.read([n_or_char])`

`fd:read([n_or_char])`

#### Parameters
- `n_or_char`:
	- if nothing passed in, then read up to `FILE_READ_CHUNK` bytes or the entire file (whichever is smaller).
	- if passed a number `n`, then read up to `n` bytes or the entire file (whichever is smaller).
	- if passed a string containing the single character `char`, then read until `char` appears next in the file, `FILE_READ_CHUNK` bytes have been read, or EOF is reached.

#### Returns
File content as a string, or `nil` when EOF


## file.readline(), file.obj:readline()

Read the next line from the open file. Lines are defined as zero or more bytes ending with a EOL ('\n') byte. If the next line is longer than 1024, this function only returns the first 1024 bytes.
It has the same parameters and return values as [`file.readline()` / `file.obj:readline()`](../../docs/modules/file.md#filereadline-fileobjreadline)


#### Syntax
`file.readline()`

`fd:readline()`

#### Parameters
none

#### Returns
File content in string, line by line, including EOL('\n'). Return `nil` when EOF.


## file.seek(), file.obj:seek()

Sets and gets the file position, measured from the beginning of the file, to the position given by offset plus a base specified by the string whence.
It has the same parameters and return values as [`file.seek()` / `file.obj:seek()`](../../docs/modules/file.md#fileseek-fileobjseek)

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

## file.stat()

Get attribtues of a file or directory in a table. Elements of the table are:

- `size` file size in bytes
- `name` file name
- `time` table with time stamp information. Default is 1970-01-01 00:00:00 in case time stamps are not supported (on SPIFFS).

    - `year`
    - `mon`
    - `day`
    - `hour`
    - `min`
    - `sec`

- `is_dir` flag `true` if item is a directory, otherwise `false`
- `is_rdonly` flag `true` if item is read-only, otherwise `false`
- `is_hidden` flag `true` if item is hidden, otherwise `false`
- `is_sys` flag `true` if item is system, otherwise `false`
- `is_arch` flag `true` if item is archive, otherwise `false`
- `is_LFS` flag `true` if item is stored in LFS, otherwise it is not present in the `file.stat()` result table - **the only difference to `file.stat()`**

#### Syntax
`file.stat(filename)`

#### Parameters
`filename` file name

#### Returns
table containing file attributes

