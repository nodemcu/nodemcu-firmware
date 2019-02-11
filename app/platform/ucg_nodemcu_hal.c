// Do not use the code from ucg submodule and skip the complete source here
// if the ucg module is not selected.
// Reason: The whole ucg submodule code tree might not even exist in this case.
#include "user_modules.h"
#ifdef LUA_USE_MODULES_UCG

#include <string.h>
#include "c_stdlib.h"

#include "platform.h"

#define USE_PIN_LIST
#include "ucg_nodemcu_hal.h"

#define delayMicroseconds os_delay_us


static spi_data_type cache;
static uint8_t cached;

#define CACHED_TRANSFER(dat, num) cache = cached = 0;  \
    while( arg > 0 ) {                                \
        if (cached == 4) {                                              \
            platform_spi_transaction( 1, 0, 0, 32, cache, 0, 0, 0 );    \
            cache = cached = 0;                                         \
        }                                                               \
        cache = (cache << num*8) | dat;                                 \
        cached += num;                                                  \
        arg--;                                                          \
    }                                                                   \
    if (cached > 0) {                                                   \
        platform_spi_transaction( 1, 0, 0, cached * 8, cache, 0, 0, 0 ); \
    }


int16_t ucg_com_nodemcu_hw_spi(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data)
{
  switch(msg)
  {
    case UCG_COM_MSG_POWER_UP:
        /* "data" is a pointer to ucg_com_info_t structure with the following information: */
        /*	((ucg_com_info_t *)data)->serial_clk_speed value in nanoseconds */
        /*	((ucg_com_info_t *)data)->parallel_clk_speed value in nanoseconds */

        /* setup pins */

        // we assume that the SPI interface was already initialized
        // just care for the /CS and D/C pins
        //platform_gpio_write( ucg->pin_list[0], value );

        if ( ucg->pin_list[UCG_PIN_RST] != UCG_PIN_VAL_NONE )
            platform_gpio_mode( ucg->pin_list[UCG_PIN_RST], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
        platform_gpio_mode( ucg->pin_list[UCG_PIN_CD], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );

        if ( ucg->pin_list[UCG_PIN_CS] != UCG_PIN_VAL_NONE )
            platform_gpio_mode( ucg->pin_list[UCG_PIN_CS], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );

        break;

    case UCG_COM_MSG_POWER_DOWN:
        break;

    case UCG_COM_MSG_DELAY:
        delayMicroseconds(arg);
        break;

    case UCG_COM_MSG_CHANGE_RESET_LINE:
        if ( ucg->pin_list[UCG_PIN_RST] != UCG_PIN_VAL_NONE )
            platform_gpio_write( ucg->pin_list[UCG_PIN_RST], arg );
        break;

    case UCG_COM_MSG_CHANGE_CS_LINE:
        if ( ucg->pin_list[UCG_PIN_CS] != UCG_PIN_VAL_NONE )
            platform_gpio_write( ucg->pin_list[UCG_PIN_CS], arg );
        break;

    case UCG_COM_MSG_CHANGE_CD_LINE:
        platform_gpio_write( ucg->pin_list[UCG_PIN_CD], arg );
        break;

    case UCG_COM_MSG_SEND_BYTE:
        platform_spi_send( 1, 8, arg );
        break;

    case UCG_COM_MSG_REPEAT_1_BYTE:
        CACHED_TRANSFER(data[0], 1);
        break;

    case UCG_COM_MSG_REPEAT_2_BYTES:
        CACHED_TRANSFER((data[0] << 8) | data[1], 2);
        break;

    case UCG_COM_MSG_REPEAT_3_BYTES:
        while( arg > 0 ) {
            platform_spi_transaction( 1, 0, 0, 24, (data[0] << 16) | (data[1] << 8) | data[2], 0, 0, 0 );
            arg--;
        }
        break;

    case UCG_COM_MSG_SEND_STR:
        CACHED_TRANSFER(*data++, 1);
        break;

    case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
        while(arg > 0)
        {
            if ( *data != 0 )
            {
                /* set the data line directly, ignore the setting from UCG_CFG_CD */
                if ( *data == 1 )
                {
                    platform_gpio_write( ucg->pin_list[UCG_PIN_CD], 0 );
                }
                else
                {
                    platform_gpio_write( ucg->pin_list[UCG_PIN_CD], 1 );
                }
            }
            data++;
            platform_spi_send( 1, 8, *data );
            data++;
            arg--;
        }
        break;
  }
  return 1;
}

#endif /* LUA_USE_MODULES_UCG */
