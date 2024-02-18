# Introduction

Welcome to the NodeMCU self-test suite.  Here you will find our growing effort
to ensure that our software behaves as we think it should and that we do not
regress against earlier versions.

Our tests are written using [NTest](./NTest/NTest.md), a lightweight yet
featureful framework for specifying unit tests.

# Building and Running Test Software on NodeMCU Devices

Naturally, to test NodeMCU on its intended hardware, you will need one or more
NodeMCU-capable boards.  At present, the test environment is specified using
two ESP8266 Devices Under Test (DUTs), but we envision expanding this to mixed
ESP8266/ESP32 environments as well.

Test programs live beside this file.  While many test programs run on the
NodeMCU DUTs, but there is reason to want to orchestrate DUTs and the
environment using the host.  Files matching the glob `NTest_*.lua` are intended
for on-DUT execution.

## Manual Test Invocation

At the moment, the testing regime and host-based orchestration is still in
development, and so things are a little more manual than perhaps desired.  The
`NTest`-based test programs all assume that they can `require "NTest"`, and so
the easiest route to success is to

* build an LFS image containing

  * [package.loader support for LFS](../lua_examples/lfs/_init.lua)

  * [NTest itself](./NTest/NTest.lua)

  * Any additional Lua support modules required (e.g., [mcp23017
  support](../lua_modules/mcp23017/mcp23017.lua) )

* build a firmware with the appropriate C modules

* program the board with your firmware and LFS images

* ensure that `package.loader` is patched appropriately on startup

* transfer the `NTest_foo` program you wish to run to the device SPIFFS
  (or have included it in the LFS).

* at the interpreter prompt, say `dofile("NTest_foo.lua")` (or
  `node.LFS.get("NTest_foo")()`) to run the `foo` test program.

## Experimental Host Orchestration

