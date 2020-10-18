// Platform-dependent functions and includes

#include "platform.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "gpio.h"
#include "user_interface.h"
#include "driver/gpio16.h"
#include "driver/i2c_master.h"
#include "driver/spi.h"
#include "driver/uart.h"
#include "driver/sigma_delta.h"

#define INTERRUPT_TYPE_IS_LEVEL(x)   ((x) >= GPIO_PIN_INTR_LOLEVEL)

#ifdef GPIO_INTERRUPT_ENABLE
static platform_task_handle_t gpio_task_handle;
static int task_init_handler(void);

#ifdef GPIO_INTERRUPT_HOOK_ENABLE
struct gpio_hook_entry {
  platform_hook_function func;
  uint32_t               bits;
};
struct gpio_hook {
  uint32_t                all_bits;
  uint32_t                count;
  struct gpio_hook_entry  entry[1];
};

static struct gpio_hook *platform_gpio_hook;
#endif
#endif

static const int uart_bitrates[] = {
    BIT_RATE_300,
    BIT_RATE_600,
    BIT_RATE_1200,
    BIT_RATE_2400,
    BIT_RATE_4800,
    BIT_RATE_9600,
    BIT_RATE_19200,
    BIT_RATE_31250,
    BIT_RATE_38400,
    BIT_RATE_57600,
    BIT_RATE_74880,
    BIT_RATE_115200,
    BIT_RATE_230400,
    BIT_RATE_256000,
    BIT_RATE_460800,
    BIT_RATE_921600,
    BIT_RATE_1843200,
    BIT_RATE_3686400
};

int platform_init ()
{
  // Setup the various forward and reverse mappings for the pins
  get_pin_map();

  (void) task_init_handler();

  cmn_platform_init();
  // All done
  return PLATFORM_OK;
}

// ****************************************************************************
// KEY_LED functions
uint8_t platform_key_led( uint8_t level){
  uint8_t temp;
  gpio16_output_set(1);   // set to high first, for reading key low level
  gpio16_input_conf();
  temp = gpio16_input_get();
  gpio16_output_conf();
  gpio16_output_set(level);
  return temp;
}

// ****************************************************************************
// GPIO functions

/*
 * Set GPIO mode to output. Optionally in RAM helper because interrupts are dsabled
 */
static void NO_INTR_CODE set_gpio_no_interrupt(uint8_t pin, uint8_t push_pull) {
  unsigned pnum = pin_num[pin];
  ETS_GPIO_INTR_DISABLE();
#ifdef GPIO_INTERRUPT_ENABLE
  pin_int_type[pin] = GPIO_PIN_INTR_DISABLE;
#endif
  PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
  //disable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(pnum), GPIO_PIN_INTR_DISABLE);
  //clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pnum));

  // configure push-pull vs open-drain
  if (push_pull) {
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pnum)),
                   GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pnum))) &
                   (~ GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)));  //disable open drain;
  } else {
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pnum)),
                   GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pnum))) |
                   GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE));      //enable open drain;
  }

  ETS_GPIO_INTR_ENABLE();
}

/*
 * Set GPIO mode to interrupt. Optionally RAM helper because interrupts are dsabled
 */
#ifdef GPIO_INTERRUPT_ENABLE
static void NO_INTR_CODE set_gpio_interrupt(uint8_t pin) {
  ETS_GPIO_INTR_DISABLE();
  PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
  GPIO_DIS_OUTPUT(pin_num[pin]);
  gpio_register_set(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])),
                    GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                    | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                    | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
  ETS_GPIO_INTR_ENABLE();
}
#endif

int platform_gpio_mode( unsigned pin, unsigned mode, unsigned pull )
{
  NODE_DBG("Function platform_gpio_mode() is called. pin_mux:%d, func:%d\n", pin_mux[pin], pin_func[pin]);
  if (pin >= NUM_GPIO)
    return -1;

  if(pin == 0){
    if(mode==PLATFORM_GPIO_INPUT)
      gpio16_input_conf();
    else
      gpio16_output_conf();

    return 1;
  }

#ifdef LUA_USE_MODULES_PWM
  platform_pwm_close(pin);    // closed from pwm module, if it is used in pwm
#endif

  if (pull == PLATFORM_GPIO_PULLUP) {
    PIN_PULLUP_EN(pin_mux[pin]);
  } else {
    PIN_PULLUP_DIS(pin_mux[pin]);
  }

  switch(mode){

    case PLATFORM_GPIO_INPUT:
      GPIO_DIS_OUTPUT(pin_num[pin]);
      set_gpio_no_interrupt(pin, TRUE);
      break;
    case PLATFORM_GPIO_OUTPUT:
      set_gpio_no_interrupt(pin, TRUE);
      GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, BIT(pin_num[pin]));
      break;
    case PLATFORM_GPIO_OPENDRAIN:
      set_gpio_no_interrupt(pin, FALSE);
      GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, BIT(pin_num[pin]));
      break;

