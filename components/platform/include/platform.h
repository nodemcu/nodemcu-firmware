#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "driver/uart.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "cpu_esp32.h"

#include "esp_task.h"


#define PLATFORM_ALIGNMENT __attribute__((aligned(4)))
#define PLATFORM_ALIGNMENT_PACKED __attribute__((aligned(4),packed))

// Error / status codes
enum
{
  PLATFORM_ERR,
  PLATFORM_OK,
  PLATFORM_UNDERFLOW = -1
};


#if CONFIG_NODEMCU_NODE_DEBUG
# define NODE_DBG printf
#else
# define NODE_DBG(...) do{}while(0)
#endif

#if CONFIG_NODEMCU_NODE_ERR
# define NODE_ERR printf
#else
# define NODE_ERR(...) do{}while(0)
#endif


int platform_init (void);

// *****************************************************************************
// GPIO subsection

int platform_gpio_exists( unsigned gpio );
int platform_gpio_output_exists( unsigned gpio );


// *****************************************************************************
// UART subsection

#define UART_BUFFER_SIZE    512

enum 
{
	PLATFORM_UART_MODE_UART = 0x0,
	PLATFORM_UART_MODE_RS485_COLLISION_DETECT = 0x1,
	PLATFORM_UART_MODE_RS485_APP_CONTROL = 0x2,
	PLATFORM_UART_MODE_HALF_DUPLEX = 0x3,
	PLATFORM_UART_MODE_IRDA = 0x4
};

// Parity
enum
{
  PLATFORM_UART_PARITY_NONE  = 0,
  PLATFORM_UART_PARITY_EVEN  = 1,
  PLATFORM_UART_PARITY_ODD   = 2,
  PLATFORM_UART_PARITY_MARK  = 3,
  PLATFORM_UART_PARITY_SPACE = 4
};


// Stop bits
enum
{
  PLATFORM_UART_STOPBITS_1   = 1,
  PLATFORM_UART_STOPBITS_2   = 2,
  PLATFORM_UART_STOPBITS_1_5 = 3
};

typedef struct {
  int tx_pin;
  int rx_pin;
  int rts_pin;
  int cts_pin;
  bool tx_inverse;
  bool rx_inverse;
  bool rts_inverse;
  bool cts_inverse;
  int flow_control;
} uart_pins_t;

typedef struct {
  QueueHandle_t queue;
  xTaskHandle taskHandle;
  int receive_rf;
  int error_rf;
  char *line_buffer;
  size_t line_position;
  uint16_t need_len;
  int16_t end_char;
} uart_status_t;

typedef struct {
  unsigned id;
  int type;
  size_t size;
  char* data;
} uart_event_post_t;

// Flow control types (this is a bit mask, one can specify PLATFORM_UART_FLOW_RTS | PLATFORM_UART_FLOW_CTS )
#define PLATFORM_UART_FLOW_NONE               0
#define PLATFORM_UART_FLOW_RTS                1
#define PLATFORM_UART_FLOW_CTS                2

// The platform UART functions
static inline int platform_uart_exists( unsigned id ) { return id < NUM_UART; }
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits, uart_pins_t* pins );
void platform_uart_setmode(unsigned id, unsigned mode);
void platform_uart_send_multi( unsigned id, const char *data, size_t len );
void platform_uart_send( unsigned id, uint8_t data );
void platform_uart_flush( unsigned id );
int platform_uart_start( unsigned id );
void platform_uart_stop( unsigned id );
int platform_uart_get_config(unsigned id, uint32_t *baudp, uint32_t *databitsp, uint32_t *parityp, uint32_t *stopbitsp);
int platform_uart_set_wakeup_threshold(unsigned id, unsigned threshold);


// *****************************************************************************
// Sigma-Delta subsection
int platform_sigma_delta_exists( unsigned channel );

uint8_t platform_sigma_delta_setup( uint8_t channel, uint8_t gpio_num );
uint8_t platform_sigma_delta_close( uint8_t channel );
// PWM emulation not possible, code kept for future reference
//uint8_t platform_sigma_delta_set_pwmduty( uint8_t channel, uint8_t duty );
uint8_t platform_sigma_delta_set_prescale( uint8_t channel, uint8_t prescale );
uint8_t platform_sigma_delta_set_duty( uint8_t channel, int8_t duty );

// *****************************************************************************
// ADC
int platform_adc_exists( uint8_t adc );
int platform_adc_channel_exists( uint8_t adc, uint8_t channel );
uint8_t platform_adc_set_width( uint8_t adc, int bits );
uint8_t platform_adc_setup( uint8_t adc, uint8_t channel, uint8_t attn );
int platform_adc_read( uint8_t adc, uint8_t channel );
int platform_adc_read_hall_sensor( );
enum {
    PLATFORM_ADC_ATTEN_0db   = 0,
    PLATFORM_ADC_ATTEN_2_5db = 1,
    PLATFORM_ADC_ATTEN_6db   = 2,
    PLATFORM_ADC_ATTEN_11db  = 3,
};

// *****************************************************************************
// I2C platform interface

// I2C speed
enum
{
  PLATFORM_I2C_SPEED_SLOW = 100000,
  PLATFORM_I2C_SPEED_FAST = 400000,
  PLATFORM_I2C_SPEED_FASTPLUS = 1000000
};

// I2C direction
enum
{
  PLATFORM_I2C_DIRECTION_TRANSMITTER,
  PLATFORM_I2C_DIRECTION_RECEIVER
};

