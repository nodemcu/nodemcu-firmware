There are essentially three ways to build your NodeMCU firmware: cloud build service, Docker image, dedicated Linux environment (possibly VM).

## Tools

### Cloud Build Service
NodeMCU "application developers" just need a ready-made firmware. There's a [cloud build service](https://nodemcu-build.com/) with a nice UI and configuration options for them.

### Docker Image
Occasional NodeMCU firmware hackers don't need full control over the complete tool chain. They might not want to setup a Linux VM with the build environment. Docker to the rescue. Give [Docker NodeMCU build](https://hub.docker.com/r/marcelstoer/nodemcu-build/) a try.

!!! caution

    Take note that you need to clone the repository including Git submodules just as described below for the Linux environment. 

### Linux Build Environment
NodeMCU firmware developers commit or contribute to the project on GitHub and might want to build their own full fledged build environment with the complete tool chain.

#### Build environment dependencies, tools and libraries:

#### Ubuntu:

```bash
sudo apt-get install -y gperf python-pip python-dev flex bison build-essential libssl-dev libffi-dev libncurses5-dev libncursesw5-dev libreadline-dev cmake
```

#### Setting up the repository

Run the following command for a new checkout from scratch. This will fetch the nodemcu repo, checkout the `dev-esp32` branch and finally pull all submodules:

```
git clone --branch dev-esp32 --recurse-submodules https://github.com/nodemcu/nodemcu-firmware.git nodemcu-firmware-esp32
```

To install the prerequisites for the ESP32 SDK and NodeMCU components, run:
```
./install.sh
```

The `make menuconfig` command initiates the build process, which will start with the configuration menu to set the build options.

!!! important

    GNU make version 4.0 or higher is required for a successful build. Versions 3.8.2 and below will produce an incomplete firmware image.

Updating your clone from upstream needs an additional command to update the submodules as well:

```
git pull origin dev-esp32
git submodule init #only if repo was cloned w/o submodules init
git submodule update --recursive
```

Here is a video walk through by John Lauer (ChiliPeppr) of building the firmware in Linux from scratch with a fresh install of Ubuntu 19 so you can see all of the dependencies needed to get your build completed and flashed to your ESP32 device.

[![Video walk through for Linux Build Environment](https://img.youtube.com/vi/x6CGECsioYg/0.jpg)](https://www.youtube.com/watch?v=x6CGECsioYg "Video walk through for Linux Build Environment")

## Build Options

All configuration options are accessed from the file `sdkconfig`. It's advisable to set it up with the interactive `make menuconfig` - on a fresh checkout you're prompted to run through it by default.

The most notable options are described in the following sections.

### Select Modules

Follow the menu path
```
Component config --->
  NodeMCU modules --->
```
Tick or untick modules as required.

### UART default bit rate

Follow the menu path
```
Component config --->
  Platform config --->
    UART console default bit rate --->
```

### CPU Frequency

Follow the menu path
```
Component config --->
  ESP32-specific --->
    CPU frequency --->
```

### Stack Size

If you experience random crashes then increase the stack size and feed back your observation on the project's issues list.

Follow the menu path
```
Component config --->
  ESP32-specific --->
    Main task stack size --->
```

### Flashing Options

Default settings for flashing the firmware with esptool.py are also configured with menuconfig:

```
Serial flasher config --->
  Default serial port
  Default baud rate
  Flash SPI mode --->
  Detect flash size when flashing bootloader --->
```

### Partition Table
IDF's default partition table `Single factory app, no OTA` does not provide enough room for a firmware including large modules like e.g. `http` or `sodium`. To enable full feature sets, NodeMCU uses a custom partition table from `components/platform/partitions.csv` which allocates ~1.5&nbsp;MB for the firmware image. During first boot, the firmware creates an additional partition for SPIFFS in the remaining flash space.

For 2MB flash modules an alternative partition table available as `components/platform/partitions-2MB.csv`. It restricts the SPIFFS partition to  ~448&nbsp;kB and can be used with menuconfig:

```
Partition Table --->
  Partition Table (Custom partition table CSV)
  (components/platform/partitions-2MB.csv) Custom partition CSV file
  (0x10000) Factory app partition offset
```

### Using external components

It is possible, and relatively easy, to include external components and modules in NodeMCU. It is not uncommon to have one or more custom modules one wishes to include in the firmware. To enable this NodeMCU leverages the standard IDF `EXTRA_COMPONENT_DIRS` functionality. As such, it is possible to not only add extra Lua C modules, but also other components such as libraries.

To include one (or more) additional IDF components, simply set the `EXTRA_COMPONENT_DIRS` environment variable to the space-separated list of directories of said components. E.g.

```
export EXTRA_COMPONENT_DIRS="/path/to/mymod /path/to/mylib"
make menuconfig
make
```

To get started, a template directory structure is provided in [extcomp-template/](../extcomp-template) which provides a skeleton for a simple Lua C module, including the build logic in `CMakeLists.txt`, the configuration option in `Kconfig` and the Lua C module code in `mymod.c`. A detailed discussion on the specifics is beyond this document, but the first two are described comprehensively in the [official IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html), and module development is covered in [Programming in NodeMCU](nodemcu-pil.md).

In fact, to quickly try it out it's even possible to include the template itself, as-is:


```
export EXTRA_COMPONENT_DIRS="$PWD/extcomp-template"
make menuconfig
make
```

after which the command `mymod.hello()` is available in the Lua environment.