#ifdef GPIO_INTERRUPT_ENABLE
    case PLATFORM_GPIO_INT:
      set_gpio_interrupt(pin);
      break;
#endif

    default:
      break;
  }
  return 1;
}


int platform_gpio_write( unsigned pin, unsigned level )
{
  // NODE_DBG("Function platform_gpio_write() is called. pin:%d, level:%d\n",GPIO_ID_PIN(pin_num[pin]),level);
  if (pin >= NUM_GPIO)
    return -1;
  if(pin == 0){
    gpio16_output_conf();
    gpio16_output_set(level);
    return 1;
  }

  GPIO_OUTPUT_SET(GPIO_ID_PIN(pin_num[pin]), level);
}

int platform_gpio_read( unsigned pin )
{
  // NODE_DBG("Function platform_gpio_read() is called. pin:%d\n",GPIO_ID_PIN(pin_num[pin]));
  if (pin >= NUM_GPIO)
    return -1;

  if(pin == 0){
    // gpio16_input_conf();
    return 0x1 & gpio16_input_get();
  }

  // GPIO_DIS_OUTPUT(pin_num[pin]);
  return 0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[pin]));
}

#ifdef GPIO_INTERRUPT_ENABLE
static void ICACHE_RAM_ATTR platform_gpio_intr_dispatcher (void *dummy){
  uint32_t j=0;
  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  uint32_t now = system_get_time();
  (void)(dummy);

#ifdef GPIO_INTERRUPT_HOOK_ENABLE
  if (gpio_status & platform_gpio_hook->all_bits) {
    for (j = 0; j < platform_gpio_hook->count; j++) {
       if (gpio_status & platform_gpio_hook->entry[j].bits)
         gpio_status = (platform_gpio_hook->entry[j].func)(gpio_status);
    }
  }
#endif
  /*
   * gpio_status is a bit map where bit 0 is set if unmapped gpio pin 0 (pin3) has
   * triggered the ISR. bit 1 if unmapped gpio pin 1 (pin10=U0TXD), etc.  Since this
   * is the ISR, it makes sense to optimize this by doing a fast scan of the status
   * and reverse mapping any set bits.
   */
   for (j = 0; gpio_status>0; j++, gpio_status >>= 1) {
    if (gpio_status&1) {
      int i = pin_num_inv[j];
      if (pin_int_type[i]) {
        uint16_t diff = pin_counter[i].seen ^ pin_counter[i].reported;

        pin_counter[i].seen = 0x7fff & (pin_counter[i].seen + 1);

        if (INTERRUPT_TYPE_IS_LEVEL(pin_int_type[i])) {
          //disable interrupt
          gpio_pin_intr_state_set(GPIO_ID_PIN(j), GPIO_PIN_INTR_DISABLE);
        }
        //clear interrupt status
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(j));

        if (diff == 0 || diff & 0x8000) {
          uint32_t level = 0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(j));
	  if (!platform_post_high (gpio_task_handle, (now << 8) + (i<<1) + level)) {
            // If we fail to post, then try on the next interrupt
            pin_counter[i].seen |= 0x8000;
          }
          // We re-enable the interrupt when we execute the callback (if level)
        }
      } else {
        // this is an unexpected interrupt so shut it off for now
        gpio_pin_intr_state_set(GPIO_ID_PIN(j), GPIO_PIN_INTR_DISABLE);
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(j));
      }
    }
  }
}

void platform_gpio_init( platform_task_handle_t gpio_task )
{
  gpio_task_handle = gpio_task;
  // No error handling but this is called at startup when there is a lot of free RAM
  platform_gpio_hook =  calloc (1, sizeof(*platform_gpio_hook) - sizeof(struct gpio_hook_entry));
  ETS_GPIO_INTR_ATTACH(platform_gpio_intr_dispatcher, NULL);
}

#ifdef GPIO_INTERRUPT_HOOK_ENABLE
/*
 * Register an ISR hook to be called from the GPIO ISR for a given GPIO bitmask.
 * This routine is only called a few times so has been optimised for size and
 * the unregister is a special case when the bits are 0.
 *
 * Each hook function can only be registered once. If it is re-registered
 * then the hooked bits are just updated to the new value.
 */
