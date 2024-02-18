Hardware Test Harness
=====================

There is an implementation of the hardware testing design which is a small 4in x 4in board with positions for
two Wemos D! Mini ESP8266 boards, a breadboard area and a number of positions for peripherals. A rendering of the board is below and the [schematics](Test-harness-schematic-v1.pdf)
are available as well: ![Board Render](Test-Harness-Render-V1.png). 


The test harness runs from a dedicated host computer, which is expected
to have reset- and programming-capable UART links to both ESP8266
devices, as found on almost all ESP8266 boards with USB to UART
adapters, but the host does not necessarily need to use USB to connect,
so long as TXD, RXD, DTR, and RTS are wired across.

The alternate pins on the primary D1 Mini (DUT0) are cross wired to the
RX and TX pins on the secondary D1 Mini (DUT1) and these are enabled by
a pin on the MCP23017.

Build Notes
-----------

The only thing that needs to be done is to solder on 0.1" headers at the required
positions. Typically D1 Minis come with 2 sets of 8 pin headers, both male and female.
I solder the female headers to the board, and the maie headers to the D1 minis. Other,
mostly 4 pin, headers can be soldered at the other positions. The 7 pin header for
the color sensor (TCS34725) requires some care as the board needs to be mounted
upside down so that the sensor is directly above the WS2812. 

The screw holes at the corners are for M3 screws. A standard adhesive rubber foot can
also be used. There are no components on the underside of the test board, so not much clearance
is required (only the length of the various headers soldered on the board).

Power
-----

The board is powered by either (or both) D1 Mini USB connection. Given the cross connects
between the two D1 Minis, I think that all the tests can be conducted from DUT0, but
it is probably easier to connected both of the D1 Minis via USB to the test runner.

There is a small resistor between the two 5 volt rails to prevent large currents
if the two USB feeds are at slughtly different voltages. The 3.3 volt rails are
directly connected together. If the regulators produce slightly different voltages,
then the one producing the higher voltage will end up providing all the power for the
3.3 volt devices.

Peripherals
-----------

### I2C Bus

There is an I2C bus hanging off DUT 0. Attached hardware is used both as
tests of modules directly and also to facilitate testing other modules
(e.g., gpio).

Most of the positions on the board are connected to the DUT1 I2C bus.

#### MCP23017: I/O Expander

At address 0x20. An 16-bit tristate GPIO expander, this chip is used to
test I2C, GPIO, and ADC functionality. This chip's interconnections are
as follows:

MPC23017 | Purpose
---------|--------------------------------------------------------------
/RESET   |DUT0 reset. This resets the chip whenever the host computer resets DUT 0 over its serial link (using DTR/RTS).
B 0      |4K7 resistor to DUT 0 ADC.
B 1      |2K2 resistor to DUT 0 ADC.
B 2      |Direct to DUT1 RST
B 3      |Direct to DUT1 D3
B 4      |When low, connects the alternate UART pins on DUT0 to RX,TX on DUT1
B 5      |DUT1 GPIO16/WAKE via 4K7 resitor
B 6      |DUT0 GPIO13 via 4K4 resistor and DUT1 GPIO15 via 4K7 resistor (also feeds in the primary TX from DUT1 when enabled by B4)
B 7      |DUT0 GPIO15 via 4K7 resistor and DUT1 GPIO13 via 4K7 resistor (also feeds the primary RX on DUT1 when enabled by B4)

Notes:

-   DUT 0's ADC pin is connected via a 2K2 reistor to this chip's port
    B, pin 1 and via a 4K7 resistor to port B, pin 0. This gives us the
    ability to produce approximately 0 (both pins low), 1.1 (pin 0 high,
    pin 1 low), 2.2 (pin 1 high, pin 0 low), and 3.3V (both pins high)
    on the ADC pin.
-   Port B pins 6 and 7 sit on the UART cross-wiring between DUT 0 and
    DUT 1. The 23017 will be tristated for inter-DUT UART tests, but
    these
-   All of port A, remain available for expansion and are routed to the breadboard area.

#### WS2812s