int platform_i2c_exists( unsigned id );
int platform_i2c_setup( unsigned id, uint8_t sda, uint8_t scl, uint32_t speed );
int platform_i2c_send_start( unsigned id );
int platform_i2c_send_stop( unsigned id );
int platform_i2c_send_address( unsigned id, uint16_t address, int direction, int ack_check_en );
int platform_i2c_send_byte( unsigned id, uint8_t data, int ack_check_en );
// ack_val: 1 = send ACK, 0 = send NACK
int platform_i2c_recv_byte( unsigned id, int ack_val );


// *****************************************************************************
// Onewire platform interface

typedef struct {
  unsigned char ROM_NO[8];
  uint8_t LastDiscrepancy;
  uint8_t LastFamilyDiscrepancy;
  uint8_t LastDeviceFlag;
  uint8_t power;
} platform_onewire_bus_t;

int platform_onewire_init( uint8_t gpio_num );
int platform_onewire_reset( uint8_t gpio_num, uint8_t *presence );
int platform_onewire_write_bytes( uint8_t gpio_num, const uint8_t *buf, uint16_t count, bool power );
int platform_onewire_depower( uint8_t gpio_num );
int platform_onewire_read_bytes( uint8_t gpio_num, uint8_t *buf, uint16_t count );
int platform_onewire_depower( uint8_t gpio_num );
void platform_onewire_reset_search( platform_onewire_bus_t *bus );
void platform_onewire_target_search( uint8_t family_code, platform_onewire_bus_t *bus );
uint8_t platform_onewire_search( uint8_t pin, uint8_t *newAddr, platform_onewire_bus_t *bus );
uint8_t platform_onewire_crc8( const uint8_t *addr, uint8_t len );
uint8_t platform_onewire_crc8( const uint8_t *addr, uint8_t len );
bool platform_onewire_check_crc16( const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc );
uint16_t platform_onewire_crc16( const uint8_t* input, uint16_t len, uint16_t crc );

// *****************************************************************************
// DHT platform interface
#define PLATFORM_DHT11_WAKEUP_MS 20
#define PLATFORM_DHT2X_WAKEUP_MS 1

int platform_dht_read( uint8_t gpio_num, uint8_t wakeup_ms, uint8_t *data );


// *****************************************************************************
// WS2812 platform interface

void platform_ws2812_init( void );
int platform_ws2812_setup( uint8_t gpio_num, uint8_t num_mem, const uint8_t *data, size_t len );
int platform_ws2812_release( void );
int platform_ws2812_send( void );


// Internal flash erase/write functions

uint32_t platform_flash_get_sector_of_address( uint32_t addr );
uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_flash_get_num_sectors(void);
int platform_flash_erase_sector( uint32_t sector_id );


// Internal flash partitions
#define PLATFORM_PARTITION_TYPE_APP     0x00
#define PLATFORM_PARTITION_TYPE_DATA    0x01
#define PLATFORM_PARTITION_TYPE_NODEMCU 0xC2

#define PLATFORM_PARTITION_SUBTYPE_APP_FACTORY 0x00
#define PLATFORM_PARTITION_SUBTYPE_APP_OTA(n)  (0x10+n)
#define PLATFORM_PARTITION_SUBTYPE_APP_TEST    0x20

#define PLATFORM_PARTITION_SUBTYPE_DATA_OTA    0x00
#define PLATFORM_PARTITION_SUBTYPE_DATA_RF     0x01
#define PLATFORM_PARTITION_SUBTYPE_DATA_WIFI   0x02

#define PLATFORM_PARTITION_SUBTYPE_NODEMCU_SPIFFS 0x00
#define PLATFORM_PARTITION_SUBTYPE_NODEMCU_LFS    0x01

typedef struct {
  uint8_t  label[16];
  uint32_t offs;
  uint32_t size;
  uint8_t  type;
  uint8_t  subtype;
} platform_partition_t;

/**
 * Obtain partition information for the internal flash.
 * @param idx Which partition index to load info for.
 * @param info Buffer to store the info in.
 * @returns True if the partition info was loaded, false if not (e.g. no such
 *   partition idx).
 */
bool platform_partition_info (uint8_t idx, platform_partition_t *info);

/**
 * Appends a partition entry to the partition table, if possible.
 * Intended for auto-creation of a SPIFFS partition.
 * @param info The partition definition to append.
 * @returns True if the partition could be added, false if not.
 */
bool platform_partition_add (const platform_partition_t *info);


// *****************************************************************************
// Helper macros
#define MOD_CHECK_ID( mod, id )\
  if( !platform_ ## mod ## _exists( id ) )\
    return luaL_error( L, #mod" %d does not exist", ( unsigned )id )

#define MOD_CHECK_TIMER( id )\
  if( id == PLATFORM_TIMER_SYS_ID && !platform_timer_sys_available() )\
    return luaL_error( L, "the system timer is not available on this platform" );\
  if( !platform_tmr_exists( id ) )\
    return luaL_error( L, "timer %d does not exist", ( unsigned )id )\

#define MOD_CHECK_RES_ID( mod, id, resmod, resid )\
  if( !platform_ ## mod ## _check_ ## resmod ## _id( id, resid ) )\
    return luaL_error( L, #resmod" %d not valid with " #mod " %d", ( unsigned )resid, ( unsigned )id )



#endif
