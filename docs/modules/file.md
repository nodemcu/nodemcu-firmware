# file Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [file.c](../../components/modules/file.c)|

Historically the file module provided both file meta information/manipulation
as well as file contents manipulation. These days file content manipulation
is handled via the standard Lua `io` module, and only meta information
is handled by the file module.

The file module operates on the virtual file system provided by the
Espressif VFS component, with a couple of exceptions. Specifically, the
[`fsinfo()`](#filefsinfo) and [`format()`](#fileformat) functions
operate only on the default SPIFFS file system, as configured in the
platform settings in Kconfig.


## file.exists()

Determines whether the specified file exists.

#### Syntax
`file.exists(filename)`

#### Parameters
- `filename` file to check

#### Returns
true if the file exists (even if 0 bytes in size), and false if it does not exist

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


## file.format()

Format the file system. Completely erases any existing file system and writes a new one. Depending on the size of the flash chip in the ESP, this may take several seconds.

!!! note

    Function is not supported for SD cards.

#### Syntax
`file.format()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`file.remove()`](#fileremove)


## file.fsinfo()

Return size information for the file system, in bytes.

!!! note

    Function is not supported for SD cards.

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
print("\nFile system info:\nTotal : "..total.." (k)Bytes\nUsed : "..used.." (k)Bytes\nRemain: "..remaining.." (k)Bytes\n")
```

## file.list()

Lists all files in the file system.

#### Syntax
`file.list([mountpoint])`

#### Parameters
`mountpoint` to list files in other file systems, the mountpoint can be given.

#### Returns
A lua table which contains the {file name: file size} pairs. For SPIFFS
file systems the size is returned in bytes, whereas for FAT file systems
the size is given in kilobytes.

#### Example
```lua
l = file.list();
for k,v in pairs(l) do
  print("name:"..k..", size:"..v)
end
```

## file.mkdir()

Creates a directory, provided the underlying file system supports directories. SPIFFS does not, but FAT (which you may have on an attached SD card) does.

#### Syntax
`file.mkdir(path [, mode])`

#### Parameters
- `path` the full path name of the directory to create. E.g. "/SD0/MYDIR".
- `mode` optional, only used for file systems which use mode permissions. Defaults to 0777 (octal).

#### Returns
`nil`

Throws an error if the directory could not be created. Error code 134 (at the
time of writing) indicates that the filesystem at the given path does not
support directories.

## file.rmdir()

Removes an empty directory, provided the underlying file system supports directories. SPIFFS does not, but FAT (which you may have on an attached SD card) does.

#### Syntax
`file.rmdir(path)`

#### Parameters
- `path` the path to the directory to remove. The directory must be empty.

#### Returns
`nil`

Throws an error if the directory could not be removed.

## file.remove()

Remove a file from the file system. The file must not be currently open.

#### Syntax
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


## file.rename()

Renames a file.

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
