
// Do not use the code from u8g2 submodule and skip the complete source here
// if the u8g2 module is not selected.
// Reason: The whole u8g2 submodule code tree might not even exist in this case.
#include "user_modules.h"
#ifdef LUA_USE_MODULES_U8G2

#include <string.h>
#include "c_stdlib.h"

#include "platform.h"

#define U8X8_USE_PINS
#include "u8x8_nodemcu_hal.h"


uint8_t u8x8_gpio_and_delay_nodemcu(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint32_t temp;

  switch(msg)
  {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:  // called once during init phase of u8g2/u8x8
                                      // can be used to setup pins
    for (int idx = 0; idx < U8X8_PIN_OUTPUT_CNT; idx++) {
      if (u8x8->pins[idx] != U8X8_PIN_NONE) {
        // configure pin as output
        if (idx == U8X8_PIN_I2C_CLOCK || idx == U8X8_PIN_I2C_DATA) {
          platform_gpio_mode( u8x8->pins[idx], PLATFORM_GPIO_OPENDRAIN, PLATFORM_GPIO_PULLUP );
        } else {
          platform_gpio_mode( u8x8->pins[idx], PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
        }
      }
    }
    break;

  case U8X8_MSG_DELAY_NANO:           // delay arg_int * 1 nano second
    os_delay_us( 1 );
    break;    

  case U8X8_MSG_DELAY_100NANO:        // delay arg_int * 100 nano seconds
    temp = arg_int * 100;
    temp /= 1000;
    os_delay_us( temp > 0 ? temp : 1 );
    break;

  case U8X8_MSG_DELAY_10MICRO:        // delay arg_int * 10 micro seconds
    os_delay_us( arg_int * 10 );
    break;

  case U8X8_MSG_DELAY_MILLI:          // delay arg_int * 1 milli second
    os_delay_us( arg_int * 1000 );
    break;

  case U8X8_MSG_DELAY_I2C:                // arg_int is the I2C speed in 100KHz, e.g. 4 = 400 KHz
    temp = 5000 / arg_int;                // arg_int=1: delay by 5us, arg_int = 4: delay by 1.25us
    temp /= 1000;
    os_delay_us( temp > 0 ? temp : 1 );
    break;

  case U8X8_MSG_GPIO_D0:              // D0 or SPI clock pin: Output level in arg_int
  //case U8X8_MSG_GPIO_SPI_CLOCK:
    break;
  case U8X8_MSG_GPIO_D1:              // D1 or SPI data pin: Output level in arg_int
  //case U8X8_MSG_GPIO_SPI_DATA:
    break;
  case U8X8_MSG_GPIO_D2:              // D2 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_D3:              // D3 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_D4:              // D4 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_D5:              // D5 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_D6:              // D6 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_D7:              // D7 pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_E:               // E/WR pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_CS:              // CS (chip select) pin: Output level in arg_int
    platform_gpio_write( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;
  case U8X8_MSG_GPIO_DC:              // DC (data/cmd, A0, register select) pin: Output level in arg_int
    platform_gpio_write( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;
  case U8X8_MSG_GPIO_RESET:           // Reset pin: Output level in arg_int
    if (u8x8_GetPinValue( u8x8, msg ) != U8X8_PIN_NONE)
      platform_gpio_write( u8x8_GetPinValue(u8x8, msg), arg_int );
    break;
  case U8X8_MSG_GPIO_CS1:             // CS1 (chip select) pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_CS2:             // CS2 (chip select) pin: Output level in arg_int
    break;

  case U8X8_MSG_GPIO_I2C_CLOCK:       // arg_int=0: Output low at I2C clock pin
                                      // arg_int=1: Input dir with pullup high for I2C clock pin
    // for SW comm routine
    platform_gpio_write( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;

  case U8X8_MSG_GPIO_I2C_DATA:        // arg_int=0: Output low at I2C data pin
                                      // arg_int=1: Input dir with pullup high for I2C data pin
    // for SW comm routine
    platform_gpio_write( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;

  case U8X8_MSG_GPIO_MENU_SELECT:
  case U8X8_MSG_GPIO_MENU_NEXT:
  case U8X8_MSG_GPIO_MENU_PREV:
  case U8X8_MSG_GPIO_MENU_HOME:
    u8x8_SetGPIOResult( u8x8, /* get menu select pin state */ 0 );
    break;
  default:
    u8x8_SetGPIOResult( u8x8, 1 ); // default return value
    break;
  }
  return 1;
}


// static variables containing info about the i2c link
// TODO: move to user space in u8x8_t once available
typedef struct {
  uint8_t id;
} hal_i2c_t;

uint8_t u8x8_byte_nodemcu_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint8_t *data;
  hal_i2c_t *hal = ((u8g2_nodemcu_t *)u8x8)->hal;
 
  switch(msg) {
  case U8X8_MSG_BYTE_SEND:
    if (hal->id == 0) {
      data = (uint8_t *)arg_ptr;
      
      while( arg_int > 0 ) {
        platform_i2c_send_byte( 0, *data );
        data++;
        arg_int--;
      }

    } else {
      // invalid id
      return 0;
    }

    break;

  case U8X8_MSG_BYTE_INIT:
    {
      // the hal member initially contains the i2c id
      int id = (int)hal;
      if (!(hal = c_malloc( sizeof ( hal_i2c_t ) )))
        return 0;
      hal->id = id;
      ((u8g2_nodemcu_t *)u8x8)->hal = hal;
    }
    break;

  case U8X8_MSG_BYTE_SET_DC:
    break;

  case U8X8_MSG_BYTE_START_TRANSFER:
    if (hal->id == 0) {
      platform_i2c_send_start( 0 );
      platform_i2c_send_address( 0, u8x8_GetI2CAddress(u8x8), PLATFORM_I2C_DIRECTION_TRANSMITTER );

    } else {
      // invalid id
      return 0;
    }
    break;

  case U8X8_MSG_BYTE_END_TRANSFER:
    if (hal->id == 0) {
      platform_i2c_send_stop( 0 );

    } else {
      // invalid id
      return 0;
    }
    break;

  default:
    return 0;
  }

  return 1;
}


// static variables containing info about the spi link
// TODO: move to user space in u8x8_t once available
typedef struct {
  uint8_t host;
  //spi_device_handle_t device;
  uint8_t last_dc;
  struct {
    uint8_t *data;
    size_t size, used;
  } buffer;
} hal_spi_t;

static void flush_buffer_spi( hal_spi_t *hal )
{
  if (hal->buffer.used > 0) {
    platform_spi_blkwrite( hal->host, hal->buffer.used, hal->buffer.data );

    hal->buffer.used = 0;
  }
}

uint8_t u8x8_byte_nodemcu_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  hal_spi_t *hal = ((u8g2_nodemcu_t *)u8x8)->hal;
 
  switch(msg) {
  case U8X8_MSG_BYTE_INIT:
    {
      /* disable chipselect */
      u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_disable_level );

      // the hal member initially contains the spi host id
      int host = (int)hal;
      if (!(hal = c_malloc( sizeof ( hal_spi_t ) )))
        return 0;
      hal->host = host;
      ((u8g2_nodemcu_t *)u8x8)->hal = hal;

      hal->last_dc = 0;
    }
    break;

  case U8X8_MSG_BYTE_SET_DC:
    if (hal->last_dc != arg_int)
      flush_buffer_spi( hal );

    u8x8_gpio_SetDC( u8x8, arg_int );
    hal->last_dc = arg_int;
    break;

  case U8X8_MSG_BYTE_START_TRANSFER:
    hal->buffer.size = 256;
    if (!(hal->buffer.data = (uint8_t *)c_malloc( hal->buffer.size )))
      return 0;
    hal->buffer.used = 0;

    u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_enable_level );
    break;

  case U8X8_MSG_BYTE_SEND:
    if (!hal->buffer.data)
      return 0;

    while (hal->buffer.size - hal->buffer.used < arg_int) {
      hal->buffer.size *= 2;
      uint8_t *tmp;
      if (!(tmp = (uint8_t *)c_malloc( hal->buffer.size ))) {
        c_free( hal->buffer.data );
        hal->buffer.data = NULL;
        return 0;
      }
      os_memcpy( tmp, hal->buffer.data, hal->buffer.used );
      c_free( hal->buffer.data );
      hal->buffer.data = tmp;
    }
    os_memcpy( hal->buffer.data + hal->buffer.used, arg_ptr, arg_int );
    hal->buffer.used += arg_int;
    break;

  case U8X8_MSG_BYTE_END_TRANSFER:
    if (!hal->buffer.data)
      return 0;

    flush_buffer_spi( hal );

    u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_disable_level );

    c_free( hal->buffer.data );
    break;

  default:
    return 0;
  }

  return 1;
}

#endif /* LUA_USE_MODULES_U8G2 */
