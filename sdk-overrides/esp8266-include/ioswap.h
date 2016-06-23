#ifndef _SDK_OVERRIDES_IOSWAP_H_
#define _SDK_OVERRIDES_IOSWAP_H_

#define ESP8266_DREG(addr) *((volatile uint32_t *)(0x3FF00000+(addr)))
#define IOSWAP    ESP8266_DREG(0x28)
#define IOSWAPU   0 //Swaps UART
#define IOSWAPS   1 //Swaps SPI
#define IOSWAPU0  2 //Swaps UART 0 pins (u0rxd <-> u0cts), (u0txd <-> u0rts)
#define IOSWAPU1  3 //Swaps UART 1 pins (u1rxd <-> u1cts), (u1txd <-> u1rts)
#define IOSWAPHS  5 //Sets HSPI with higher prio
#define IOSWAP2HS 6 //Sets Two SPI Masters on HSPI
#define IOSWAP2CS 7 //Sets Two SPI Masters on CSPI

#endif