int platform_gpio_register_intr_hook(uint32_t bits, platform_hook_function hook)
{
  struct gpio_hook *oh = platform_gpio_hook;
  int i, j, cur = -1;

  if (!hook)   // Cannot register or unregister null hook
    return 0;

  // Is the hook already registered?
  for (i=0; i<oh->count; i++) {
    if (hook == oh->entry[i].func) {
      cur = i;
      break;
    }
  }

  // return error status if there is a bits clash
  if (oh->all_bits & ~(cur < 0 ? 0 : oh->entry[cur].bits) & bits)
    return 0;

  // Allocate replacement hook block and return 0 on alloc failure
  int count = oh->count + (cur < 0 ? 1 : (bits == 0 ? -1 : 0)); 
  struct gpio_hook *nh = malloc (sizeof *oh + (count -1)*sizeof(struct gpio_hook_entry));
  if (!oh)
    return 0;

  nh->all_bits = 0;
  nh->count = count;

  for (i=0, j=0; i<oh->count; i++) {
    if (i == cur && !bits) 
      continue;  /* unregister entry is a no-op */
    nh->entry[j] = oh->entry[i];  /* copy existing entry */
    if (i == cur)
      nh->entry[j].bits = bits;   /* update bits if this is a replacement */
    nh->all_bits |= nh->entry[j++].bits;
  }
 
  if (cur < 0) {                  /* append new hook entry */
    nh->entry[j].func = hook;
    nh->entry[j].bits = bits;
    nh->all_bits |= bits;
  } 
       
  ETS_GPIO_INTR_DISABLE();
  platform_gpio_hook = nh;
  ETS_GPIO_INTR_ENABLE();

  free(oh);
  return 1;
}
#endif // GPIO_INTERRUPT_HOOK_ENABLE

/*
 * Initialise GPIO interrupt mode. Optionally in RAM because interrupts are disabled
 */
void NO_INTR_CODE platform_gpio_intr_init( unsigned pin, GPIO_INT_TYPE type )
{
  if (platform_gpio_exists(pin)) {
    ETS_GPIO_INTR_DISABLE();
    //clear interrupt status
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin_num[pin]));
    pin_int_type[pin] = type;
    //enable interrupt
    gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), type);
    ETS_GPIO_INTR_ENABLE();
  }
}
#endif

// ****************************************************************************
// UART
// TODO: Support timeouts.

// UartDev is defined and initialized in rom code.
extern UartDevice UartDev;
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits )
{
  switch( baud )
  {
    case BIT_RATE_300:
    case BIT_RATE_600:
    case BIT_RATE_1200:
    case BIT_RATE_2400:
    case BIT_RATE_4800:
    case BIT_RATE_9600:
    case BIT_RATE_19200:
    case BIT_RATE_31250:
    case BIT_RATE_38400:
    case BIT_RATE_57600:
    case BIT_RATE_74880:
    case BIT_RATE_115200:
    case BIT_RATE_230400:
    case BIT_RATE_256000:
    case BIT_RATE_460800:
    case BIT_RATE_921600:
    case BIT_RATE_1843200:
    case BIT_RATE_3686400:
      UartDev.baut_rate = baud;
      break;
    default:
      UartDev.baut_rate = BIT_RATE_9600;
      break;
  }

  switch( databits )
  {
    case 5:
      UartDev.data_bits = FIVE_BITS;
      break;
    case 6:
      UartDev.data_bits = SIX_BITS;
      break;
    case 7:
      UartDev.data_bits = SEVEN_BITS;
      break;
    case 8:
      UartDev.data_bits = EIGHT_BITS;
      break;
    default:
      UartDev.data_bits = EIGHT_BITS;
      break;
  }

  switch (stopbits)
  {
    case PLATFORM_UART_STOPBITS_1_5:
      UartDev.stop_bits = ONE_HALF_STOP_BIT;
      break;
    case PLATFORM_UART_STOPBITS_2:
      UartDev.stop_bits = TWO_STOP_BIT;
      break;
    default:
      UartDev.stop_bits = ONE_STOP_BIT;
      break;
  }

  switch (parity)
  {
    case PLATFORM_UART_PARITY_EVEN:
      UartDev.parity = EVEN_BITS;
      UartDev.exist_parity = STICK_PARITY_EN;
      break;
    case PLATFORM_UART_PARITY_ODD:
      UartDev.parity = ODD_BITS;
      UartDev.exist_parity = STICK_PARITY_EN;
      break;
    default:
      UartDev.parity = NONE_BITS;
      UartDev.exist_parity = STICK_PARITY_DIS;
      break;
  }

  uart_setup(id);

  return baud;
}

void platform_uart_get_config(unsigned id, uint32_t *baudp, uint32_t *databitsp, uint32_t *parityp, uint32_t *stopbitsp) {
  UartConfig config =  uart_get_config(id);
  int i;

  int offset = config.baut_rate;

  for (i = 0; i < sizeof(uart_bitrates) / sizeof(uart_bitrates[0]); i++) {
    int diff = config.baut_rate - uart_bitrates[i];

    if (diff < 0) {
      diff = -diff;
    }

    if (diff < offset) {
       offset = diff;
       *baudp = uart_bitrates[i];
    }
  }

  switch( config.data_bits )
  {
    case FIVE_BITS:
      *databitsp = 5;
      break;
    case SIX_BITS:
      *databitsp = 6;
      break;
    case SEVEN_BITS:
      *databitsp = 7;
      break;
    case EIGHT_BITS:
    default:
      *databitsp = 8;
      break;
  }

  switch (config.stop_bits)
  {
    case ONE_HALF_STOP_BIT:
      *stopbitsp = PLATFORM_UART_STOPBITS_1_5;
      break;
    case TWO_STOP_BIT:
      *stopbitsp = PLATFORM_UART_STOPBITS_2;
      break;
    default:
      *stopbitsp = PLATFORM_UART_STOPBITS_1;
      break;
  }

  if (config.exist_parity == STICK_PARITY_DIS) {
    *parityp = PLATFORM_UART_PARITY_NONE;
  } else if (config.parity == EVEN_BITS) {
    *parityp = PLATFORM_UART_PARITY_EVEN;
  } else {
    *parityp = PLATFORM_UART_PARITY_ODD;
  }
}

