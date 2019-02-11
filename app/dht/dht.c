//
//    FILE: dht.cpp
//  AUTHOR: Rob Tillaart
// VERSION: 0.1.14
// PURPOSE: DHT Temperature & Humidity Sensor library for Arduino
//     URL: http://arduino.cc/playground/Main/DHTLib
//
// HISTORY:
// 0.1.14 replace digital read with faster (~3x) code => more robust low MHz machines.
// 0.1.13 fix negative dht_temperature
// 0.1.12 support DHT33 and DHT44 initial version
// 0.1.11 renamed DHTLIB_TIMEOUT
// 0.1.10 optimized faster WAKEUP + TIMEOUT
// 0.1.09 optimize size: timeout check + use of mask
// 0.1.08 added formula for timeout based upon clockspeed
// 0.1.07 added support for DHT21
// 0.1.06 minimize footprint (2012-12-27)
// 0.1.05 fixed negative dht_temperature bug (thanks to Roseman)
// 0.1.04 improved readability of code using DHTLIB_OK in code
// 0.1.03 added error values for temp and dht_humidity when read failed
// 0.1.02 added error codes
// 0.1.01 added support for Arduino 1.0, fixed typos (31/12/2011)
// 0.1.00 by Rob Tillaart (01/04/2011)
//
// inspired by DHT11 library
//
// Released to the public domain
//

#include "user_interface.h"
#include "platform.h"
#include "c_stdio.h"
#include "dht.h"

#ifndef LOW
#define LOW     0
#endif /* ifndef LOW */

#ifndef HIGH
#define HIGH    1
#endif /* ifndef HIGH */

#define COMBINE_HIGH_AND_LOW_BYTE(byte_high, byte_low)  (((byte_high) << 8) | (byte_low))

static double dht_humidity;
static double dht_temperature;

static uint8_t dht_bytes[5];  // buffer to receive data
static int dht_readSensor(uint8_t pin, uint8_t wakeupDelay);

/////////////////////////////////////////////////////
//
// PUBLIC
//

// return values:
// Humidity
double dht_getHumidity(void)
{
    return dht_humidity;
}

// return values:
// Temperature
double dht_getTemperature(void)
{
    return dht_temperature;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read_universal(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT_UNI_WAKEUP);
    if (rv != DHTLIB_OK)
    {
        dht_humidity    = DHTLIB_INVALID_VALUE;  // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE;  // invalid value
        return rv; // propagate error value
    }

#if defined(DHT_DEBUG_BYTES)
    int i;
    for (i = 0; i < 5; i++)
    {
        DHT_DEBUG("%02X\n", dht_bytes[i]);
    }
#endif // defined(DHT_DEBUG_BYTES)

    // Assume it is DHT11
    // If it is DHT11, both bit[1] and bit[3] is 0
    if ((dht_bytes[1] == 0) && (dht_bytes[3] == 0))
    {
        // It may DHT11
        // CONVERT AND STORE
        DHT_DEBUG("DHT11 method\n");
        dht_humidity    = dht_bytes[0];  // dht_bytes[1] == 0;
        dht_temperature = dht_bytes[2];  // dht_bytes[3] == 0;

        // TEST CHECKSUM
        // dht_bytes[1] && dht_bytes[3] both 0
        uint8_t sum = dht_bytes[0] + dht_bytes[2];
        if (dht_bytes[4] != sum)
        {
            // It may not DHT11
            dht_humidity    = DHTLIB_INVALID_VALUE; // invalid value, or is NaN prefered?
            dht_temperature = DHTLIB_INVALID_VALUE; // invalid value
            // Do nothing
        }
        else
        {
            return DHTLIB_OK;
        }
    }

    // Assume it is not DHT11
    // CONVERT AND STORE
    DHT_DEBUG("DHTxx method\n");
    dht_humidity = (double)COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[0], dht_bytes[1]) * 0.1;
    dht_temperature = (double)COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[2] & 0x7F, dht_bytes[3]) * 0.1;
    if (dht_bytes[2] & 0x80)  // negative dht_temperature
    {
        dht_temperature = -dht_temperature;
    }

    // TEST CHECKSUM
    uint8_t sum = dht_bytes[0] + dht_bytes[1] + dht_bytes[2] + dht_bytes[3];
    if (dht_bytes[4] != sum)
    {
        return DHTLIB_ERROR_CHECKSUM;
    }
    return DHTLIB_OK;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read11(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT11_WAKEUP);
    if (rv != DHTLIB_OK)
    {
        dht_humidity    = DHTLIB_INVALID_VALUE; // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE; // invalid value
        return rv;
    }

    // CONVERT AND STORE
    dht_humidity    = dht_bytes[0];  // dht_bytes[1] == 0;
    dht_temperature = dht_bytes[2];  // dht_bytes[3] == 0;

    // TEST CHECKSUM
    // dht_bytes[1] && dht_bytes[3] both 0
    uint8_t sum = dht_bytes[0] + dht_bytes[2];
    if (dht_bytes[4] != sum) return DHTLIB_ERROR_CHECKSUM;

    return DHTLIB_OK;
}


// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read(uint8_t pin)
{
    // READ VALUES
    int rv = dht_readSensor(pin, DHTLIB_DHT_WAKEUP);
    if (rv != DHTLIB_OK)
    {
        dht_humidity    = DHTLIB_INVALID_VALUE;  // invalid value, or is NaN prefered?
        dht_temperature = DHTLIB_INVALID_VALUE;  // invalid value
        return rv; // propagate error value
    }

    // CONVERT AND STORE
    dht_humidity = (double)COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[0], dht_bytes[1]) * 0.1;
    dht_temperature = (double)COMBINE_HIGH_AND_LOW_BYTE(dht_bytes[2] & 0x7F, dht_bytes[3]) * 0.1;
    if (dht_bytes[2] & 0x80)  // negative dht_temperature
    {
        dht_temperature = -dht_temperature;
    }

    // TEST CHECKSUM
    uint8_t sum = dht_bytes[0] + dht_bytes[1] + dht_bytes[2] + dht_bytes[3];
    if (dht_bytes[4] != sum)
    {
        return DHTLIB_ERROR_CHECKSUM;
    }
    return DHTLIB_OK;
}

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read21(uint8_t pin)  __attribute__((alias("dht_read")));

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read22(uint8_t pin)  __attribute__((alias("dht_read")));

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read33(uint8_t pin)  __attribute__((alias("dht_read")));

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int dht_read44(uint8_t pin)  __attribute__((alias("dht_read")));

/////////////////////////////////////////////////////
//
// PRIVATE
//

// return values:
// DHTLIB_OK
// DHTLIB_ERROR_TIMEOUT
int dht_readSensor(uint8_t pin, uint8_t wakeupDelay)
{
    // INIT BUFFERVAR TO RECEIVE DATA
    uint8_t mask = 128;
    uint8_t idx = 0;
    uint8_t i = 0;

    // replace digitalRead() with Direct Port Reads.
    // reduces footprint ~100 bytes => portability issue?
    // direct port read is about 3x faster
    // uint8_t bit = digitalPinToBitMask(pin);
    // uint8_t port = digitalPinToPort(pin);
    // volatile uint8_t *PIR = portInputRegister(port);

    // EMPTY BUFFER
    for (i = 0; i < 5; i++) dht_bytes[i] = 0;

    // REQUEST SAMPLE
    // pinMode(pin, OUTPUT);
    platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_PULLUP);
    DIRECT_MODE_OUTPUT(pin);
    // digitalWrite(pin, LOW); // T-be
    DIRECT_WRITE_LOW(pin);
    // delay(wakeupDelay);
    for (i = 0; i < wakeupDelay; i++) os_delay_us(1000);
    // Disable interrupts
    ets_intr_lock();
    // digitalWrite(pin, HIGH);   // T-go
    DIRECT_WRITE_HIGH(pin);
    os_delay_us(40);
    // pinMode(pin, INPUT);
    DIRECT_MODE_INPUT(pin);

    // GET ACKNOWLEDGE or TIMEOUT
    uint16_t loopCntLOW = DHTLIB_TIMEOUT;
    while (DIRECT_READ(pin) == LOW )  // T-rel
    {
        os_delay_us(1);
        if (--loopCntLOW == 0) return DHTLIB_ERROR_TIMEOUT;
    }

    uint16_t loopCntHIGH = DHTLIB_TIMEOUT;
    while (DIRECT_READ(pin) != LOW )  // T-reh
    {
        os_delay_us(1);
        if (--loopCntHIGH == 0) return DHTLIB_ERROR_TIMEOUT;
    }

    // READ THE OUTPUT - 40 BITS => 5 BYTES
    for (i = 40; i != 0; i--)
    {
        loopCntLOW = DHTLIB_TIMEOUT;
        while (DIRECT_READ(pin) == LOW )
        {
            os_delay_us(1);
            if (--loopCntLOW == 0) return DHTLIB_ERROR_TIMEOUT;
        }

        uint32_t t = system_get_time();

        loopCntHIGH = DHTLIB_TIMEOUT;
        while (DIRECT_READ(pin) != LOW )
        {
            os_delay_us(1);
            if (--loopCntHIGH == 0) return DHTLIB_ERROR_TIMEOUT;
        }

        if ((system_get_time() - t) > 40)
        {
            dht_bytes[idx] |= mask;
        }
        mask >>= 1;
        if (mask == 0)   // next byte?
        {
            mask = 128;
            idx++;
        }
    }
    // Enable interrupts
    ets_intr_unlock();
    // pinMode(pin, OUTPUT);
    DIRECT_MODE_OUTPUT(pin);
    // digitalWrite(pin, HIGH);
    DIRECT_WRITE_HIGH(pin);

    return DHTLIB_OK;
}
//
// END OF FILE
//
