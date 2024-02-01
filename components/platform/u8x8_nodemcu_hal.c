#include "u8x8_nodemcu_hal.h"
#include "platform.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_heap_caps.h"
#include <string.h>


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
        gpio_config_t cfg;
        memset( (void *)&cfg, 0, sizeof( cfg ) );
        cfg.pin_bit_mask = 1ULL << u8x8->pins[idx];

        if (idx == U8X8_PIN_I2C_CLOCK || idx == U8X8_PIN_I2C_DATA) {
          cfg.mode = GPIO_MODE_OUTPUT_OD;
          cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        } else {
          cfg.mode = GPIO_MODE_OUTPUT;
          cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        }
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config( &cfg );
      }
    }
    break;

  case U8X8_MSG_DELAY_NANO:           // delay arg_int * 1 nano second
    esp_rom_delay_us( 1 );
    break;    

  case U8X8_MSG_DELAY_100NANO:        // delay arg_int * 100 nano seconds
    temp = arg_int * 100;
    temp /= 1000;
    esp_rom_delay_us( temp > 0 ? temp : 1 );
    break;

  case U8X8_MSG_DELAY_10MICRO:        // delay arg_int * 10 micro seconds
    esp_rom_delay_us( arg_int * 10 );
    break;

  case U8X8_MSG_DELAY_MILLI:          // delay arg_int * 1 milli second
    esp_rom_delay_us( arg_int * 1000 );
    break;

  case U8X8_MSG_DELAY_I2C:                // arg_int is the I2C speed in 100KHz, e.g. 4 = 400 KHz
    temp = 5000 / arg_int;                // arg_int=1: delay by 5us, arg_int = 4: delay by 1.25us
    temp /= 1000;
    esp_rom_delay_us( temp > 0 ? temp : 1 );
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
    gpio_set_level( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;
  case U8X8_MSG_GPIO_DC:              // DC (data/cmd, A0, register select) pin: Output level in arg_int
    gpio_set_level( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;
  case U8X8_MSG_GPIO_RESET:           // Reset pin: Output level in arg_int
    if (u8x8_GetPinValue( u8x8, msg ) != U8X8_PIN_NONE)
      gpio_set_level( u8x8_GetPinValue(u8x8, msg), arg_int );
    break;
  case U8X8_MSG_GPIO_CS1:             // CS1 (chip select) pin: Output level in arg_int
    break;
  case U8X8_MSG_GPIO_CS2:             // CS2 (chip select) pin: Output level in arg_int
    break;

  case U8X8_MSG_GPIO_I2C_CLOCK:       // arg_int=0: Output low at I2C clock pin
                                      // arg_int=1: Input dir with pullup high for I2C clock pin
    // for SW comm routine
    gpio_set_level( u8x8_GetPinValue( u8x8, msg ), arg_int );
    break;

  case U8X8_MSG_GPIO_I2C_DATA:        // arg_int=0: Output low at I2C data pin
                                      // arg_int=1: Input dir with pullup high for I2C data pin
    // for SW comm routine
    gpio_set_level( u8x8_GetPinValue( u8x8, msg ), arg_int );
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
  i2c_cmd_handle_t cmd;
  struct {
    uint8_t *data;
    size_t size, used;
  } buffer;
} hal_i2c_t;

uint8_t u8x8_byte_nodemcu_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  uint8_t *data;
  hal_i2c_t *hal = u8x8->user_ptr;
 
  switch(msg) {
  case U8X8_MSG_BYTE_SEND:
    if (hal->id == 0) {
      data = (uint8_t *)arg_ptr;
      
      while( arg_int > 0 ) {
        platform_i2c_send_byte( 0, *data, 0 );
        data++;
        arg_int--;
      }

    } else {
      while (hal->buffer.size - hal->buffer.used < arg_int) {
        hal->buffer.size *= 2;
        if (!(hal->buffer.data = (uint8_t *)realloc( hal->buffer.data, hal->buffer.size )))
          return 0;
      }
      memcpy( hal->buffer.data + hal->buffer.used, arg_ptr, arg_int );
      hal->buffer.used += arg_int;
    }

    break;

  case U8X8_MSG_BYTE_INIT:
    {
      // the user pointer initially contains the i2c id
      int id = (int)hal;
      if (!(hal = malloc( sizeof ( hal_i2c_t ) )))
        return 0;
      hal->id = id;
      u8x8->user_ptr = hal;
    }
    break;

  case U8X8_MSG_BYTE_SET_DC:
    break;

  case U8X8_MSG_BYTE_START_TRANSFER:
    if (hal->id == 0) {
      platform_i2c_send_start( 0 );
      platform_i2c_send_address( 0, u8x8_GetI2CAddress(u8x8), PLATFORM_I2C_DIRECTION_TRANSMITTER, 0 );

    } else {
      hal->buffer.size = 256;
      if (!(hal->buffer.data = (uint8_t *)malloc( hal->buffer.size )))
        return 0;
      hal->buffer.used = 0;

      hal->cmd = i2c_cmd_link_create();
      i2c_master_start( hal->cmd );
      i2c_master_write_byte( hal->cmd, u8x8_GetI2CAddress(u8x8) << 1 | I2C_MASTER_WRITE, false );
    }
    break;

  case U8X8_MSG_BYTE_END_TRANSFER:
    if (hal->id == 0) {
      platform_i2c_send_stop( 0 );

    } else {
      if (hal->buffer.used > 0)
        i2c_master_write( hal->cmd, hal->buffer.data, hal->buffer.used, false );

      i2c_master_stop( hal->cmd );
      i2c_master_cmd_begin(hal->id-1, hal->cmd, portMAX_DELAY );

      if (hal->buffer.data)
        free( hal->buffer.data );
      i2c_cmd_link_delete( hal->cmd );
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
  spi_device_handle_t device;
  uint8_t last_dc;
  struct {
    uint8_t *data;
    size_t size, used;
  } buffer;
} hal_spi_t;

static void flush_buffer_spi( hal_spi_t *hal )
{
  if (hal->buffer.used > 0) {
    spi_transaction_t trans;
    memset( &trans, 0, sizeof( trans ) );
    trans.length = hal->buffer.used * 8;
    trans.tx_buffer = hal->buffer.data;
    spi_device_transmit( hal->device, &trans );

    hal->buffer.used = 0;
  }
}

uint8_t u8x8_byte_nodemcu_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  hal_spi_t *hal = u8x8->user_ptr;
 
  switch(msg) {
  case U8X8_MSG_BYTE_INIT:
    {
      /* disable chipselect */
      u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_disable_level );

      // the user pointer initially contains the spi host id
      int host = (int)hal;
      if (!(hal = malloc( sizeof ( hal_spi_t ) )))
        return 0;
      hal->host = host;
      u8x8->user_ptr = hal;

      // set up the spi device
      spi_device_interface_config_t config;
      memset( &config, 0, sizeof( config ) );

      config.spics_io_num = -1;  // CS is controlled by u8x8 gpio mechanism
      config.mode = u8x8_GetSPIClockPhase( u8x8 ) | (u8x8_GetSPIClockPolarity( u8x8 ) << 1);
      config.clock_speed_hz = u8x8->display_info->sck_clock_hz;
      config.queue_size = 1;

      spi_bus_add_device( hal->host, &config, &(hal->device) );

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
    if (!(hal->buffer.data = (uint8_t *)heap_caps_malloc( hal->buffer.size, MALLOC_CAP_DMA )))
      return 0;
    hal->buffer.used = 0;

    u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_enable_level );
    break;

  case U8X8_MSG_BYTE_SEND:
    while (hal->buffer.size - hal->buffer.used < arg_int) {
      hal->buffer.size *= 2;
      uint8_t *tmp;
      if (!(tmp = (uint8_t *)heap_caps_malloc( hal->buffer.size, MALLOC_CAP_DMA ))) {
        heap_caps_free( hal->buffer.data );
        hal->buffer.data = NULL;
        return 0;
      }
      memcpy( tmp, hal->buffer.data, hal->buffer.used );
      heap_caps_free( hal->buffer.data );
      hal->buffer.data = tmp;
    }
    memcpy( hal->buffer.data + hal->buffer.used, arg_ptr, arg_int );
    hal->buffer.used += arg_int;
    break;

  case U8X8_MSG_BYTE_END_TRANSFER:
    flush_buffer_spi( hal );

    u8x8_gpio_SetCS( u8x8, u8x8->display_info->chip_disable_level );

    if (hal->buffer.data)
      heap_caps_free( hal->buffer.data );
    break;

  default:
    return 0;
  }

  return 1;
}