// if set=1, then alternate serial output pins are used. (15=rx, 13=tx)
void platform_uart_alt( int set )
{
    uart0_alt( set );
    return;
}


// Send: version with and without mux
void platform_uart_send( unsigned id, u8 data )
{
  uart_tx_one_char(id, data);
}

// ****************************************************************************
// PWMs

static uint16_t pwms_duty[NUM_PWM] = {0};

void platform_pwm_init()
{
  int i;
  for(i=0;i<NUM_PWM;i++){
    pwms_duty[i] = DUTY(0);
  }
  pwm_init(500, NULL);
  // NODE_DBG("Function pwms_init() is called.\n");
}

// Return the PWM clock
// NOTE: Can't find a function to query for the period set for the timer,
// therefore using the struct.
// This may require adjustment if driver libraries are updated.
uint32_t platform_pwm_get_clock( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_get_clock() is called.\n");
  if( pin >= NUM_PWM)
    return 0;
  if(!pwm_exist(pin))
    return 0;

  return (uint32_t)pwm_get_freq(pin);
}

// Set the PWM clock
uint32_t platform_pwm_set_clock( unsigned pin, uint32_t clock )
{
  // NODE_DBG("Function platform_pwm_set_clock() is called.\n");
  if( pin >= NUM_PWM)
    return 0;
  if(!pwm_exist(pin))
    return 0;

  pwm_set_freq((uint16_t)clock, pin);
  pwm_start();
  return (uint32_t)pwm_get_freq( pin );
}

uint32_t platform_pwm_get_duty( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_get_duty() is called.\n");
  if( pin < NUM_PWM){
    if(!pwm_exist(pin))
      return 0;
    // return NORMAL_DUTY(pwm_get_duty(pin));
    return pwms_duty[pin];
  }
  return 0;
}

// Set the PWM duty
uint32_t platform_pwm_set_duty( unsigned pin, uint32_t duty )
{
  // NODE_DBG("Function platform_pwm_set_duty() is called.\n");
  if ( pin < NUM_PWM)
  {
    if(!pwm_exist(pin))
      return 0;
    pwm_set_duty(DUTY(duty), pin);
  } else {
    return 0;
  }
  pwm_start();
  pwms_duty[pin] = NORMAL_DUTY(pwm_get_duty(pin));
  return pwms_duty[pin];
}

uint32_t platform_pwm_setup( unsigned pin, uint32_t frequency, unsigned duty )
{
  uint32_t clock;
  if ( pin < NUM_PWM)
  {
    platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);  // disable gpio interrupt first
    if(!pwm_add(pin))
      return 0;
    // pwm_set_duty(DUTY(duty), pin);
    pwm_set_duty(0, pin);
    pwms_duty[pin] = duty;
    pwm_set_freq((uint16_t)frequency, pin);
  } else {
    return 0;
  }
  clock = platform_pwm_get_clock( pin );
  if (!pwm_start()) {
    return 0;
  }
  return clock;
}

void platform_pwm_close( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_stop() is called.\n");
  if ( pin < NUM_PWM)
  {
    pwm_delete(pin);
    pwm_start();
  }
}

bool platform_pwm_start( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_start() is called.\n");
  if ( pin < NUM_PWM)
  {
    if(!pwm_exist(pin))
      return FALSE;
    pwm_set_duty(DUTY(pwms_duty[pin]), pin);
    return pwm_start();
  }

  return FALSE;
}

void platform_pwm_stop( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_stop() is called.\n");
  if ( pin < NUM_PWM)
  {
    if(!pwm_exist(pin))
      return;
    pwm_set_duty(0, pin);
    pwm_start();
  }
}


// *****************************************************************************
// Sigma-Delta platform interface

uint8_t platform_sigma_delta_setup( uint8_t pin )
{
  if (pin < 1 || pin > NUM_GPIO)
    return 0;

  sigma_delta_setup();

  // set GPIO output mode for this pin
  platform_gpio_mode( pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );
  platform_gpio_write( pin, PLATFORM_GPIO_LOW );

  // enable sigma-delta on this pin
  GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])),
                 (GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin]))) &(~GPIO_PIN_SOURCE_MASK)) |
                 GPIO_PIN_SOURCE_SET( SIGMA_AS_PIN_SOURCE ));

  return 1;
}

uint8_t platform_sigma_delta_close( uint8_t pin )
{
  if (pin < 1 || pin > NUM_GPIO)
    return 0;

  sigma_delta_stop();

  // set GPIO input mode for this pin
  platform_gpio_mode( pin, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP );

  // CONNECT GPIO TO PIN PAD
  GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])),
                 (GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin]))) &(~GPIO_PIN_SOURCE_MASK)) |
                 GPIO_PIN_SOURCE_SET( GPIO_AS_PIN_SOURCE ));

  return 1;
}

