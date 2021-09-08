Below you'll find all necessary information to flash a NodeMCU firmware binary to ESP8266 or ESP8285. Note that this is a reference documentation and not a tutorial with fancy screen shots. Turn to the respective tool documentation for those.

!!! attention

    Keep in mind that the ESP8266 needs to be [put into flash mode](#putting-device-into-flash-mode) before you can flash a new firmware!

!!! important

    When switching between NodeMCU versions, see the notes about
    [Upgrading Firmware](#upgrading-firmware).

## Tool overview

### esptool.py
> A Python-based, open source, platform independent, utility to communicate with the ROM bootloader in Espressif ESP8266.

Source: [https://github.com/espressif/esptool](https://github.com/espressif/esptool)

Supported platforms: OS X, Linux, Windows, anything that runs Python

**Running esptool.py**

Run the following command to flash an *aggregated* binary as is produced for example by the [cloud build service](build.md#cloud-build-service) or the [Docker image](build.md#docker-image).

`esptool.py --port <serial-port-of-ESP8266> write_flash -fm <flash-mode> 0x00000 <nodemcu-firmware>.bin`

[`flash-mode`](https://github.com/espressif/esptool/#flash-modes) is `qio` for most ESP8266 ESP-01/07 (512&nbsp;kByte modules) and `dio` for most ESP32 and ESP8266 ESP-12 (>=4&nbsp;MByte modules). ESP8285 requires `dout`.

**Gotchas**

- See [below](#determine-flash-size) if you don't know or are uncertain about the capacity of the flash chip on your device. It might help to double check as e.g. some ESP-01 modules come with 512kB while others are equipped with 1MB.
- esptool.py is under heavy development. It's advised you run the latest version (check with `esptool.py version`). Since this documentation may not have been able to keep up refer to the [esptool flash modes documentation](https://github.com/themadinventor/esptool#flash-modes) for current options and parameters.
- The firmware image file contains default settings `dio` for flash mode and `40m` for flash frequency.
- In some uncommon cases, the [SDK init data](#sdk-init-data) may be invalid and NodeMCU may fail to boot. The easiest solution is to fully erase the chip before flashing:
`esptool.py --port <serial-port-of-ESP8266> erase_flash`

### NodeMCU PyFlasher
> Self-contained [NodeMCU](https://github.com/nodemcu/nodemcu-firmware) flasher with GUI based on [esptool.py](https://github.com/espressif/esptool) and [wxPython](https://www.wxpython.org/).

![NodeMCU PyFlasher](https://github.com/marcelstoer/nodemcu-pyflasher/raw/master/images/gui.png "NodeMCU PyFlasher")

Source: [https://github.com/marcelstoer/nodemcu-pyflasher](https://github.com/marcelstoer/nodemcu-pyflasher)

Supported platforms: anything that runs Python, runnable `.exe` available for Windows and `.dmg` for macOS

Disclaimer: the availability of [NodeMCU PyFlasher was announced on the NodeMCU Facebook page](https://www.facebook.com/NodeMCU/posts/663197460515251) but it is not an official offering of the current NodeMCU firmware team.

## Putting Device Into Flash Mode

To enable ESP8266 firmware flashing GPIO0 pin must be pulled low before the device is reset. Conversely, for a normal boot, GPIO0 must be pulled high or floating.

If you have a [NodeMCU dev kit](https://github.com/nodemcu/nodemcu-devkit-v1.0) then you don't need to do anything, as the USB connection can pull GPIO0 low by asserting DTR and reset your board by asserting RTS.

If you have an ESP-01 or other device without built-in USB, you will need to enable flashing yourself by pulling GPIO0 low or pressing a "flash" switch, while powering up or resetting the module.

## Which Files To Flash

If you build your firmware with the [cloud builder or the Docker image](build.md), or any other method that produces a *combined binary*, then you can flash that file directly to address 0x00000.

Otherwise, if you built your own firmware from source code:

- `bin/0x00000.bin` to 0x00000
- `bin/0x10000.bin` to 0x10000

## Upgrading Firmware

There are three potential issues that arise from upgrading (or downgrading!) firmware from one NodeMCU version to another:

* Lua scripts written for one NodeMCU version (like 0.9.x) may not work error-free on a more recent firmware.  For example, Espressif changed the `socket:send` operation to be asynchronous i.e. non-blocking. See [API documentation](modules/net.md#netsocketsend) for details.

* The NodeMCU flash file system may need to be reformatted, particularly if its address has changed because the new firmware is different in size from the old firmware.  If it is not automatically formatted then it should be valid and have the same contents as before the flash operation. You can still run [`file.format()`](modules/file.md#fileformat) manually to re-format your flash file system. You will know if you need to do this if your flash files exist but seem empty, or if data cannot be written to new files. However, this should be an exceptional case.
Formatting a file system on a large flash device (e.g. the 16MB parts) can take some time. So, on the first boot, you shouldn't get worried if nothing appears to happen for a minute. There's a message printed to console to make you aware of this.

* The Espressif SDK Init Data may change between each NodeMCU firmware version, and may need to be erased or reflashed.  See [SDK Init Data](#sdk-init-data) for details.  Fully erasing the module before upgrading firmware will avoid this issue.

## SDK Init Data

NodeMCU versions are compiled against specific versions of the Espressif SDK. The SDK reserves space in flash that is used to store calibration and other data. Espressif refers to this area as "System Param" and it occupies four 4&nbsp;Kb sectors of flash. A fifth 4&nbsp;Kb sector is also reserved for RF calibration.
-  With SDK version 2.x builds, these 5 sectors are located in the last pages at in the Flash memory.
-  With SDK version 3.x builds, these 5 sectors are located in the otherwise unused pages at Flash offset 0x0B000-0x0FFFF, between the `bin/0x00000.bin` segment at 0x00000 and the `bin/0x10000.bin` to 0x10000.

If this data gets corrupted or you are upgrading major SDK versions, then the firmware may not boot correctly. Symptoms include messages like `rf_cal[0] !=0x05,is 0xFF`, or endless reboot loops and/or fast blinking module LEDs.  If you are seeing one or several of the above symptoms, ensure that your chip is fully erased before flashing, for example by using `esptool.py`.  The SDK version 3.x firmware builds detect if the RF calibration sector has been erased or corrupted, and will automatically initialise it with the correct content before restarting the processor. This works for all SDK supported flash sizes.

## Determine flash size

The easiest way to determine the flash capacity is to load the firmware and then `print(node.info'hw'.flash_size)` which reports the flash size in Kb.  Alternatively, if you want to determine the capacity of the flash chip _before_ a firmware is installed then you can run the following command.  This will return a 2 hex digit **Manufacturer** ID and a 4 digit **Device** ID and the detected flash size.

`esptool.py --port <serial-port> flash_id`
The chip ID can then be looked up in <https://review.coreboot.org/plugins/gitiles/flashrom/+/refs/heads/master/flashchips.h>.