There are three WS2812s connected on DUT1/D4. The last Ws2812 is positioned so that a TCS34725 module
can be mounted upside down over it to read out the color of the WS2812. That device is connected to
the I2C port on DUT0. A suitable board is [CJMCU-34725 TCS34725 Color Sensor RGB color sensor development board module](https://www.aliexpress.com/item/32412698433.html). The illuminating
LED is connected to the INT pin and so you can disable the LED under software control.

#### Oled Displays

Each of the D1 Minis is connected to a position for a 128x64 OLED display, again on the primary I2C bus.    

#### Servo

On DUT1 pin D4/GPIO 2 there is a connection to a position for a small servo. The servo is powered by the
5V voltage rail. 

#### DHTxx

On DUT1 pin D6/GPIO 12 there is a connection to a position for a DHTxx device. The silk screen indicates the
orientation of the device.

#### DS18B20

There are two positions for DS18B20s -- one with the VCC pin connected and one without. The data pin is
connected to DUT1 pin D5/GPIO 14.

#### I2C devices with VCC/GND/SCL/SDA pin order

There are three positions for I2C devices which have the pins in the VCC/GND/SCL/SDA order. These
are on the DUT1 I2 bus.

#### I2C devices with other pin orders

There are three positions for I2C devices with other pin orders. Each of these positions is next
to a crossbar switch and so four blobs of solder can configure each of these headers into any
desired pin order. As far as I can tell, most of the cheap modules use the VCC/GND/SCL/SDA order.


Breadboard Area
===============

All the pins on each D1 Mini and the A port of the MCP23017 are brought out to a breadboard 
area. This can be used to solder components and/or wires, or even a header could be soldered
on to transfer all the signals to a conventional breadboard. 


ESP8266 Device 0 Connections
----------------------------

ESP       | Usage
----------|----------------------------------------------------------
D3/GPIO 0    |Used to enter programming mode; otherwise unused in test environment.
TX/GPIO 1    |Primary UART transmit; reserved for host communication
D4/GPIO 2    |[reserved for 1-Wire] [+ reserved for 23017 INT[AB] connections]
RX/GPIO 3    |Primary UART recieve; reserved for host communication
D2/GPIO 4    |I2C SDA. Connected to MCP23017, Oled display and the TCS34725 if present. 
D1/GPIO 5    |I2C SCL
GPIO 6    |[Reserved for on-chip flash]
GPIO 7    |[Reserved for on-chip flash]
GPIO 8    |[Reserved for on-chip flash]
GPIO 9    |[Reserved for on-chip flash]
GPIO 10   |[Reserved for on-chip flash]
GPIO 11   |[Reserved for on-chip flash]
D6/GPIO 12   |
D7/GPIO 13   |Secondary UART RX; DUT 1 GPIO 15, I/O expander B 6
D5/GPIO 14   |
D8/GPIO 15   |Secondary UART TX; DUT 1 GPIO 13, I/O expander B 7
D0/GPIO 16   |
A0/ADC 0     |Resistor divider with I/O expander

ESP8266 Device 1 Connections
----------------------------

ESP       | Usage
----------|----------------------------------------------------------
D3/GPIO 0    |Used to enter programming mode; otherwise unused in test environment.
TX/GPIO 1    |Primary UART transmit; reserved for host communication
D4/GPIO 2    |Connected to chain of 3 WS2812s. Also connected to the servo position.
RX/GPIO 3    |Primary UART recieve; reserved for host communication
D2/GPIO 4    |I2C SDA. Connected to all the other I2C positions on the board
D1/GPIO 5    |I2C SCL
GPIO 6    |[Reserved for on-chip flash]
GPIO 7    |[Reserved for on-chip flash]
GPIO 8    |[Reserved for on-chip flash]
GPIO 9    |[Reserved for on-chip flash]
GPIO 10   |[Reserved for on-chip flash]
GPIO 11   |[Reserved for on-chip flash]
D6/GPIO 12   |Connected to data pin for DHTxx
D7/GPIO 13   |Secondary UART RX; DUT 0 GPIO 15, I/O exp B 7 via 4K7 Also used as HSPI MOSI for SPI tests
D5/GPIO 14   |Connected to data pin for DS18B20s.
D8/GPIO 15   |Secondary UART TX; DUT 0 GPIO 13, I/O exp B 6 via 4K7 Also used as HSPI /CS for SPI tests
D0/GPIO 16   |I/O expander B 5 via 4K7 resistor, for deep-sleep tests
A0/ADC 0     |


