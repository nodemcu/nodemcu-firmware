# file Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [file.c](../../../app/modules/file.c)|

The file module provides access to the file system and its individual files.

The file system is a flat file system, with no notion of directories/folders.

Only one file can be open at any given time.

## file.close()

Closes the open file, if any.

#### Syntax
`file.close()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
-- open 'init.lua', print the first line.
file.open("init.lua", "r")
print(file.readline())
file.close()
```
#### See also
[`file.open()`](#fileopen)

## file.exists()

Determines whether the specified file exists.

#### Syntax
`file.exists(filename)`

#### Parameters
- `filename` file to check

#### Returns
true of the file exists (even if 0 bytes in size), and false if it does not exist

#### Example

```lua
files = file.list()
if files["device.config"] then
    print("Config file exists")
end

if file.exists("device.config") then
    print("Config file exists")
end
```
#### See also
[`file.list()`](#filelist)

## file.flush()

Flushes any pending writes to the file system, ensuring no data is lost on a restart. Closing the open file using [`file.close()`](#fileclose) performs an implicit flush as well.

#### Syntax
`file.flush()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
-- open 'init.lua' in 'a+' mode
file.open("init.lua", "a+")
-- write 'foo bar' to the end of the file
file.write('foo bar')
file.flush()
-- write 'baz' too
file.write('baz')
file.close()
```
#### See also
[`file.close()`](#fileclose)

## file.format()

Format the file system. Completely erases any existing file system and writes a new one. Depending on the size of the flash chip in the ESP, this may take several seconds.

#### Syntax
`file.format()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`file.remove()`](#fileremove)

## file.fscfg ()

Returns the flash address and physical size of the file system area, in bytes.

#### Syntax
`file.fscfg()`

#### Parameters
none

#### Returns
- `flash address` (number)
- `size` (number)

#### Example
```lua
print(string.format("0x%x", file.fscfg()))
```

## file.fsinfo()

Return size information for the file system, in bytes.

#### Syntax
`file.fsinfo()`

#### Parameters
none

#### Returns
- `remaining` (number)
- `used`      (number)
- `total`     (number)

#### Example

```lua
-- get file system info
remaining, used, total=file.fsinfo()
print("\nFile system info:\nTotal : "..total.." Bytes\nUsed : "..used.." Bytes\nRemain: "..remaining.." Bytes\n")
```

## file.list()

Lists all files in the file system.

#### Syntax
`file.list()`

#### Parameters
none

#### Returns
a lua table which contains the {file name: file size} pairs

#### Example
```lua
l = file.list();
for k,v in pairs(l) do
  print("name:"..k..", size:"..v)
end
```

## file.open()

Opens a file for access, potentially creating it (for write modes).

When done with the file, it must be closed using `file.close()`.

#### Syntax
`file.open(filename, mode)`

#### Parameters
- `filename` file to be opened, directories are not supported
- `mode`:
    - "r": read mode (the default)
    - "w": write mode
    - "a": append mode
    - "r+": update mode, all previous data is preserved
    - "w+": update mode, all previous data is erased
    - "a+": append update mode, previous data is preserved, writing is only allowed at the end of file

#### Returns
`nil` if file not opened, or not exists (read modes).  `true` if file opened ok.

#### Example
```lua
-- open 'init.lua', print the first line.
file.open("init.lua", "r")
print(file.readline())
file.close()
```
#### See also
- [`file.close()`](#fileclose)
- [`file.readline()`](#filereadline)

## file.read()

Read content from the open file.

#### Syntax
`file.read([n_or_str])`

#### Parameters
- `n_or_str`:
	- if nothing passed in, read up to `LUAL_BUFFERSIZE` bytes (default 1024) or the entire file (whichever is smaller)
	- if passed a number n, then read the file until the lesser of `n` bytes, `LUAL_BUFFERSIZE` bytes, or EOF is reached. Specifying a number larger than the buffer size will read the buffer size.
	- if passed a string `str`, then read until `str` appears next in the file, `LUAL_BUFFERSIZE` bytes have been read, or EOF is reached

#### Returns
File content as a string, or nil when EOF

#### Example
```lua
-- print the first line of 'init.lua'
file.open("init.lua", "r")
print(file.read('\n'))
file.close()

-- print the first 5 bytes of 'init.lua'
file.open("init.lua", "r")
print(file.read(5))
file.close()
```

#### See also
- [`file.open()`](#fileopen)
- [`file.readline()`](#filereadline)

## file.readline()

Read the next line from the open file. Lines are defined as zero or more bytes ending with a EOL ('\n') byte. If the next line is longer than `LUAL_BUFFERSIZE`, this function only returns the first `LUAL_BUFFERSIZE` bytes (this is 1024 bytes by default).

#### Syntax
`file.readline()`

#### Parameters
none

#### Returns
File content in string, line by line, including EOL('\n'). Return `nil` when EOF.

#### Example
```lua
-- print the first line of 'init.lua'
file.open("init.lua", "r")
print(file.readline())
file.close()
```
#### See also
- [`file.open()`](#fileopen)
- [`file.close()`](#fileclose)
- [`file.read()`](#filereade)

## file.remove()

Remove a file from the file system. The file must not be currently open.

###Syntax
`file.remove(filename)`

#### Parameters
`filename` file to remove

#### Returns
`nil`

#### Example

```lua
-- remove "foo.lua" from file system.
file.remove("foo.lua")
```
#### See also
[`file.open()`](#fileopen)

## file.rename()

Renames a file. If a file is currently open, it will be closed first.

#### Syntax
`file.rename(oldname, newname)`

#### Parameters
- `oldname` old file name
- `newname` new file name

#### Returns
`true` on success, `false` on error.

#### Example

```lua
-- rename file 'temp.lua' to 'init.lua'.
file.rename("temp.lua","init.lua")
```

## file.seek()
Sets and gets the file position, measured from the beginning of the file, to the position given by offset plus a base specified by the string whence.

#### Syntax
`file.seek([whence [, offset]])`

#### Parameters
- `whence`
	- "set": base is position 0 (beginning of the file)
	- "cur": base is current position (default value)
	- "end": base is end of file
- `offset` default 0

If no parameters are given, the function simply returns the current file offset.

#### Returns
the resulting file position, or `nil` on error

#### Example
```lua
file.open("init.lua", "r")
-- skip the first 5 bytes of the file
file.seek("set", 5)
print(file.readline())
file.close()
```
#### See also
[`file.open()`](#fileopen)

## file.write()

Write a string to the open file.

#### Syntax
`file.write(string)`

#### Parameters
`string` content to be write to file

#### Returns
`true` if the write is ok, `nil` on error

#### Example
```lua
-- open 'init.lua' in 'a+' mode
file.open("init.lua", "a+")
-- write 'foo bar' to the end of the file
file.write('foo bar')
file.close()
```

#### See also
- [`file.open()`](#fileopen)
- [`file.writeline()`](#filewriteline)

## file.writeline()

Write a string to the open file and append '\n' at the end.

#### Syntax
`file.writeline(string)`

#### Parameters
`string` content to be write to file

#### Returns
`true` if write ok, `nil` on error

#### Example
```lua
-- open 'init.lua' in 'a+' mode
file.open("init.lua", "a+")
-- write 'foo bar' to the end of the file
file.writeline('foo bar')
file.close()
```

#### See also
- [`file.open()`](#fileopen)
- [`file.readline()`](#filereadline)
