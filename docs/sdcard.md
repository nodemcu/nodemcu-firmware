# FAT File System on SD Card

Accessing files on external SD cards is currently only supported from the `file` module. This imposes the same overall restrictions of internal SPIFFS to SD cards:

- limited support for sub-folders
- no timestamps
- no file attributes (read-only, system, etc.)

Work is in progress to extend the `file` API with support for the missing features.

## Enabling FatFs

The FAT file system is implemented by [Chan's FatFs](http://elm-chan.org/fsw/ff/00index_e.html) version [R0.13c](http://elm-chan.org/fsw/ff/ff13c.zip). It's disabled by default to save memory space and has to be enabled before compiling the firmware:

Enable "Support for FAT filesystems" in Comoponent config ---> Platform config and enable the sdmmc module for low-level control.

## SD Card connection

Refer to the [`sdmmc` module documentation](modules/sdmmc.md).

## Lua bindings

Before mounting the volume(s) on the SD card, you need to initialize the SDMMC interface from Lua.

```lua
-- for SDMMC mode:
card = sdmmc.init(sdmmc.HS2, {width = sdmmc.W1BIT})

-- for SD SPI mode:
card = sdmmc.init(sdmmc.VSPI, {sck_pin = 18, mosi_pin = 23, miso_pin = 19, cs_pin = 22})

-- then mount the sd
-- note: the card initialization process during `card:mount()` will set spi divider temporarily to 200 (400 kHz)
-- it's reverted back to the current user setting before `card:mount()` finishes
card:mount("/SD0")
file.open("/SD0/path/to/somefile")
print(file.read())
file.close()
```

The logical drives are mounted at the root of a unified directory tree where the mount points distinguish between internal flash (`/FLASH`) and the card's paritions (`/SD0` to `/SD3`). Files are accessed via either the absolute hierarchical path or relative to the current working directory. It defaults to `/FLASH` and can be changed with `file.chdir(path)`.

Subdirectories are supported on FAT volumes only.

## Multiple partitions / multiple cards

The mapping from logical volumes (eg. `/SD0`) to partitions on an SD card is defined in [`fatfs_config.h`](../components/fatfs/fatfs_config.h). More volumes can be added to the `VolToPart` array with any combination of physical drive number (aka SDMMC slots) and partition number. Their names have to be added to `_VOLUME_STRS` in [`ffconf.h`](../components/fatfs/ffconf.h) as well.