void platform_sigma_delta_set_pwmduty( uint8_t duty )
{
  uint8_t target = 0, prescale = 0;

  target = duty > 128 ? 256 - duty : duty;
  prescale = target == 0 ? 0 : target-1;

  //freq = 80000 (khz) /256 /duty_target * (prescale+1)
  sigma_delta_set_prescale_target( prescale, duty );
}

void platform_sigma_delta_set_prescale( uint8_t prescale )
{
  sigma_delta_set_prescale_target( prescale, -1 );
}

void ICACHE_RAM_ATTR platform_sigma_delta_set_target( uint8_t target )
{
    sigma_delta_set_prescale_target( -1, target );
}


// *****************************************************************************
// I2C platform interface

uint32_t platform_i2c_setup( unsigned id, uint8_t sda, uint8_t scl, uint32_t speed ){
  if (sda >= NUM_GPIO || scl >= NUM_GPIO)
    return 0;

  // platform_pwm_close(sda);
  // platform_pwm_close(scl);

  // disable gpio interrupt first
  platform_gpio_mode(sda, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);   // inside this func call platform_pwm_close
  platform_gpio_mode(scl, PLATFORM_GPIO_INPUT, PLATFORM_GPIO_PULLUP);    // disable gpio interrupt first

  return i2c_master_setup(id, sda, scl, speed);
}

bool platform_i2c_configured( unsigned id ){
  return i2c_master_configured(id);
}

void platform_i2c_send_start( unsigned id ){
  i2c_master_start(id);
}

void platform_i2c_send_stop( unsigned id ){
  i2c_master_stop(id);
}

int platform_i2c_send_address( unsigned id, uint16_t address, int direction ){
  // Convert enum codes to R/w bit value.
  // If TX == 0 and RX == 1, this test will be removed by the compiler
  if ( ! ( PLATFORM_I2C_DIRECTION_TRANSMITTER == 0 &&
           PLATFORM_I2C_DIRECTION_RECEIVER == 1 ) ) {
    direction = ( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ) ? 0 : 1;
  }
  return i2c_master_writeByte(id,
    (uint8_t) ((address << 1) + (direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ? 0 : 1))
  );
}

int platform_i2c_send_byte(unsigned id, uint8_t data ){
  return i2c_master_writeByte(id, data);
}

int platform_i2c_recv_byte( unsigned id, int ack ){
  return i2c_master_readByte(id, ack);
}

// *****************************************************************************
// SPI platform interface
uint32_t platform_spi_setup( uint8_t id, int mode, unsigned cpol, unsigned cpha, uint32_t clock_div )
{
  spi_master_init( id, cpol, cpha, clock_div );
  // all platform functions assume LSB order for MOSI & MISO buffer
  spi_mast_byte_order( id, SPI_ORDER_LSB );
  return 1;
}

int platform_spi_send( uint8_t id, uint8_t bitlen, spi_data_type data )
{
  if (bitlen > 32)
    return PLATFORM_ERR;

  spi_mast_transaction( id, 0, 0, bitlen, data, 0, 0, 0 );
  return PLATFORM_OK;
}

spi_data_type platform_spi_send_recv( uint8_t id, uint8_t bitlen, spi_data_type data )
{
  if (bitlen > 32)
    return 0;

  spi_mast_set_mosi( id, 0, bitlen, data );
  spi_mast_transaction( id, 0, 0, 0, 0, bitlen, 0, -1 );
  return spi_mast_get_miso( id, 0, bitlen );
}

int platform_spi_blkwrite( uint8_t id, size_t len, const uint8_t *data )
{
  while (len > 0) {
    size_t chunk_len = len > 64 ? 64 : len;

    spi_mast_blkset( id, chunk_len * 8, data );
    spi_mast_transaction( id, 0, 0, 0, 0, chunk_len * 8, 0, 0 );

    data = &(data[chunk_len]);
    len -= chunk_len;
  }

  return PLATFORM_OK;
}

int platform_spi_blkread( uint8_t id, size_t len, uint8_t *data )
{
  uint8_t mosi_idle[64];

  os_memset( (void *)mosi_idle, 0xff, len > 64 ? 64 : len );

  while (len > 0 ) {
    size_t chunk_len = len > 64 ? 64 : len;

    spi_mast_blkset( id, chunk_len * 8, mosi_idle );
    spi_mast_transaction( id, 0, 0, 0, 0, chunk_len * 8, 0, -1 );
    spi_mast_blkget( id, chunk_len * 8, data );

    data = &(data[chunk_len]);
    len -= chunk_len;
  }

  return PLATFORM_OK;
}