Enthusiastic testers are encouraged to try using our very new, very
experimental host test runner, [tap-driver.expect](./tap-driver.expect).  To
use this program, in addition to the above, the LFS environment should contain
[NTestTapOut](./tests/utils/NTestTapOut.lua), an output adapter for `NTest`,
making it speak a slight variant of the [Test Anything
Protocol](https://testanything.org/).  This structured output is scanned for
by the script on the host.

You'll need `expect` and TCL and some TCL libraries available; on Debian, that
amounts to

    apt install tcl tcllib tclx8.4 expect

This program should be invoked from beside this file with something like

    TCLLIBPATH=./expectnmcu ./tap-driver.expect -serial /dev/ttyUSB3 -lfs ./lfs.img NTest_file.lua

This will...

* transfer and install the specified LFS module (and reboot the device to load LFS)

* transfer the test program

* run the test program with `NTest` shimmed to use the `NTestTapOut` output
  handler

* summarize the results

* return 0 if and only if all tests have passed

This tool is quite flexible and takes a number of other options and flags
controlling aspects of its behavior:

* Additional files, Lua or otherwise, may be transferred by specifing them
  before the test to run (e.g., `./tap-driver.expect a.lua b.lua
  NTest_foo.lua`); dually, a `-noxfer` flag will suppress transferring even the
  last file.  All transferred files are moved byte-for-byte to the DUT's
  SPIFFS with names, but not directory components, preserved.

* The `-lfs LFS.img` option need not be specified and, if not given, any
  existing `LFS` image will remain on the device for use by the test.

* A `-nontestshim` flag will skip attempting to shim the given test program
  with `NTestTapOut`; the test program is expected to provide its own TAP
  output.  The `-tpfx` argument can be used to override the leading `TAP: `
  sigil used by the `NTestTapOut` output handler.

* A `-runfunc` option indicates that the last argument is not a file to
  transfer but rather a function to be run.  It will be invoked at the REPL
  with a single argument, the shimmed `NTest` constructor, unless `-nontestshim`
  is given, in which case the argument will be `nil`.

* A `-notests` option suppresses running tests (making the tool merely another
  option for loading files to the device).

Transfers will be significantly faster if
[pipeutils](../lua_examples/pipeutils.lua) is available to `require` on the
DUT, but a fallback strategy exists if not.  We suggest either including
`pipeutils` in LFS images, in SPIFFS, or as the first file to be transferred.

# NodeMCU Testing Environment

Herein we define the environment our testing framework expects to see
when it runs. It is composed of two ESP8266 devices, each capable of
holding an entire NodeMCU firmware, LFS image, and SPIFFS file system,
as well as additional peripheral hardware. It is designed to fit
comfortably on a breadboard and so should be easily replicated and
integrated into any firmware validation testing.

The test harness runs from a dedicated host computer, which is expected
to have reset- and programming-capable UART links to both ESP8266
devices, as found on almost all ESP8266 boards with USB to UART
adapters, but the host does not necessarily need to use USB to connect,
so long as TXD, RXD, DTR, and RTS are wired across.

A particular implementation of this can be found at [Test Harness](HardwareTestHarness.md).

## Peripherals

### I2C Bus

There is an I2C bus hanging off DUT 0. Attached hardware is used both as
tests of modules directly and also to facilitate testing other modules
(e.g., gpio).

#### MCP23017: I/O Expander

At address 0x20. An 16-bit tristate GPIO expander, this chip is used to
test I2C, GPIO, and ADC functionality. This chip's interconnections are
as follows:

MPC23017 | Purpose
---------|--------------------------------------------------------------
/RESET   |DUT0 reset. This resets the chip whenever the host computer resets DUT 0 over its serial link (using DTR/RTS).
B 0      |4K7 resistor to DUT 0 ADC.
B 1      |2K2 resistor to DUT 0 ADC.
B 5      |DUT1 GPIO16/WAKE via 4K7 resitor
B 6      |DUT0 GPIO13 via 4K7 resistor and DUT1 GPIO15 via 4K7 resistor
B 7      |DUT0 GPIO15 via 4K7 resistor and DUT1 GPIO13 via 4K7 resistor

Notes:

-   DUT 0's ADC pin is connected via a 2K2 reistor to this chip's port
    B, pin 1 and via a 4K7 resistor to port B, pin 0. This gives us the
    ability to produce approximately 0 (both pins low), 1.1 (pin 0 high,
    pin 1 low), 2.2 (pin 1 high, pin 0 low), and 3.3V (both pins high)
    on the ADC pin.
-   Port B pins 6 and 7 sit on the UART cross-wiring between DUT 0 and
    DUT 1. The 23017 will be tristated for inter-DUT UART tests, but
    these
-   Port B pins 2, 3, and 4, as well as all of port A, remain available
    for expansion.
-   The interrupt pins are not yet routed, but could be. We reserve DUT
    0 GPIO 2 for this purpose with the understanding that the 23017's
    interrupt functionality will be disabled (INTA, INTB set to
    open-drain, GPINTEN set to 0) when not explicitly under test.

ESP8266 Device 0 Connections
----------------------------

ESP       | Usage
----------|----------------------------------------------------------
GPIO 0    |Used to enter programming mode; otherwise unused in test environment.
GPIO 1    |Primary UART transmit; reserved for host communication
GPIO 2    |[reserved for 1-Wire] [+ reserved for 23017 INT[AB] connections]
GPIO 3    |Primary UART recieve; reserved for host communication
GPIO 4    |I2C SDA
GPIO 5    |I2C SCL
GPIO 6    |[Reserved for on-chip flash]
GPIO 7    |[Reserved for on-chip flash]
GPIO 8    |[Reserved for on-chip flash]
GPIO 9    |[Reserved for on-chip flash]
GPIO 10   |[Reserved for on-chip flash]
GPIO 11   |[Reserved for on-chip flash]
GPIO 12   |
GPIO 13   |Secondary UART RX; DUT 1 GPIO 15, I/O expander B 6
GPIO 14   |
GPIO 15   |Secondary UART TX; DUT 1 GPIO 13, I/O expander B 7
GPIO 16   |
ADC 0     |Resistor divider with I/O expander

ESP8266 Device 1 Connections
----------------------------

ESP       | Usage
----------|----------------------------------------------------------
GPIO 0    |Used to enter programming mode; otherwise unused in test environment.
GPIO 1    |Primary UART transmit; reserved for host communication
GPIO 2    |[Reserved for WS2812]
GPIO 3    |Primary UART recieve; reserved for host communication
GPIO 4    |
GPIO 5    |
GPIO 6    |[Reserved for on-chip flash]
GPIO 7    |[Reserved for on-chip flash]
GPIO 8    |[Reserved for on-chip flash]
GPIO 9    |[Reserved for on-chip flash]
GPIO 10   |[Reserved for on-chip flash]
GPIO 11   |[Reserved for on-chip flash]
GPIO 12   |HSPI MISO
GPIO 13   |Secondary UART RX; DUT 0 GPIO 15, I/O exp B 7 via 4K7 Also used as HSPI MOSI for SPI tests
GPIO 14   |HSPI CLK
GPIO 15   |Secondary UART TX; DUT 0 GPIO 13, I/O exp B 6 via 4K7 Also used as HSPI /CS for SPI tests
GPIO 16   |I/O expander B 5 via 4K7 resistor, for deep-sleep tests
ADC 0     |


