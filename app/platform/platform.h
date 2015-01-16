// Platform-specific functions

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "cpu_esp8266.h"

#include "c_types.h"
#include "driver/pwm.h"
// Error / status codes
enum
{
  PLATFORM_ERR,
  PLATFORM_OK,
  PLATFORM_UNDERFLOW = -1
};

// Platform initialization
int platform_init(void);
void platform_int_init(void);

// ****************************************************************************
// KEY_LED functions
uint8_t platform_key_led( uint8_t level);

// *****************************************************************************
// GPIO subsection
#define PLATFORM_GPIO_FLOAT 0
#define PLATFORM_GPIO_PULLUP 1
#define PLATFORM_GPIO_PULLDOWN 2

#define PLATFORM_GPIO_INT 2
#define PLATFORM_GPIO_OUTPUT 1
#define PLATFORM_GPIO_INPUT 0

#define PLATFORM_GPIO_HIGH 1
#define PLATFORM_GPIO_LOW 0

/* GPIO interrupt handler */
typedef void (* platform_gpio_intr_handler_fn_t)( unsigned pin, unsigned level );

int platform_gpio_mode( unsigned pin, unsigned mode, unsigned pull );
int platform_gpio_write( unsigned pin, unsigned level );
int platform_gpio_read( unsigned pin );
void platform_gpio_init( platform_gpio_intr_handler_fn_t cb );
int platform_gpio_intr_init( unsigned pin, GPIO_INT_TYPE type );
// *****************************************************************************
// Timer subsection

// Timer data type
typedef uint32_t timer_data_type;

// *****************************************************************************
// CAN subsection

// Maximum length for any CAN message
#define PLATFORM_CAN_MAXLEN                   8

// eLua CAN ID types
enum
{
  ELUA_CAN_ID_STD = 0,
  ELUA_CAN_ID_EXT
};

int platform_can_exists( unsigned id );
uint32_t platform_can_setup( unsigned id, uint32_t clock );
int platform_can_send( unsigned id, uint32_t canid, uint8_t idtype, uint8_t len, const uint8_t *data );
int platform_can_recv( unsigned id, uint32_t *canid, uint8_t *idtype, uint8_t *len, uint8_t *data );

// *****************************************************************************
// SPI subsection

// There are 4 "virtual" SPI ports (SPI0...SPI3).
#define PLATFORM_SPI_TOTAL                    4
// TODO: PLATFORM_SPI_TOTAL is not used - figure out purpose, or remove?

// SPI mode
#define PLATFORM_SPI_MASTER                   1
#define PLATFORM_SPI_SLAVE                    0
// SS values
#define PLATFORM_SPI_SELECT_ON                1
#define PLATFORM_SPI_SELECT_OFF               0
// SPI enable/disable
#define PLATFORM_SPI_ENABLE                   1
#define PLATFORM_SPI_DISABLE                  0
// SPI clock phase
#define PLATFORM_SPI_CPHA_LOW                 0
#define PLATFORM_SPI_CPHA_HIGH                1
// SPI clock polarity
#define PLATFORM_SPI_CPOL_LOW                 0
#define PLATFORM_SPI_CPOL_HIGH                1
// SPI databits
#define PLATFORM_SPI_DATABITS_8               8
#define PLATFORM_SPI_DATABITS_16              16


// Data types
typedef uint32_t spi_data_type;

// The platform SPI functions
int platform_spi_exists( unsigned id );
uint32_t platform_spi_setup( unsigned id, int mode, unsigned cpol, unsigned cpha, unsigned databits, uint32_t clock);
spi_data_type platform_spi_send_recv( unsigned id, spi_data_type data );
void platform_spi_select( unsigned id, int is_select );

// *****************************************************************************
// UART subsection

// There are 3 "virtual" UART ports (UART0...UART2).
#define PLATFORM_UART_TOTAL                   3
// TODO: PLATFORM_UART_TOTAL is not used - figure out purpose, or remove?
// Note: Some CPUs (e.g. LM4F/TM4C) have more than 3 hardware UARTs

// Parity
enum
{
  PLATFORM_UART_PARITY_EVEN,
  PLATFORM_UART_PARITY_ODD,
  PLATFORM_UART_PARITY_NONE,
  PLATFORM_UART_PARITY_MARK,
  PLATFORM_UART_PARITY_SPACE
};

// Stop bits
enum
{
  PLATFORM_UART_STOPBITS_1,
  PLATFORM_UART_STOPBITS_1_5,
  PLATFORM_UART_STOPBITS_2
};

