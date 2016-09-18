# SPIFFS File System

The NodeMCU project uses the [SPIFFS](https://github.com/pellepl/spiffs) 
filesystem to store files in the flash chip. The technical details about how this is configured can be found below, along with various build time options.

# spiffsimg - Manipulate SPI Flash File System disk images

Ever wished you could prepare a SPIFFS image offline and flash the whole
thing onto your microprocessor's storage instead of painstakingly upload
file-by-file through your app on the micro? With spiffsimg you can!

NodeMCU uses a SPIFFS filesystem that knows how big it is -- i.e. when you build a file system
image, it must fit into the flash chip, and it cannot be expanded once flashed.
It is important to give the `spiffimg` tool the correct size. You can provide either the `-c` option or both the `-U` and `-S` options.

### Syntax 

```
spiffsimg -f <filename> 
	[-o <offsetfile>]
	[-c <size>] 
	[-S <flashsize>]
	[-U <usedsize>]
	[-d]
	[-l | -i | -r <scriptname> ]
```

### Supported operations:

  * `-f` specifies the filename for the disk image. '%x' will be replaced by the calculated offset of the file system (`-U` must also be specified to calculate the offset).
  * `-o` specifies the filename which is to contain the calculated offset.
  * `-S` specifies the size of the flash chip. `32m` is 32 mbits, `4MB` is 4 megabytes.
  * `-U` specifies the amount of flash used by the firmware. Decimal or Hex bytes (if starts with 0x).
  * `-c` Create a blank disk image of the given size. Decimal or Hex bytes (if starts with 0x).
  * `-l` List the contents of the given disk image.
  * `-i` Interactive commands.
  * `-r` Scripted commands from filename.
  * `-d` causes the disk image to be deleted on error. This makes it easier to script.

### Available commands:

  * `ls` List contents. Output format is {type} {size} {name}.
  * `cat <filename>` Dump file contents to stdout.
  * `rm <filename>` Delete file.
  * `info` Display SPIFFS usage estimates.
  * `import <srcfile> <spiffsname>` Import a file into the disk image.
  * `export <spiffsname> <dstfile>` Export a file from the disk image.

### Example:
```lua
# spiffsimg -f flash.img -S 32m -U 524288 -i
> import myapp/lua/init.lua init.lua
> import myapp/lua/httpd.lua httpd.lua
> import myapp/html/index.html http/index.html
> import myapp/html/favicon.ico http/favicon.ico
> ls
f    122 init.lua
f   5169 httpd.lua
f   2121 http/index.html
f    880 http/favicon.ico
>^D
#
```

### Known limitations:

  * The block & page sizes are hard-coded to be compatible with nodemcu.
  * Error handling is not entirely consistent, some errors result in an
    early exit, others just print an error (both cause a non-zero exit though).
  * Only flat SPIFFS is supported.


# Technical Details

The SPIFFS configuration is 4k sectors (the only size supported by the SDK) and 8k blocks. 256 byte pages. Magic is enabled and magic_len is also enabled. This allows the firmware to find the start of the filesystem (and also the size).
One of the goals is to make the filsystem more persistent across reflashing of the firmware. However, there are still cases
where spiffs detects a filesystem and uses it when it isn't valid. If you are getting weirdness with the filesystem, then just reformat it.

There are two significant sizes of flash -- the 512K and 4M (or bigger). 

The file system has to start on a 4k boundary, but since it ends on a much bigger boundary (a 16k boundary), it also starts on an 8k boundary. For the small flash chip, there is 
not much spare space, so a newly formatted file system will start as low as possible (to get as much space as possible). For the large flash, the 
file system will start on a 64k boundary. A newly formatted file system will start between 64k and 128k from the end of the firmware. This means that the file 
system will survive lots of reflashing and at least 64k of firmware growth. 

The standard build process for the firmware builds the `spiffsimg` tool (found in the `tools/spiffsimg` subdirectory).
The top level Makfile also checks if
there is any data in the `local/fs` directory tree, and it will then copy these files
into the flash disk image. Two images will normally be created -- one for the 512k flash part and the other for the 4M flash part. If the data doesn't 
fit into the 512k part after the firmware is included, then the file will not be generated.
The disk image file is placed into the `bin` directory and it is named `0x<offset>-<size>.bin` where the offset is the location where it should be 
flashed, and the size is the size of the flash part. It is quite valid (and quicker) to flash the 512k image into a 4M part. However, there will probably be
limited space in the file system for creating new files.

The default configuration will try and build three different file systems for 512KB, 1MB and 4MB flash sizes. The 1MB size is suitable for the ESP8285. This can be overridden by specifying the FLASHSIZE parameter to the makefile.

If the `local/fs` directory is empty, then no flash images will be created (and the ones from the last build will be removed). The `spiffsimg` tool can 
then be used to build an image as required. 

If no file system is found during platform boot, then a new file system will be formatted. This can take some time on the first boot.

Note that the last 16k of the flash chip is reserved for the SDK to store parameters (such as the client wifi settings).

In order to speed up the boot time, it is possible to define (at build time) the size of the SPIFFS Filesystem to be formatted. This can be as small as 32768 bytes which gives a filesystem with about 15k bytes of usable space.
Just place the following define in `user_config.h` or some other file that is included during the build.

```
#define SPIFFS_MAX_FILESYSTEM_SIZE	32768
```

This filesystem size limit only affects the formatting of a file system -- if the firm finds an existing valid filesystem (of any size) it will use that. However, if the 
filesystem is reformatted from Lua (using file.format()) then the new file system will obey the size limit. 

There is also an option to control the positioning of the SPIFFS file system:

```
#define SPIFFS_FIXED_LOCATION   	0x100000
```

This specifies that the SPIFFS filesystem starts at 1Mb from the start of the flash. Unless otherwise specified, it will run to the end of the flash (excluding the 16k of space reserved by the SDK). 

There is an option that limits the size of the file system to run up to the next 1MB boundary (minus the 16k for the parameter space). This may be useful when dealing with OTA upgrades.

```
#define SPIFFS_SIZE_1M_BOUNDARY
```
