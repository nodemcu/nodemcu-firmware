# spiffsimg - Manipulate SPI Flash File System disk images

Ever wished you could prepare a SPIFFS image offline and flash the whole
thing onto your microprocessor's storage instead of painstakingly upload
file-by-file through your app on the micro? With spiffsimg you can!

NodeMCU uses a SPIFFS filesystem that knows how big it is -- i.e. when you build a file system
image, it must fit into the flash chip, and it cannot be expanded once flashed.
It is important to give the `spiffimg` tool the correct size. You can provide either the `-c` option or both the `-U` and `-S` options.

### Syntax 

`spiffsimg -f <filename> 
	[-o <offsetfile>]
	[-c <size>] 
	[-S <flashsize>]
	[-U <usedsize>]
	[-d]
	[-l | -i | -r <scriptname> ]`

### Supported operations:

  * -f specifies the filename for the disk image. '%x' will be replaced by the calculated offset of the file system.
  * -o specifies the file which is to contain the calculated offset.
  * -S specifies the size of the flash chip. '32m' is 32 mbits, '1MB' is 1 megabyte.
  * -U specifies the amount of flash used by the firmware. Decimal or Hex bytes.
  * Create (-c size) a blank disk image of the given size.
  * List (-l) the contents of the given disk image.
  * Interactive (-i) or scripted (-r) commands.
  * -d causes the disk image to be deleted on error. This makes it easier to script.

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


### See also:
  * The SPIFFS project: https://github.com/pellepl/spiffs
