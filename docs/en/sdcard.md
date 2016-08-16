# FAT File System on SD Card

Accessing files on external SD cards is currently only supported from the `file` module. This imposes the same overall restrictions of internal SPIFFS to SD cards:
- only one file can be opened at a time
- no support for sub-folders
- no timestamps
- no file attributes (read-only, system, etc.)

Work is in progress to extend the `file` API with support for the missing features.

## Enabling FatFs

The FAT file system is implemented by [Chan's FatFs](http://elm-chan.org/fsw/ff/00index_e.html) version [R0.12a](http://elm-chan.org/fsw/ff/ff12a.zip). It's disabled by default to save memory space and has to be enabled before compiling the firmware:

Uncomment `#define BUILD_FATFS` in [`user_config.h`](../../app/include/user_config.h).

## SD Card connection

The SD card is operated in SPI mode, thus the card has to be wired to the respective ESP pins of the HSPI interface:
- `CLK` - pin5 / GPIO14
- `MISO` - pin 6 / GPIO12
- `MOSI` - pin 7 / GPIO13
- `CS` - recommended pin 8 / GPIO15

Connection of `CS` can be done to any of the GPIOs on pins 1 to 12. This allows coexistence of the SD card with other SPI slaves on the same bus.

## Lua bindings

Before mounting the volume(s) on the SD card, you need to initialize the SPI interface from Lua.

```lua
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)

-- initialize other slaves

vol = file.mount("/SD0", 8)   -- incl. 2nd optional parameter for SS/CS pin
if not vol then
  print("retry mounting")
  vol = file.mount("/SD0", 8)
  if not vol then
    error("mount failed")
  end
end
file.open("/SD0/somefile")
print(file.read())
file.close()
```

DOS-like identifiers are used to distinguish between internal flash (`/FLASH`) and the card's paritions (`/SD0` to `/SD3`). This identifier can be omitted for backwards-compatibility and access will be directed to internal flash in this case. Use `file.chdir("/SD0")` to switch the current drive to SD card.

!!! note "Note:"

    If the card doesn't work when calling `file.mount()` for the first time then re-try the command. It's possible that certain cards time out during the first initialization after power-up.

## Multiple partitions / multiple cards

The mapping from logical volumes (eg. `/SD0`) to partitions on an SD card is define in [`fatfs_config.h`](../../app/include/fatfs_config.h). More volumes can be added to the `VolToPart` array with any combination of physical drive number (aka SS/CS pin) and partition number. Their names have to be added to `_VOLUME_STRS` in [`ffconf.h`](../../app/fatfs/ffconf.h) as well.
