
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "platform.h"

#include "ucg_nodemcu_hal.h"


// static variables containing info about the spi link
// TODO: move to user space once available
typedef struct {
  uint8_t host;
  spi_device_handle_t device;
  uint8_t last_dc;
} hal_spi_t;


// transfers with payload smaller than this should be done in
// polling mode to save overhead
#define SPI_TRANSFER_POLLING_TRESHOLD 16

static void send_byte( hal_spi_t *hal, uint8_t data ) {
  spi_transaction_t trans;
  memset( &trans, 0, sizeof( trans ) );
  trans.flags = SPI_TRANS_USE_TXDATA;
  trans.length = 8;
  trans.tx_data[0] = data;
  spi_device_polling_transmit( hal->device, &trans );
}


// send buffer in DMA'able RAM for caching data in bigger transfers
#define SEND_BUFFER_SIZE (2*3 * 20)
static DMA_ATTR uint8_t send_buffer[SEND_BUFFER_SIZE];

int16_t ucg_com_nodemcu_hw_spi(ucg_t *ucg, int16_t msg, uint16_t arg, uint8_t *data)
{
  hal_spi_t *hal = ((ucg_nodemcu_t *)ucg)->hal;

  switch(msg) {
  case UCG_COM_MSG_POWER_UP:
    /* "data" is a pointer to ucg_com_info_t structure with the following information: */
    /*	((ucg_com_info_t *)data)->serial_clk_speed value in nanoseconds */
    /*	((ucg_com_info_t *)data)->parallel_clk_speed value in nanoseconds */
      
    /* setup pins */
    for (int idx = 0; idx < UCG_PIN_COUNT; idx++) {
      if (ucg->pin_list[idx] != UCG_PIN_VAL_NONE) {
        // configure pin as output
        gpio_config_t cfg;
        memset( (void *)&cfg, 0, sizeof( cfg ) );
        cfg.pin_bit_mask = 1ULL << ucg->pin_list[idx];
        cfg.mode = GPIO_MODE_OUTPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config( &cfg );
      }
    }

    {
      // the hal member initially contains the spi host id
      int host = (int)hal;
      if (!(hal = malloc( sizeof ( hal_spi_t ) )))
        return 0;
      hal->host = host;
      ((ucg_nodemcu_t *)ucg)->hal = hal;

      // set up the spi device
      spi_device_interface_config_t config;
      memset( &config, 0, sizeof( config ) );

      config.spics_io_num = -1;  // CS is controlled by ucg gpio mechanism
      config.mode = 0;
      config.clock_speed_hz = 10000000;
      config.queue_size = 1;

      spi_bus_add_device( hal->host, &config, &(hal->device) );

      hal->last_dc = 0;
    }
    break;

    case UCG_COM_MSG_POWER_DOWN:
      break;

    case UCG_COM_MSG_DELAY:
      esp_rom_delay_us(arg);
      break;

    case UCG_COM_MSG_CHANGE_RESET_LINE:
      if (ucg->pin_list[UCG_PIN_RST] != UCG_PIN_VAL_NONE)
        gpio_set_level( ucg->pin_list[UCG_PIN_RST], arg );
      break;

    case UCG_COM_MSG_CHANGE_CS_LINE:
      if ( ucg->pin_list[UCG_PIN_CS] != UCG_PIN_VAL_NONE )
        gpio_set_level( ucg->pin_list[UCG_PIN_CS], arg );
      break;

    case UCG_COM_MSG_CHANGE_CD_LINE:
      gpio_set_level( ucg->pin_list[UCG_PIN_CD], arg );
      break;

    case UCG_COM_MSG_SEND_BYTE:
      send_byte( hal, arg );
      break;

    case UCG_COM_MSG_REPEAT_1_BYTE:
      {
        spi_transaction_t trans;
        memset( &trans, 0, sizeof( trans ) );
        trans.tx_buffer = send_buffer;
        while (arg > 0) {
          size_t idx;
          for (idx = 0; (idx < SEND_BUFFER_SIZE) && (arg > 0); idx++, arg--) {
            send_buffer[idx] = data[0];
          }
          trans.length = idx * 8;
          trans.rxlength = 0;
          if (idx < SPI_TRANSFER_POLLING_TRESHOLD) {
            spi_device_polling_transmit( hal->device, &trans );
          } else {
            spi_device_transmit( hal->device, &trans );
          }
        }
      }
      break;

    case UCG_COM_MSG_REPEAT_2_BYTES:
      {
        spi_transaction_t trans;
        memset( &trans, 0, sizeof( trans ) );
        trans.tx_buffer = send_buffer;
        while (arg > 0) {
          size_t idx;
          for (idx = 0; (idx < SEND_BUFFER_SIZE) && (arg > 0); idx += 2, arg--) {
            send_buffer[idx]   = data[0];
            send_buffer[idx+1] = data[1];
          }
          trans.length = idx * 8;
          trans.rxlength = 0;
          if (idx < SPI_TRANSFER_POLLING_TRESHOLD) {
            spi_device_polling_transmit( hal->device, &trans );
          } else {
            spi_device_transmit( hal->device, &trans );
          }
        }
      }
      break;

    case UCG_COM_MSG_REPEAT_3_BYTES:
      {
        spi_transaction_t trans;
        memset( &trans, 0, sizeof( trans ) );
        trans.tx_buffer = send_buffer;
        while (arg > 0) {
          size_t idx;
          for (idx = 0; (idx < SEND_BUFFER_SIZE) && (arg > 0); idx += 3, arg--) {
            send_buffer[idx]   = data[0];
            send_buffer[idx+1] = data[1];
            send_buffer[idx+2] = data[2];
          }
          trans.length = idx * 8;
          trans.rxlength = 0;
          if (idx < SPI_TRANSFER_POLLING_TRESHOLD) {
            spi_device_polling_transmit( hal->device, &trans );
          } else {
            spi_device_transmit( hal->device, &trans );
          }
        }
      }
      break;

    case UCG_COM_MSG_SEND_STR:
      {
        spi_transaction_t trans;
        memset( &trans, 0, sizeof( trans ) );
        trans.tx_buffer = send_buffer;
        while (arg > 0) {
          size_t len = arg > SEND_BUFFER_SIZE ? SEND_BUFFER_SIZE : arg;
          trans.length = len * 8;
          trans.rxlength = 0;
          memcpy( send_buffer, data, len );
          if (len < SPI_TRANSFER_POLLING_TRESHOLD) {
            spi_device_polling_transmit( hal->device, &trans );
          } else {
            spi_device_transmit( hal->device, &trans );
          }
          arg -= len;
        }
      }
      break;

    case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
      while (arg > 0) {
        if (*data != 0) {
          /* set the data line directly, ignore the setting from UCG_CFG_CD */
          if (*data == 1) {
            gpio_set_level( ucg->pin_list[UCG_PIN_CD], 0 );
          } else {
            gpio_set_level( ucg->pin_list[UCG_PIN_CD], 1 );
          }
        }
        data++;
        send_byte( hal, *data );
        data++;
        arg--;
      }
      break;
  }
  return 1;
}