int platform_spi_transaction( uint8_t id, uint8_t cmd_bitlen, spi_data_type cmd_data,
                              uint8_t addr_bitlen, spi_data_type addr_data,
                              uint16_t mosi_bitlen, uint8_t dummy_bitlen, int16_t miso_bitlen )
{
  if ((cmd_bitlen   >  16) ||
      (addr_bitlen  >  32) ||
      (mosi_bitlen  > 512) ||
      (dummy_bitlen > 256) ||
      (miso_bitlen  > 512))
    return PLATFORM_ERR;

  spi_mast_transaction( id, cmd_bitlen, cmd_data, addr_bitlen, addr_data, mosi_bitlen, dummy_bitlen, miso_bitlen );

  return PLATFORM_OK;
}

// ****************************************************************************
// Flash access functions

/*
 * Assumptions:
 * > toaddr is INTERNAL_FLASH_WRITE_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_WRITE_UNIT_SIZE
 */
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
  SpiFlashOpResult r;
  const uint32_t blkmask = INTERNAL_FLASH_WRITE_UNIT_SIZE - 1;
  uint32_t *apbuf = NULL;
  uint32_t fromaddr = (uint32_t)from;
  if( (fromaddr & blkmask ) || (fromaddr >= INTERNAL_FLASH_MAPPED_ADDRESS)) {
    apbuf = (uint32_t *)malloc(size);
    if(!apbuf)
      return 0;
    memcpy(apbuf, from, size);
  }
  system_soft_wdt_feed ();
  r = flash_write(toaddr, apbuf?(uint32_t *)apbuf:(uint32_t *)from, size);
  if(apbuf)
    free(apbuf);
  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_write: r=%d at %p\n", r, toaddr);
    return 0;
  }
}

/*
 * Assumptions:
 * > fromaddr is INTERNAL_FLASH_READ_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_READ_UNIT_SIZE
 */
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
  if (size==0)
    return 0;

  SpiFlashOpResult r;
  system_soft_wdt_feed ();

  const uint32_t blkmask = (INTERNAL_FLASH_READ_UNIT_SIZE - 1);
  if( ((uint32_t)to) & blkmask )
  {
    uint32_t size2=size-INTERNAL_FLASH_READ_UNIT_SIZE;
    uint32_t* to2=(uint32_t*)((((uint32_t)to)&(~blkmask))+INTERNAL_FLASH_READ_UNIT_SIZE);
    r = flash_read(fromaddr, to2, size2);
    if(SPI_FLASH_RESULT_OK == r)
    {
      memmove(to,to2,size2);  // This is overlapped so must be memmove and not memcpy
      char back[ INTERNAL_FLASH_READ_UNIT_SIZE ] __attribute__ ((aligned(INTERNAL_FLASH_READ_UNIT_SIZE)));
      r=flash_read(fromaddr+size2,(uint32*)back,INTERNAL_FLASH_READ_UNIT_SIZE);
      memcpy((uint8_t*)to+size2,back,INTERNAL_FLASH_READ_UNIT_SIZE);
    }
  }
  else
    r = flash_read(fromaddr, (uint32_t *)to, size);

  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_read: r=%d at %p\n", r, fromaddr);
    return 0;
  }
}

int platform_flash_erase_sector( uint32_t sector_id )
{
  NODE_DBG( "flash_erase_sector(%u)\n", sector_id);
  return flash_erase( sector_id ) == SPI_FLASH_RESULT_OK ? PLATFORM_OK : PLATFORM_ERR;
}

static uint32_t flash_map_meg_offset (void) {
  uint32_t cache_ctrl = READ_PERI_REG(CACHE_FLASH_CTRL_REG);
  if (!(cache_ctrl & CACHE_FLASH_ACTIVE))
    return -1;
  uint32_t m0 = (cache_ctrl & CACHE_FLASH_MAPPED0) ? 0x100000 : 0;
  uint32_t m1 = (cache_ctrl & CACHE_FLASH_MAPPED1) ? 0x200000 : 0;
  return m0 + m1;
}

uint32_t platform_flash_mapped2phys (uint32_t mapped_addr) {
  uint32_t meg = flash_map_meg_offset();
  return (meg&1) ? -1 : mapped_addr - INTERNAL_FLASH_MAPPED_ADDRESS + meg ;
}

uint32_t platform_flash_phys2mapped (uint32_t phys_addr) {
  uint32_t meg = flash_map_meg_offset();
  return (meg&1) ? -1 : phys_addr + INTERNAL_FLASH_MAPPED_ADDRESS - meg;
}

