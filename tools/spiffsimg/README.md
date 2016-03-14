# spiffsimg - Manipulate SPI Flash File System disk images

Ever wished you could prepare a SPIFFS image offline and flash the whole
thing onto your microprocessor's storage instead of painstakingly upload
file-by-file through your app on the micro? With spiffsimg you can!

`Syntax: spiffsimg -f <filename> [-c size] [-l | -i | -r <scriptname> ]`

### Supported operations:

  * Create (-c size) a blank disk image of the given size.
  * List (-l) the contents of the given disk image.
  * Interactive (-i) or scripted (-r) commands.

### Available commands:

  * `ls` List contents. Output format is {type} {size} {name}.
  * `cat <filename>` Dump file contents to stdout.
  * `rm <filename>` Delete file.
  * `info` Display SPIFFS usage estimates.
  * `import <srcfile> <spiffsname>` Import a file into the disk image.
  * `export <spiffsname> <dstfile>` Export a file from the disk image.

### Example:
```lua
# spiffsimg -f flash.img -c 524288 -i
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
