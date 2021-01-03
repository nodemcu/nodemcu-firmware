//
//    FILE: dht.h
//  AUTHOR: Rob Tillaart
// VERSION: 0.1.14
// PURPOSE: DHT Temperature & Humidity Sensor library for Arduino
//     URL: http://arduino.cc/playground/Main/DHTLib
//
// HISTORY:
// see dht.cpp file
//

#ifndef dht_h
#define dht_h

// #if ARDUINO < 100
// #include <WProgram.h>
// #else
// #include <Arduino.h>
// #endif
#include <stdint.h>

#define DHT_LIB_VERSION "0.1.14"

#define DHTLIB_OK                0
#define DHTLIB_ERROR_CHECKSUM   -1
#define DHTLIB_ERROR_TIMEOUT    -2
#define DHTLIB_INVALID_VALUE    -999

#define DHTLIB_DHT11_WAKEUP     18
#define DHTLIB_DHT_WAKEUP       1
#define DHTLIB_DHT_UNI_WAKEUP   18

// max timeout is 100 usec.
// For a 16 Mhz proc 100 usec is 1600 clock cycles
// loops using DHTLIB_TIMEOUT use at least 4 clock cycli
// so 100 us takes max 400 loops
// so by dividing F_CPU by 40000 we "fail" as fast as possible
// ESP8266 uses delay_us get 1us time
#define DHTLIB_TIMEOUT (100)

// Platform specific I/O definitions

#define DIRECT_READ(pin)         (0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[pin])))
#define DIRECT_MODE_INPUT(pin)   GPIO_DIS_OUTPUT(pin_num[pin])
#define DIRECT_MODE_OUTPUT(pin)
#define DIRECT_WRITE_LOW(pin)    (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 0))
#define DIRECT_WRITE_HIGH(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), 1))

typedef enum {
    DHT11 = 0,
    DHT12,
    DHT22,
    DHT_NON11
} dht_type;

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read(uint8_t pin, dht_type type);
double dht_getHumidity(void);
double dht_getTemperature(void);

#endif
//
// END OF FILE
//