uint32_t platform_flash_get_partition (uint32_t part_id, uint32_t *addr) {
  partition_item_t pt = {0,0,0};
  system_partition_get_item(SYSTEM_PARTITION_CUSTOMER_BEGIN + part_id, &pt);
  if (addr) {
    *addr = pt.addr;
  }
  return  pt.type == 0 ? 0 : pt.size;
}
/*
 * The Reboot Config Records are stored in the 4K flash page at offset 0x10000 (in
 * the linker section .irom0.ptable) and is used for configuration changes that
 * persist across reboots. This page contains a sequence of records, each of which
 * is word-aligned and comprises a header and body of length 0-64 words.  The 4-byte
 * header comprises a length, a RCR id, and two zero fill bytes.  These are written
 * using flash NAND writing rules, so any unused area (all 0xFF) can be overwritten
 * by a new record without needing to erase the RCR page.  Ditto any existing
 * record can be marked as deleted by over-writing the header with the id set to
 * PLATFORM_RCR_DELETED (0x0).  Note that the last word is not used additions so a
 * scan for PLATFORM_RCR_FREE will always terminate.
 *
 * The number of updates is extremely low, so it is unlikely (but possible) that
 * the page might fill with the churn of new RCRs, so in this case the write function
 * compacts the page by eliminating all deleted records.  This does require a flash
 * sector erase.
 *
 * NOTE THAT THIS ALGO ISN'T 100% ROBUST, eg. a powerfail between the erase and the
 * wite-back will leave the page unitialised; ditto a powerfail between the record
 * appned and old deletion will leave two records.  However this is better than the
 * general integrity of SPIFFS, for example and the vulnerable window is typically
 * less than 1 mSec every configuration change.
 */
extern uint32_t _irom0_text_start[];
#define RCR_WORD(i) (_irom0_text_start[i])
#define WORDSIZE sizeof(uint32_t)
#define FLASH_SECTOR_WORDS (INTERNAL_FLASH_SECTOR_SIZE/WORDSIZE)

uint32_t platform_rcr_read (uint8_t rec_id, void **rec) {
    platform_rcr_t *rcr = (platform_rcr_t *) &RCR_WORD(0);
    uint32_t i = 0;
   /*
    * Chain down the RCR page looking for a record that matches the record
    * ID. If found return the size of the record and optionally its address.
    */
   while (1) {
        // copy RCR header into RAM to avoid unaligned exceptions
        platform_rcr_t r = (platform_rcr_t) RCR_WORD(i);
        if (r.id == rec_id) {
            if (rec) *rec = &RCR_WORD(i+1);
            return r.len * WORDSIZE;
        } else if (r.id == PLATFORM_RCR_FREE) {
            break;
        }
        i += 1 + r.len;
    }
    return ~0;
}

uint32_t platform_rcr_get_startup_option() {
  static uint32_t option = ~0;
  uint32_t *option_p;

  if (option == ~0) {
    option = 0;

    if (platform_rcr_read(PLATFORM_RCR_STARTUP_OPTION, (void **) &option_p) == sizeof(*option_p)) {
      option = *option_p;
    }
  }
  return option;
}

uint32_t platform_rcr_delete (uint8_t rec_id) {
  uint32_t *rec = NULL;
  platform_rcr_read(rec_id, (void**)&rec);
  if (rec) {
    uint32_t *pHdr = rec - 1;  /* the header is the word proceeding the rec */ 
    platform_rcr_t hdr = {.hdr = *pHdr};
    hdr.id = PLATFORM_RCR_DELETED;
    platform_s_flash_write(&hdr, platform_flash_mapped2phys((uint32_t) pHdr), WORDSIZE);
    return 0;
  }
  return ~0;
}

/*
 * Chain down the RCR page and look for an existing record that matches the record
 * ID and the first free record.  If there is enough room, then append the new
 * record and mark any previous record as deleted.  If the page is full then GC,
 * erase the page and rewrite with the GCed content.
 */
#define MAXREC 65
uint32_t platform_rcr_write (uint8_t rec_id, const void *inrec, uint8_t n) {
    uint32_t nwords = (n+WORDSIZE-1) / WORDSIZE;
    uint32_t reclen = (nwords+1)*WORDSIZE;
    uint32_t *prev=NULL, *new = NULL;

    // make local stack copy of inrec including header and any trailing fill bytes
    uint32_t rec[MAXREC];
    if (nwords >= MAXREC)
        return ~0;
    rec[0] = 0; rec[nwords] = 0;
    ((platform_rcr_t *) rec)->id = rec_id;
    ((platform_rcr_t *) rec)->len = nwords;
    memcpy(rec+1, inrec, n);  // let memcpy handle 0 and odd byte cases

    // find previous copy if any and exit if the replacement is the same value
    uint8_t np = platform_rcr_read (rec_id, (void **) &prev);
    if (prev && !os_memcmp(prev-1, rec, reclen))
        return n;

    // find next free slot
    platform_rcr_read (PLATFORM_RCR_FREE, (void **) &new);
    uint32_t nfree = &RCR_WORD(FLASH_SECTOR_WORDS) - new;

    // Is there enough room to fit the rec in the RCR page?
    if (nwords < nfree) {  // Note inequality needed to leave at least one all set word
        uint32_t addr = platform_flash_mapped2phys((uint32_t)&new[-1]);
        platform_s_flash_write(rec, addr, reclen);

        if (prev) { // If a previous exists, then overwrite the hdr as DELETED
            platform_rcr_t rcr = {0};
            addr = platform_flash_mapped2phys((uint32_t)&prev[-1]);
            rcr.id = PLATFORM_RCR_DELETED; rcr.len = np/WORDSIZE;
            platform_s_flash_write(&rcr, addr, WORDSIZE);
        }

    } else {
        platform_rcr_t *rcr = (platform_rcr_t *) &RCR_WORD(0), newrcr = {0};
        uint32_t flash_addr = platform_flash_mapped2phys((uint32_t)&RCR_WORD(0));
        uint32_t *buf, i, l, pass;

        for (pass = 1; pass <= 2; pass++)  {
            for (i = 0, l = 0; i < FLASH_SECTOR_WORDS - nfree; ) {
                platform_rcr_t r = rcr[i]; // again avoid unaligned exceptions
                if (r.id == PLATFORM_RCR_FREE)
                    break;
                if (r.id != PLATFORM_RCR_DELETED && r.id != rec_id) {
                    if (pass == 2) memcpy(buf + l, rcr + i, (r.len + 1)*WORDSIZE);
                    l += r.len + 1;
                }
                i += r.len + 1;
            }
            if (pass == 2) memcpy(buf + l, rec, reclen);
            l += nwords + 1;
            if (pass == 1) buf = malloc(l * WORDSIZE);

            if (l >= FLASH_SECTOR_WORDS || !buf)
                return ~0;
        }
        platform_flash_erase_sector(flash_addr/INTERNAL_FLASH_SECTOR_SIZE);
        platform_s_flash_write(buf, flash_addr, l*WORDSIZE);
        free(buf);
    }
    return nwords*WORDSIZE;
}

