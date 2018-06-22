// Platform-specific functions

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "cpu_esp8266.h"

#include "c_types.h"
#include "driver/pwm.h"
#include "driver/uart.h"
#include "task/task.h"

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

#define PLATFORM_GPIO_INT 2
#define PLATFORM_GPIO_OUTPUT 1
#define PLATFORM_GPIO_OPENDRAIN 3
#define PLATFORM_GPIO_INPUT 0

#define PLATFORM_GPIO_HIGH 1
#define PLATFORM_GPIO_LOW 0

typedef uint32_t (* platform_hook_function)(uint32_t bitmask);

static inline int platform_gpio_exists( unsigned pin ) { return pin < NUM_GPIO; }
int platform_gpio_mode( unsigned pin, unsigned mode, unsigned pull );
int platform_gpio_write( unsigned pin, unsigned level );
int platform_gpio_read( unsigned pin );

// Note that these functions will not be compiled in unless GPIO_INTERRUPT_ENABLE and
// GPIO_INTERRUPT_HOOK_ENABLE are defined.
int platform_gpio_register_intr_hook(uint32_t gpio_bits, platform_hook_function hook);
#define platform_gpio_unregister_intr_hook(hook) \
  platform_gpio_register_intr_hook(0, hook);
void platform_gpio_intr_init( unsigned pin, GPIO_INT_TYPE type );
void platform_gpio_init( task_handle_t gpio_task );
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

static inline int platform_can_exists( unsigned id ) { return NUM_CAN && (id < NUM_CAN); }
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


// Data types
typedef uint32_t spi_data_type;

// The platform SPI functions
static inline int platform_spi_exists( unsigned id ) { return id < NUM_SPI; }
uint32_t platform_spi_setup( uint8_t id, int mode, unsigned cpol, unsigned cpha, uint32_t clock_div);
int platform_spi_send( uint8_t id, uint8_t bitlen, spi_data_type data );
spi_data_type platform_spi_send_recv( uint8_t id, uint8_t bitlen, spi_data_type data );
void platform_spi_select( unsigned id, int is_select );

int platform_spi_blkwrite( uint8_t id, size_t len, const uint8_t *data );
int platform_spi_blkread( uint8_t id, size_t len, uint8_t *data );
int platform_spi_transaction( uint8_t id, uint8_t cmd_bitlen, spi_data_type cmd_data,
                              uint8_t addr_bitlen, spi_data_type addr_data,
                              uint16_t mosi_bitlen, uint8_t dummy_bitlen, int16_t miso_bitlen );


// *****************************************************************************
// UART subsection

// There are 3 "virtual" UART ports (UART0...UART2).
#define PLATFORM_UART_TOTAL                   3
// TODO: PLATFORM_UART_TOTAL is not used - figure out purpose, or remove?
// Note: Some CPUs (e.g. LM4F/TM4C) have more than 3 hardware UARTs

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

// Flow control types (this is a bit mask, one can specify PLATFORM_UART_FLOW_RTS | PLATFORM_UART_FLOW_CTS )
#define PLATFORM_UART_FLOW_NONE               0
#define PLATFORM_UART_FLOW_RTS                1
#define PLATFORM_UART_FLOW_CTS                2

// The platform UART functions
static inline int platform_uart_exists( unsigned id ) { return id < NUM_UART; }
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits );
int platform_uart_set_buffer( unsigned id, unsigned size );
void platform_uart_send( unsigned id, uint8_t data );
void platform_s_uart_send( unsigned id, uint8_t data );
int platform_uart_recv( unsigned id, unsigned timer_id, timer_data_type timeout );
int platform_s_uart_recv( unsigned id, timer_data_type timeout );
int platform_uart_set_flow_control( unsigned id, int type );
int platform_s_uart_set_flow_control( unsigned id, int type );
void platform_uart_alt( int set );
void platform_uart_get_config(unsigned id, uint32_t *baudp, uint32_t *databitsp, uint32_t *parityp, uint32_t *stopbitsp);

// *****************************************************************************
// PWM subsection

// There are 16 "virtual" PWM channels (PWM0...PWM15)
#define PLATFORM_PWM_TOTAL                    16
// TODO: PLATFORM_PWM_TOTAL is not used - figure out purpose, or remove?

#define NORMAL_PWM_DEPTH  PWM_DEPTH
#define NORMAL_DUTY(d) (((unsigned)(d)*NORMAL_PWM_DEPTH) / PWM_DEPTH)
#define DUTY(d) ((uint16_t)( ((unsigned)(d)*PWM_DEPTH) / NORMAL_PWM_DEPTH) )

// The platform PWM functions
void platform_pwm_init( void );
static inline int platform_pwm_exists( unsigned id ) { return ((id < NUM_PWM) && (id > 0)); }
uint32_t platform_pwm_setup( unsigned id, uint32_t frequency, unsigned duty );
void platform_pwm_close( unsigned id );
bool platform_pwm_start( unsigned id );
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
static inline int platform_adc_exists( unsigned id ) { return id < NUM_ADC; }
uint32_t  platform_adc_get_maxval( unsigned id );
uint32_t  platform_adc_set_smoothing( unsigned id, uint32_t length );
void platform_adc_set_blocking( unsigned id, uint32_t mode );
void platform_adc_set_freerunning( unsigned id, uint32_t mode );
uint32_t  platform_adc_is_done( unsigned id );
void platform_adc_set_timer( unsigned id, uint32_t timer );

// ****************************************************************************
// OneWire functions

static inline int platform_ow_exists( unsigned id ) { return ((id < NUM_OW) && (id > 0)); }

// ****************************************************************************
// Timer functions

static inline int platform_tmr_exists( unsigned id ) { return id < NUM_TMR; }

// *****************************************************************************
// Sigma-Delta platform interface

// ****************************************************************************
// Sigma-Delta functions

static inline int platform_sigma_delta_exists( unsigned id ) {return ((id < NUM_GPIO) && (id > 0)); }
uint8_t platform_sigma_delta_setup( uint8_t pin );
uint8_t platform_sigma_delta_close( uint8_t pin );
void platform_sigma_delta_set_pwmduty( uint8_t duty );
void platform_sigma_delta_set_prescale( uint8_t prescale );
void platform_sigma_delta_set_target( uint8_t target );

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

#ifdef NUM_I2C
static inline int platform_i2c_exists( unsigned id ) { return id < NUM_I2C; }
#else
static inline int platform_i2c_exists( unsigned id ) { return 0; }
#endif
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

/**
 * Translated a mapped address to a physical flash address, based on the
 * current flash cache mapping, and v.v.
 * @param mapped_addr Address to translate (>= INTERNAL_FLASH_MAPPED_ADDRESS)
 * @return the corresponding physical flash address, or -1 if flash cache is
 *  not currently active.
 * @see Cache_Read_Enable.
 */
uint32_t platform_flash_mapped2phys (uint32_t mapped_addr);
uint32_t platform_flash_phys2mapped (uint32_t phys_addr);

// *****************************************************************************
// Allocator support

void* platform_get_first_free_ram( unsigned id );
void* platform_get_last_free_ram( unsigned id );


// *****************************************************************************
// Other glue

int platform_ow_exists( unsigned id );
int platform_gpio_exists( unsigned id );
int platform_tmr_exists( unsigned id );

// *****************************************************************************

void* platform_print_deprecation_note( const char *msg, const char *time_frame);

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
