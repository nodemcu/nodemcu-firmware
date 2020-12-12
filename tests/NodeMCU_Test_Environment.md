NodeMCU Testing Environment
===========================

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

Peripherals
-----------

### I2C Bus

There is an I2C bus hanging off DUT 0. Attached hardware is used both as
tests of modules directly and also to facilitate testing other modules
(e.g., gpio).

#### MCP23017: I/O Expander

At address 0x20. An 16-bit tristate GPIO expander, this chip is used to
test I2C, GPIO, and ADC functionality. This chip's interconnections are
as follows:

  --------- --------------------------------------------------------------
  /RESET    DUT0 reset. This resets the chip whenever the host computer
            resets DUT 0 over its serial link (using DTR/RTS).

  B 0       4K7 resistor to DUT 0 ADC.

  B 1       2K2 resistor to DUT 0 ADC.

  B 5       DUT1 GPIO16/WAKE via 4K7 resitor

  B 6       DUT0 GPIO13 via 4K7 resistor and DUT1 GPIO15 via 4K7 resistor

  B 7       DUT0 GPIO15 via 4K7 resistor and DUT1 GPIO13 via 4K7 resistor
  --------- --------------------------------------------------------------

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

  ---------- ----------------------------------------------------------
  ESP        

  GPIO 0     Used to enter programming mode; otherwise unused in test
             environment.

  GPIO 1     Primary UART transmit; reserved for host communication

  GPIO 2     [reserved for 1-Wire] [+ reserved for 23017 INT[AB]
             connections]

  GPIO 3     Primary UART recieve; reserved for host communication

  GPIO 4     I2C SDA

  GPIO 5     I2C SCL

  GPIO 6     [Reserved for on-chip flash]

  GPIO 7     [Reserved for on-chip flash]

  GPIO 8     [Reserved for on-chip flash]

  GPIO 9     [Reserved for on-chip flash]

  GPIO 10    [Reserved for on-chip flash]

  GPIO 11    [Reserved for on-chip flash]

  GPIO 12    

  GPIO 13    Secondary UART RX; DUT 1 GPIO 15, I/O expander B 6

  GPIO 14    

  GPIO 15    Secondary UART TX; DUT 1 GPIO 13, I/O expander B 7

  GPIO 16    

  ADC 0      Resistor divider with I/O expander
  ---------- ----------------------------------------------------------

ESP8266 Device 1 Connections
----------------------------

  ---------- ----------------------------------------------------------
  ESP        

  GPIO 0     Used to enter programming mode; otherwise unused in test
             environment.

  GPIO 1     Primary UART transmit; reserved for host communication

  GPIO 2     [Reserved for WS2812]

  GPIO 3     Primary UART recieve; reserved for host communication

  GPIO 4     

  GPIO 5     

  GPIO 6     [Reserved for on-chip flash]

  GPIO 7     [Reserved for on-chip flash]

  GPIO 8     [Reserved for on-chip flash]

  GPIO 9     [Reserved for on-chip flash]

  GPIO 10    [Reserved for on-chip flash]

  GPIO 11    [Reserved for on-chip flash]

  GPIO 12    HSPI MISO

  GPIO 13    Secondary UART RX; DUT 0 GPIO 15, I/O exp B 7 via 4K7 Also
             used as HSPI MOSI for SPI tests

  GPIO 14    HSPI CLK

  GPIO 15    Secondary UART TX; DUT 0 GPIO 13, I/O exp B 6 via 4K7 Also
             used as HSPI /CS for SPI tests

  GPIO 16    I/O expander B 5 via 4K7 resistor, for deep-sleep tests

  ADC 0      
  ---------- ----------------------------------------------------------


