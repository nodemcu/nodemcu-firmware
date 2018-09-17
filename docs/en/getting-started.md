# Getting Started aka NodeMCU Quick Start

The basic process to get started with NodeMCU consists of the following three steps.

1.  [Build the firmware](build.md) with the modules you need
1.  [Flash the firmware](flash.md) to the chip
1.  [Upload code](upload.md) to the device.

You will typically do steps 1 and 2 only once, but then repeat step 3 as you develop your application. If your application outgrows the limited on-chip RAM then you can use the [Lua Flash Store](lfs.md) (LFS) to move your Lua code into flash memory, freeing a lot more RAM for variable data. This is why it is a good idea to enable LFS for step 1 if you are developing a larger application. As documented below there is a different approach to uploading Lua code. 
 

!!! caution
	There's more than one way to skin a cat. For each of the tasks you have a number of choices with regards to tooling. The colored boxes represent an opinionated path to start your journey - the quickest way to success so to speak. Feel free to follow the links above to get more detailed information.

### Task / OS selector

<table id="gs">
  <tr>
    <!--th >Task ↓ \ OS →</th-->
    <th>Task \ OS</th>
    <th>Windows<div class="icon windowsIcon"></span></th>
    <th>macOS<div class="icon appleIcon"></th>
    <th>Linux<div class="icon linuxIcon"></th>
  </tr>
  <tr>
    <th rowspan="3">Build firmware</th>
    <td class="select"><a href="#cloud-builder">cloud builder</a></td>
    <td class="select"><a href="#cloud-builder">cloud builder</a></td>
    <td class="select"><a href="#cloud-builder">cloud builder</a></td>
  </tr>
  <tr>
    <td><a href="#docker">Docker</a></td>
    <td><a href="#docker">Docker</a></td>
    <td><a href="#docker">Docker</a></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td>native</td>
  </tr>
  <tr>
    <th rowspan="2">Flash firmware</th>
    <td class="select"><a href="#nodemcu-pyflasher">NodeMCU PyFlasher</a></td>
    <td class="select"><a href="#nodemcu-pyflasher">NodeMCU PyFlasher</a></td>
    <td></td>
  </tr>
  <tr>
    <td><a href="#esptoolpy">esptool.py</a></td>
    <td><a href="#esptoolpy">esptool.py</a></td>
    <td class="select"><a href="#esptoolpy">esptool.py</a></td>
  </tr>  
  <tr>
    <th rowspan="2">Upload code</th>
    <td class="select"><a href="#esplorer">ESPlorer (Java)</a></td>
    <td><a href="#esplorer">ESPlorer (Java)</a></td>
    <td><a href="#esplorer">ESPlorer (Java)</a></td>
  </tr>
  <tr>
    <td><a href="#nodemcu-tool">NodeMCU-Tool (Node.js)</a></td>
    <td class="select"><a href="#nodemcu-tool">NodeMCU-Tool (Node.js)</a></td>
    <td class="select"><a href="#nodemcu-tool">NodeMCU-Tool (Node.js)</a></td>
  </tr> 
  <t>
    <th colspan="4">LFS tasks below</th>
  </tr>
  <tr>
    <th rowspan="3">Build LFS<br>enabled firmware</th>
    <td class="select"><a href="#for-lfs">cloud builder</a></td>
    <td class="select"><a href="#for-lfs">cloud builder</a></td>
    <td class="select"><a href="#for-lfs">cloud builder</a></td>
  </tr>
  <tr>
    <td><a href="#for-lfs_1">Docker</a></td>
    <td><a href="#for-lfs_1">Docker</a></td>
    <td><a href="#for-lfs_1">Docker</a></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td>native</td>
  </tr>
  <tr>
    <th rowspan="2">Build luac.cross</th>
    <td>not needed if you use Terry's webservice or Docker to later compile LFS image</td>
    <td>not needed if you use Terry's webservice or Docker to later compile LFS image</td>
    <td>not needed if you use Terry's webservice or Docker to later compile LFS image</td>
  </tr>
  <tr>
    <td><a href="#windows">native</a></td>
    <td><a href="#macos">native</a></td>
    <td><a href="#linux">native</a></td>
  </tr>
  <tr>
    <th rowspan="3">Compile Lua into<br>LFS image</th>
    <td class="select"><a href="#terrys-lfs-lua-cross-compile-web-service">webservice</a></td>
    <td><a href="#terrys-lfs-lua-cross-compile-web-service">webservice</a></td>
    <td><a href="#terrys-lfs-lua-cross-compile-web-service">webservice</a></td>
  </tr>
  <tr>
    <td><a href="#docker_1">Docker</a></td>
    <td><a href="#docker_1">Docker</a></td>
    <td><a href="#docker_1">Docker</a></td>
  </tr>
  <tr>
    <td><a href="#native-on-os">native</a></td>
    <td><a href="#native-on-os">native</a></td>
    <td><a href="#native-on-os">native</a></td>
  </tr>
  <tr>
    <th>Upload LFS image</th>
    <td class="select"><a href="#upload-lfs-image">generic</a></td>
    <td class="select"><a href="#upload-lfs-image">generic</a></td>
    <td class="select"><a href="#upload-lfs-image">generic</a></td>
  </tr>  
