/**
 * External modules library
 */

#ifndef __MODULES_H__
#define __MODULES_H__

#if defined(LUA_USE_MODULES_GPIO)
#define MODULES_GPIO        "gpio"
#define ROM_MODULES_GPIO    \
    _ROM(MODULES_GPIO, luaopen_gpio, gpio_map)
#else
#define ROM_MODULES_GPIO
#endif

#if defined(LUA_USE_MODULES_PWM)
#define MODULES_PWM         "pwm"
#define ROM_MODULES_PWM     \
    _ROM(MODULES_PWM, luaopen_pwm, pwm_map)
#else
#define ROM_MODULES_PWM
#endif

#if defined(LUA_USE_MODULES_WIFI)
#define MODULES_WIFI        "wifi"
#define ROM_MODULES_WIFI    \
    _ROM(MODULES_WIFI, luaopen_wifi, wifi_map)
#else
#define ROM_MODULES_WIFI
#endif

#if defined(LUA_USE_MODULES_NET)
#define MODULES_NET         "net"
#define ROM_MODULES_NET     \
    _ROM(MODULES_NET, luaopen_net, net_map)
#else
#define ROM_MODULES_NET
#endif

#if defined(LUA_USE_MODULES_COAP)
#define MODULES_COAP        "coap"
#define ROM_MODULES_COAP    \
    _ROM(MODULES_COAP, luaopen_coap, coap_map)
#else
#define ROM_MODULES_COAP
#endif

#if defined(LUA_USE_MODULES_MQTT)
#define MODULES_MQTT        "mqtt"
#define ROM_MODULES_MQTT    \
    _ROM(MODULES_MQTT, luaopen_mqtt, mqtt_map)
#else
#define ROM_MODULES_MQTT
#endif

#if defined(LUA_USE_MODULES_U8G)
#define MODULES_U8G         "u8g"
#define ROM_MODULES_U8G     \
    _ROM(MODULES_U8G, luaopen_u8g, lu8g_map)
#else
#define ROM_MODULES_U8G
#endif

#if defined(LUA_USE_MODULES_UCG)
#define MODULES_UCG         "ucg"
#define ROM_MODULES_UCG     \
    _ROM(MODULES_UCG, luaopen_ucg, lucg_map)
#else
#define ROM_MODULES_UCG
#endif

#if defined(LUA_USE_MODULES_I2C)
#define MODULES_I2C         "i2c"
#define ROM_MODULES_I2C     \
    _ROM(MODULES_I2C, luaopen_i2c, i2c_map)
#else
#define ROM_MODULES_I2C
#endif

#if defined(LUA_USE_MODULES_SPI)
#define MODULES_SPI         "spi"
#define ROM_MODULES_SPI     \
    _ROM(MODULES_SPI, luaopen_spi, spi_map)
#else
#define ROM_MODULES_SPI
#endif

#if defined(LUA_USE_MODULES_TMR)
#define MODULES_TMR         "tmr"
#define ROM_MODULES_TMR     \
    _ROM(MODULES_TMR, luaopen_tmr, tmr_map)
#else
#define ROM_MODULES_TMR
#endif

#if defined(LUA_USE_MODULES_NODE)
#define MODULES_NODE        "node"
#define ROM_MODULES_NODE    \
    _ROM(MODULES_NODE, luaopen_node, node_map)
#else
#define ROM_MODULES_NODE
#endif

#if defined(LUA_USE_MODULES_FILE)
#define MODULES_FILE        "file"
#define ROM_MODULES_FILE    \
    _ROM(MODULES_FILE, luaopen_file, file_map)
#else
#define ROM_MODULES_FILE
#endif

#if defined(LUA_USE_MODULES_ADC)
#define MODULES_ADC         "adc"
#define ROM_MODULES_ADC     \
    _ROM(MODULES_ADC, luaopen_adc, adc_map)
#else
#define ROM_MODULES_ADC
#endif

#if defined(LUA_USE_MODULES_UART)
#define MODULES_UART        "uart"
#define ROM_MODULES_UART    \
    _ROM(MODULES_UART, luaopen_uart, uart_map)
#else
#define ROM_MODULES_UART
#endif

#if defined(LUA_USE_MODULES_OW)
#define MODULES_OW          "ow"
#define ROM_MODULES_OW      \
    _ROM(MODULES_OW, luaopen_ow, ow_map)
#else
#define ROM_MODULES_OW
#endif

#if defined(LUA_USE_MODULES_BIT)
#define MODULES_BIT         "bit"
#define ROM_MODULES_BIT     \
    _ROM(MODULES_BIT, luaopen_bit, bit_map)
#else
#define ROM_MODULES_BIT
#endif

#if defined(LUA_USE_MODULES_WS2801)
#define MODULES_WS2801      "ws2801"
#define ROM_MODULES_WS2801  \
    _ROM(MODULES_WS2801, luaopen_ws2801, ws2801_map)
#else
#define ROM_MODULES_WS2801
#endif

#if defined(LUA_USE_MODULES_WS2812)
#define MODULES_WS2812      "ws2812"
#define ROM_MODULES_WS2812  \
    _ROM(MODULES_WS2812, luaopen_ws2812, ws2812_map)