void* platform_print_deprecation_note( const char *msg, const char *time_frame)
{
  printf( "Warning, deprecated API! %s. It will be removed %s. See documentation for details.\n", msg, time_frame );
}

#define TH_MONIKER 0x68680000
#define TH_MASK    0xFFF80000
#define TH_UNMASK  (~TH_MASK)
#define TH_SHIFT   2
#define TH_ALLOCATION_BRICK 4   // must be a power of 2
#define TASK_DEFAULT_QUEUE_LEN 8
#define TASK_PRIORITY_MASK    3
#define TASK_PRIORITY_COUNT   3

/*
 * Private struct to hold the 3 event task queues and the dispatch callbacks
 */
static struct taskQblock {
  os_event_t *task_Q[TASK_PRIORITY_COUNT];
  platform_task_callback_t *task_func;
  int task_count;
  } TQB = {0};

static void platform_task_dispatch (os_event_t *e) {
  platform_task_handle_t handle = e->sig;
  if ( (handle & TH_MASK) == TH_MONIKER) {
    uint16_t entry    = (handle & TH_UNMASK) >> TH_SHIFT;
    uint8_t  priority = handle & TASK_PRIORITY_MASK;
    if ( priority <= PLATFORM_TASK_PRIORITY_HIGH &&
         TQB.task_func &&
         entry < TQB.task_count ){
      /* call the registered task handler with the specified parameter and priority */
      TQB.task_func[entry](e->par, priority);
      return;
    }
  }
  /* Invalid signals are ignored */
  NODE_DBG ( "Invalid signal issued: %08x",  handle);
}

/*
 * Initialise the task handle callback for a given priority.
 */
static int task_init_handler (void) {
  int p, qlen = TASK_DEFAULT_QUEUE_LEN;
  for (p = 0; p < TASK_PRIORITY_COUNT; p++){
    TQB.task_Q[p] = (os_event_t *) malloc( sizeof(os_event_t)*qlen );
    if (TQB.task_Q[p]) {
      os_memset(TQB.task_Q[p], 0, sizeof(os_event_t)*qlen);
      system_os_task(platform_task_dispatch, p, TQB.task_Q[p], TASK_DEFAULT_QUEUE_LEN);
    } else {
      NODE_DBG ( "Malloc failure in platform_task_init_handler" );
      return PLATFORM_ERR;
    }
  }
}


/*
 * Allocate a task handle in the relevant TCB.task_Q.  Note that these Qs are resized
 * as needed growing in 4 unit bricks.  No GC is adopted so handles are permanently
 * allocated during boot life.  This isn't an issue in practice as only a few handles
 * are created per priority during application init and the more volitile Lua tasks
 * are allocated in the Lua registery using the luaX interface which is layered on
 * this mechanism.
 */
platform_task_handle_t platform_task_get_id (platform_task_callback_t t) {
  if ( (TQB.task_count & (TH_ALLOCATION_BRICK - 1)) == 0 ) {
    TQB.task_func = (platform_task_callback_t *) realloc(
        TQB.task_func,
        sizeof(platform_task_callback_t) * (TQB.task_count+TH_ALLOCATION_BRICK));
    if (!TQB.task_func) {
      NODE_DBG ( "Malloc failure in platform_task_get_id");
      return 0;
    }
    os_memset (TQB.task_func+TQB.task_count, 0,
               sizeof(platform_task_callback_t)*TH_ALLOCATION_BRICK);
  }
  TQB.task_func[TQB.task_count++] = t;
  return TH_MONIKER + ((TQB.task_count-1) << TH_SHIFT);
}