</table>

**How to read this**

Use case: you're just starting with NodeMCU and your OS of choice is Windows (and you are not using LFS), then the blue boxes in the 'Windows' column are your guideline. You:

- build the firmware on the cloud builder
- download and run the NodeMCU PyFlasher to transfer the firmware to the device
- download and run ESPlorer, which requires Java, to transfer Lua files from your system to the device

**Missing tools?**

Our intention is to introduce you to programming in Lua on the ESP8266 as quickly as possible, so we have kept the number of tools mentioned here to a minimum; [frightanic.com: Tools and IDEs](https://frightanic.com/iot/tools-ides-nodemcu/) discusses other tools and options.

!!! caution
	The below chapters are not meant to be followed one-by-one. Pick a task from the matrix above and it will take you to the relevant chapter.

## Cloud Builder

The cloud builder at [https://nodemcu-build.com](https://nodemcu-build.com) allows to pick NodeMCU branch, modules and a few other configuration options (e.g. SSL yes/no). After the build is completed you will receive an email with two links to download your custom firmware: 

- one for NodeMCU with floating support
- one for NodeMCU *without* floating support i.e. an integer-only binary

We recommend using the floating point build, even though the integer variant uses less RAM for storing variables, as there is little runtime difference between the two variants. Furthermore, the floating point variant handles non-integer values properly and this greatly simplifies numeric calculations.

For everything else the cloud builder GUI is self-explanatory. Hence, no need for further explanations here.

### For LFS

1. Expand the "LFS options" panel
1. Select an LFS size, 64KB is likely going to be large enough
1. Select other options and build

[↑ back to matrix](#task-os-selector)

_Note that this service is not maintained by the NodeMCU team. It's run by a NodeMCU team member as an individual, though._

## NodeMCU PyFlasher

[Self-contained NodeMCU flasher](https://github.com/marcelstoer/nodemcu-pyflasher) with GUI based on Python, esptool.py (see below) and wxPython. A runnable .exe is available for Windows and a .dmg for macOS.

**No installation required on Windows and macOS!** Instructions how to run it on other platforms are available on the project site.

![](https://github.com/marcelstoer/nodemcu-pyflasher/raw/master/images/gui.png)

1. [Install drivers for USB-to-serial](https://docs.thingpulse.com/how-tos/install-drivers/). Which driver you need depends on the ESP8266 module or USB-to-serial converter you use.
1. Connect USB cable to device and computer.
1. [Download](https://github.com/marcelstoer/nodemcu-pyflasher) then start PyFlasher
1. Select serial port, browse for firmware binary and set the flash options.

[↑ back to matrix](#task-os-selector)

_Note that this tool is not an official NodeMCU offering. It's maintained by a NodeMCU team member as an individual, though._

## esptool.py

[esptool.py](https://github.com/espressif/esptool) was started as a ESP8266 community effort but has since been adopted by Espressif. It's their officially recommended way to flash firmware to ESPxxx chips.

1. [Install drivers for USB-to-serial](https://docs.thingpulse.com/how-tos/install-drivers/). Which driver you need depends on the ESP8266 module or USB-to-serial converter you use.
1. Install [either Python 2.7 or Python >=3.4](https://www.python.org/downloads/) on your system if it's not available yet.
1. Connect USB cable to device and computer.
1. `$ pip install esptool` (also installs pySerial)
1. `$ esptool.py --port <serial-port-of-ESP8266> --baud <baud-rate> write_flash -fm <flash-mode> 0x00000 <nodemcu-firmware>.bin`

[`flash-mode`](https://github.com/espressif/esptool/#flash-modes) is `qio` for most ESP8266 ESP-01/07 (512&nbsp;kByte modules) and `dio` for most ESP32 and ESP8266 ESP-12 (>=4&nbsp;MByte modules). ESP8285 requires `dout`.

The [default baud rate](https://github.com/espressif/esptool#baud-rate) is 115200. Most hardware configurations should work with 230400 dependent on OS, driver, and module. NodeMCU and WeMos modules are usually ok with 921600.

More details available on esptool.py GitHub repo.

[↑ back to matrix](#task-os-selector)

## ESPlorer

TBD [https://github.com/4refr0nt/ESPlorer](https://github.com/4refr0nt/ESPlorer)

[↑ back to matrix](#task-os-selector)

## NodeMCU-Tool

Arguably [NodeMCU-Tool](https://github.com/andidittrich/NodeMCU-Tool), which requires Node.js, is the better code upload & execution tool than ESPlorer. Also, in contrast to the former it is very well maintained. However, we also understand that Windows users in general prefer GUI over command line.

The [list of features](https://github.com/andidittrich/NodeMCU-Tool#tool-summary) is quite long but essentially NodeMCU-Tool offers:

- upload (Lua) files from your host system to the device
- manage the device file system (delete, up-/download, etc.)
- run files on NodeMCU and display the output over UART/serial

Quick start:

1. [Install Node.js and NPM](https://nodejs.org/en/download/) if not available yet
1. Install NodeMCU-Tool globally `$ npm install nodemcu-tool -g`
1. Verify installation by runnin `$ nodemcu-tool --version`
1. Upload a Lua file `$ nodemcu-tool upload --port=/dev/ttyUSB0 helloworld.lua`
1. Run it `$ nodemcu-tool run helloworld.lua`

Note that you may need to use the `sudo` prefix to install the tool at step 2, and also possibly add the `–unsafe-perm` flag after the install command.

[↑ back to matrix](#task-os-selector)

## Docker

The [Docker NodeMCU build image](https://github.com/marcelstoer/docker-nodemcu-build) is the easiest method to build NodeMCU related components locally on your preferred platform.

Offering:

- build NodeMCU firmware based on locally cloned sources and configuration
- cross-compile Lua files into LFS image locally

Detailed instructions available in the image's README. As for available config options [check the documentation](build.md#build-options) and study the comments in `app/include/user_config.h`.

### For LFS

1. In `app/include/user_config.h` uncomment `#define LUA_FLASH_STORE 0x10000` and adjust the size if necessary.
2. Build as you would otherwise build with this image (i.e. see its README)

[↑ back to matrix](#task-os-selector)

_Note that this Docker image is not an official NodeMCU offering. It's maintained by a NodeMCU team member as an individual, though._

## Build `luac.cross`

A local copy of `luac.cross` is only needed if you want to compile the Lua files into an LFS image yourself and you are _not_ using Docker.

### Windows
Windows 10 users can install and use the Windows Subsystem for Linux (WSL). Alternatively all Windows users can [install Cygwin](https://www.cygwin.com/install.html) (only Cygwin core + **gcc-core** + **gnu make**). Either way, you will need a copy of the `luac.cross` compiler:

-  You can either download this from Terry's fileserver.  The [ELF variant](http://files.ellisons.org.uk/esp8266/luac.cross) is used for all recent Linux and WSL flavours, or the [cygwin binary](http://files.ellisons.org.uk/esp8266/luac.cross.cygwin)) for the Cygwin environment.
-  Or you can compile it yourself by downloading the current NodeMCU sources (this [ZIPfile](https://github.com/nodemcu/nodemcu-firmware/archive/master.zip)); edit the `app/includes/user_config.h` file and then `cd` to the `app/lua/luac_cross` and run make to build the compiler in the NodeMCU firmware root directory.  Note that the `luac.cross` make only needs the host toolchain which is installed by default.

### macOS

TBD

1. `$ cd app/lua/luac_cross`
2. `$ make` 

### Linux

TBD

1. `$ cd app/lua/luac_cross`
2. `$ make`

[↑ back to matrix](#task-os-selector)

## Compile Lua into LFS image

### Select Lua files to be run from LFS

The easiest approach is to maintain all the Lua files for your project in a single directory on your host. (These files will be compiled by `luac.cross` to build the LFS image in next step.)
    
For example to run the Telnet and FTP servers from LFS, put the following files in your project directory:
	
* [lua_examples/lfs/_init.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/_init.lua).  LFS helper routines and functions.
* [lua_examples/lfs/dummy_strings.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/dummy_strings.lua).  Moving common strings into LFS.
* [lua_examples/telnet/telnet.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/telnet/telnet.lua).  A simple **telnet** server.
* [lua_modules/ftp/ftpserver.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_modules/ftp/ftpserver.lua).  A simple **FTP** server.

You should always include the first two modules, but the remaining files would normally be replaced by your own project files.  Also remember that these are examples and that you are entirely free to modify or to replace them for your own application needs.

### Terry's LFS Lua Cross-Compile Web Service

[https://blog.ellisons.org.uk/article/nodemcu/a-lua-cross-compile-web-service/](https://blog.ellisons.org.uk/article/nodemcu/a-lua-cross-compile-web-service/)

Note: read up on [selecting Lua files](#select-lua-files-to-be-run-from-lfs) first

Upload a ZIP file with all your Lua files ready for LFS. The webservice will cross-compile them into a `.img` ready to be uploaded to the device. It supports LFS images for both floating point and integer firmware variants.

Further details available on the service site.

_Note that this service is not maintained by the NodeMCU team. It's run by a NodeMCU team member as an individual, though._

### Docker

Note: read up on [selecting Lua files](#select-lua-files-to-be-run-from-lfs) first

The same Docker image you used to build the NodeMCU firmware can be used to [compile Lua files into an LFS image](https://github.com/marcelstoer/docker-nodemcu-build#run-this-image-with-docker-to-create-an-lfs-image).

1. `$ cd <your-nodemcu-firmware-folder>`
1. `$ docker run --rm -ti -v `pwd`:/opt/nodemcu-firmware -v {PathToLuaSourceFolder}:/opt/lua marcelstoer/nodemcu-build lfs-image`

### Native on OS

Note: read up on [selecting Lua files](#select-lua-files-to-be-run-from-lfs) first

For Windows you will do this from within the WSL / Cygwin command window, both of which use the `bash` shell.

1. `$ cd <project-dir>`
1. `$ luac.cross -o lfs.img -f *.lua`

You will need to adjust the `img` and `lua` paths according to their location, and ensure that `luac.cross` is in your `$PATH` search list.  For example if you are using WSL and your project files are in `D:\myproject` then the Lua path would be `/mnt/d/myproject/*.lua` (For cygwin replace `mnt` by `cygwin`).  This will create the `lfs.img` file if there are no Lua compile errors (again specify an explicit directory path if needed).

You might also want to add a simple one-line script file to your `~/bin` directory to wrap this command up.

[↑ back to matrix](#task-os-selector)

## Upload LFS image

The compiled LFS image file (e.g. `lfs.img`) is uploaded as a regular file to the device file system (SPIFFS). You do this just like with Lua files with e.g. [ESPlorer](#esplorer) or [NodeMCU-Tool](#nodemcu-tool). There is also a new example, [HTTP_OTA.lua](https://github.com/nodemcu/nodemcu-firmware/tree/dev/lua_examples/lfs/HTTP_OTA.lua), in `lua_examples` that can retrieve images from a standard web service.
  
Once the LFS image file is on SPIFFS, you can execute the [node.flashreload()](../modules/node/#nodeflashreload) command and the loader will then load it into flash and immediately restart the ESP module with the new LFS loaded, if the image file is valid. However, the call will return with an error _if_ the file is found to be invalid, so your reflash code should include logic to handle such an error return.

### Edit your `init.lua` file

`init.lua` is the file that is first executed by the NodeMCU firmware. Usually it setups the WiFi connection and executes the main Lua application.  Assuming that you have included the `_init` file discussed above, then executing this will add a simple API for LFS module access:

-  Individual functions can be executed directly, e.g. `LFS.myfunc(a,b)`
-  LFS is now in the require path, so `require 'myModule'` works as expected.

Do a protected call of this `_init` code: `pcall(node.flashindex("_init"))` and check the error status.  See [Programming Techniques and Approachs](lfs.md#programming-techniques-and-approachs) in the LFS whitepaper for a more detailed description. 

[↑ back to matrix](#task-os-selector)