#else
#define ROM_MODULES_WS2812
#endif

#if defined(LUA_USE_MODULES_ENDUSER_SETUP)
#define MODULES_ENDUSER_SETUP      "enduser_setup"
#define ROM_MODULES_ENDUSER_SETUP  \
    _ROM(MODULES_ENDUSER_SETUP, luaopen_enduser_setup, enduser_setup_map)
#else
#define ROM_MODULES_ENDUSER_SETUP
#endif

#if defined(LUA_USE_MODULES_CJSON)
#define MODULES_CJSON       "cjson"
#define ROM_MODULES_CJSON   \
    _ROM(MODULES_CJSON, luaopen_cjson, cjson_map)
#else
#define ROM_MODULES_CJSON
#endif

#if defined(LUA_USE_MODULES_CRYPTO)
#define MODULES_CRYPTO      "crypto"
#define ROM_MODULES_CRYPTO  \
    _ROM(MODULES_CRYPTO, luaopen_crypto, crypto_map)
#else
#define ROM_MODULES_CRYPTO
#endif

#if defined(LUA_USE_MODULES_RC)
#define MODULES_RC          "rc"
#define ROM_MODULES_RC      \
    _ROM(MODULES_RC, luaopen_rc, rc_map)
#else
#define ROM_MODULES_RC
#endif

#if defined(LUA_USE_MODULES_DHT)
#define MODULES_DHT         "dht"
#define ROM_MODULES_DHT     \
    _ROM(MODULES_DHT, luaopen_dht, dht_map)
#else
#define ROM_MODULES_DHT
#endif

#if defined(LUA_USE_MODULES_RTCMEM)
#define MODULES_RTCMEM      "rtcmem"
#define ROM_MODULES_RTCMEM  \
    _ROM(MODULES_RTCMEM, luaopen_rtcmem, rtcmem_map)
#else
#define ROM_MODULES_RTCMEM
#endif

#if defined(LUA_USE_MODULES_RTCTIME)
#define MODULES_RTCTIME      "rtctime"
#define ROM_MODULES_RTCTIME  \
    _ROM(MODULES_RTCTIME, luaopen_rtctime, rtctime_map)
#else
#define ROM_MODULES_RTCTIME
#endif

#if defined(LUA_USE_MODULES_RTCFIFO)
#define MODULES_RTCFIFO      "rtcfifo"
#define ROM_MODULES_RTCFIFO  \
    _ROM(MODULES_RTCFIFO, luaopen_rtcfifo, rtcfifo_map)
#else
#define ROM_MODULES_RTCFIFO
#endif

#if defined(LUA_USE_MODULES_SNTP)
#define MODULES_SNTP      "sntp"
#define ROM_MODULES_SNTP  \
    _ROM(MODULES_SNTP, luaopen_sntp, sntp_map)
#else
#define ROM_MODULES_SNTP
#endif

#if defined(LUA_USE_MODULES_BMP085)
#define MODULES_BMP085      "bmp085"
#define ROM_MODULES_BMP085  \
    _ROM(MODULES_BMP085, luaopen_bmp085, bmp085_map)
#else
#define ROM_MODULES_BMP085
#endif

#if defined(LUA_USE_MODULES_TSL2561)
#define MODULES_TSL2561      "tsl2561"
#define ROM_MODULES_TSL2561  \
    _ROM(MODULES_TSL2561, luaopen_tsl2561, tsl2561_map)
#else
#define ROM_MODULES_TSL2561
#endif

#if defined(LUA_USE_MODULES_HX711)
#define MODULES_HX711      "hx711"
#define ROM_MODULES_HX711  \
    _ROM(MODULES_HX711, luaopen_hx711, hx711_map)
#else
#define ROM_MODULES_HX711
#endif

#define LUA_MODULES_ROM     \
        ROM_MODULES_GPIO    \
        ROM_MODULES_PWM		\
        ROM_MODULES_WIFI	\
        ROM_MODULES_COAP	\
        ROM_MODULES_MQTT    \
        ROM_MODULES_U8G     \
        ROM_MODULES_UCG     \
        ROM_MODULES_I2C     \
        ROM_MODULES_SPI     \
        ROM_MODULES_TMR     \
        ROM_MODULES_NODE    \
        ROM_MODULES_FILE    \
        ROM_MODULES_NET     \
        ROM_MODULES_ADC     \
        ROM_MODULES_UART    \
        ROM_MODULES_OW      \
        ROM_MODULES_BIT     \
        ROM_MODULES_ENDUSER_SETUP \
        ROM_MODULES_WS2801  \
        ROM_MODULES_WS2812  \
        ROM_MODULES_CJSON   \
        ROM_MODULES_CRYPTO  \
        ROM_MODULES_RC      \
        ROM_MODULES_DHT     \
        ROM_MODULES_RTCMEM  \
        ROM_MODULES_RTCTIME \
        ROM_MODULES_RTCFIFO \
        ROM_MODULES_SNTP    \
        ROM_MODULES_BMP085  \
        ROM_MODULES_TSL2561 \
        ROM_MODULES_HX711  

#endif
