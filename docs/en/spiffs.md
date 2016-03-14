# SPIFFS File System

The SPIFFS configuration is 4k sectors (the only size supported by the SDK) and 8k blocks. 256 byte pages. Magic is enabled and magic_len is also enabled. This allows the firmware to find the start of the filesystem (and also the size).
One of the goals is to make the filsystem more persistent across reflashing of the firmware.

There are two significant sizes of flash -- the 512K and 4M (or bigger). 

The file system has to start on a 4k boundary, but since it ends on a much bigger boundary (a 16k boundary), it also starts on an 8k boundary. For the small flash chip, there is 
not much spare space, so a newly formatted file system will start as low as possible (to get as much space as possible). For the large flash, the 
file system will start on a 64k boundary. A newly formatted file system will start between 64k and 128k from the end of the firmware. This means that the file 
system will survive lots of reflashing and at least 64k of firmware growth. 

The spiffsimg tool can also be built (from source) in the nodemcu-firmware build tree. If there is any data in the `local/fs` directory tree, then it will
be copied into the flash disk image. Two images will normally be created -- one for the 512k flash part and the other for the 4M flash part. If the data doesn't 
fit into the 512k part after the firmware is included, then the file will not be generated.

The disk image file is placed into the `bin` directory and it is named `0x<offset>-<size>.bin` where the offset is the location where it should be 
flashed, and the size is the size of the flash part. It is quite valid (and quicker) to flash the 512k image into a 4M part. However, there will probably be
limited space in the file system for creating new files.

If no file system is found during platform boot, then a new file system will be formatted. This can take some time on the first boot.

Note that the last 16k of the flash chip is reserved for the SDK to store parameters (such as the client wifi settings).