// Flow control types (this is a bit mask, one can specify PLATFORM_UART_FLOW_RTS | PLATFORM_UART_FLOW_CTS )
#define PLATFORM_UART_FLOW_NONE               0
#define PLATFORM_UART_FLOW_RTS                1
#define PLATFORM_UART_FLOW_CTS                2

// The platform UART functions
int platform_uart_exists( unsigned id );
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits );
int platform_uart_set_buffer( unsigned id, unsigned size );
void platform_uart_send( unsigned id, uint8_t data );
void platform_s_uart_send( unsigned id, uint8_t data );
int platform_uart_recv( unsigned id, unsigned timer_id, timer_data_type timeout );
int platform_s_uart_recv( unsigned id, timer_data_type timeout );
int platform_uart_set_flow_control( unsigned id, int type );
int platform_s_uart_set_flow_control( unsigned id, int type );

// *****************************************************************************
// PWM subsection

// There are 16 "virtual" PWM channels (PWM0...PWM15)
#define PLATFORM_PWM_TOTAL                    16
// TODO: PLATFORM_PWM_TOTAL is not used - figure out purpose, or remove?

#define NORMAL_PWM_DEPTH  PWM_DEPTH
#define NORMAL_DUTY(d) (((unsigned)(d)*NORMAL_PWM_DEPTH) / PWM_DEPTH)
#define DUTY(d) ((uint16_t)( ((unsigned)(d)*PWM_DEPTH) / NORMAL_PWM_DEPTH) )

// The platform PWM functions
int platform_pwm_exists( unsigned id );
uint32_t platform_pwm_setup( unsigned id, uint32_t frequency, unsigned duty );
void platform_pwm_close( unsigned id );
void platform_pwm_start( unsigned id );
void platform_pwm_stop( unsigned id );
uint32_t platform_pwm_set_clock( unsigned id, uint32_t data );
uint32_t platform_pwm_get_clock( unsigned id );
uint32_t platform_pwm_set_duty( unsigned id, uint32_t data );
uint32_t platform_pwm_get_duty( unsigned id );


// *****************************************************************************
// The platform ADC functions

// Functions requiring platform-specific implementation
int  platform_adc_update_sequence(void);
int  platform_adc_start_sequence(void);
void platform_adc_stop( unsigned id );
uint32_t  platform_adc_set_clock( unsigned id, uint32_t frequency);
int  platform_adc_check_timer_id( unsigned id, unsigned timer_id );

// ADC Common Functions
int  platform_adc_exists( unsigned id );
uint32_t  platform_adc_get_maxval( unsigned id );
uint32_t  platform_adc_set_smoothing( unsigned id, uint32_t length );
void platform_adc_set_blocking( unsigned id, uint32_t mode );
void platform_adc_set_freerunning( unsigned id, uint32_t mode );
uint32_t  platform_adc_is_done( unsigned id );
void platform_adc_set_timer( unsigned id, uint32_t timer );

// *****************************************************************************
// I2C platform interface

// I2C speed
enum
{
  PLATFORM_I2C_SPEED_SLOW = 100000,
  PLATFORM_I2C_SPEED_FAST = 400000
};

// I2C direction
enum
{
  PLATFORM_I2C_DIRECTION_TRANSMITTER,
  PLATFORM_I2C_DIRECTION_RECEIVER
};

int platform_i2c_exists( unsigned id );
uint32_t platform_i2c_setup( unsigned id, uint8_t sda, uint8_t scl, uint32_t speed );
void platform_i2c_send_start( unsigned id );
void platform_i2c_send_stop( unsigned id );
int platform_i2c_send_address( unsigned id, uint16_t address, int direction );
int platform_i2c_send_byte( unsigned id, uint8_t data );
int platform_i2c_recv_byte( unsigned id, int ack );

// *****************************************************************************
// Ethernet specific functions

void platform_eth_send_packet( const void* src, uint32_t size );
uint32_t platform_eth_get_packet_nb( void* buf, uint32_t maxlen );
void platform_eth_force_interrupt(void);
uint32_t platform_eth_get_elapsed_time(void);

// *****************************************************************************
// Internal flash erase/write functions

uint32_t platform_flash_get_first_free_block_address( uint32_t *psect );
uint32_t platform_flash_get_sector_of_address( uint32_t addr );
uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_flash_get_num_sectors(void);
int platform_flash_erase_sector( uint32_t sector_id );

// *****************************************************************************
// Allocator support

void* platform_get_first_free_ram( unsigned id );
void* platform_get_last_free_ram( unsigned id );

#endif
