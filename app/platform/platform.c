// Platform-dependent functions and includes

#include "platform.h"
#include "common.h"
#include "c_stdio.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "llimits.h"
#include "gpio.h"
#include "user_interface.h"
#include "driver/gpio16.h"
#include "driver/i2c_master.h"
#include "driver/spi.h"
#include "driver/uart.h"
#include "driver/sigma_delta.h"

#define INTERRUPT_TYPE_IS_LEVEL(x)   ((x) >= GPIO_PIN_INTR_LOLEVEL)

#ifdef GPIO_INTERRUPT_ENABLE
static task_handle_t gpio_task_handle;

#ifdef GPIO_INTERRUPT_HOOK_ENABLE
struct gpio_hook_entry {
  platform_hook_function func;
  uint32_t               bits;
};
struct gpio_hook {
  struct gpio_hook_entry *entry;
  uint32_t                all_bits;
  uint32_t                count;
};

static struct gpio_hook platform_gpio_hook;
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

int platform_init()
{
  // Setup the various forward and reverse mappings for the pins
  get_pin_map();

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
static void NO_INTR_CODE set_gpio_no_interrupt(uint8 pin, uint8_t push_pull) {
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
static void NO_INTR_CODE set_gpio_interrupt(uint8 pin) {
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
  uint32 j=0;
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  uint32 now = system_get_time();
  UNUSED(dummy);

#ifdef GPIO_INTERRUPT_HOOK_ENABLE
  if (gpio_status & platform_gpio_hook.all_bits) {
    for (j = 0; j < platform_gpio_hook.count; j++) {
       if (gpio_status & platform_gpio_hook.entry[j].bits)
         gpio_status = (platform_gpio_hook.entry[j].func)(gpio_status);
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
          uint32 level = 0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(j));
	  if (!task_post_high (gpio_task_handle, (now << 8) + (i<<1) + level)) {
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

void platform_gpio_init( task_handle_t gpio_task )
{
  gpio_task_handle = gpio_task;

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
  struct gpio_hook nh, oh = platform_gpio_hook;
  int i, j;

  if (!hook) {
    // Cannot register or unregister null hook
    return 0;
  }

  int delete_slot = -1;

  // If hook already registered, just update the bits
  for (i=0; i<oh.count; i++) {
    if (hook == oh.entry[i].func) {
      if (!bits) {
	// Unregister if move to zero bits
	delete_slot = i;
	break;
      }
      if (bits & (oh.all_bits & ~oh.entry[i].bits)) {
	// Attempt to hook an already hooked bit
	return 0;
      }
      // Update the hooked bits (in the right order)
      uint32_t old_bits = oh.entry[i].bits;
      *(volatile uint32_t *) &oh.entry[i].bits = bits;
      *(volatile uint32_t *) &oh.all_bits = (oh.all_bits & ~old_bits) | bits;
      return 1;
    }
  }

  // This must be the register new hook / delete old hook
  
  if (delete_slot < 0) {
    if (bits & oh.all_bits) {
      return 0;   // Attempt to hook already hooked bits
    }
    nh.count = oh.count + 1;    // register a new hook
  } else {
    nh.count = oh.count - 1;    // unregister an old hook
  }

  // These return NULL if the count = 0 so only error check if > 0)
  nh.entry = c_malloc( nh.count * sizeof(*(nh.entry)) );
  if (nh.count && !(nh.entry)) {
    return 0;  // Allocation failure
  }

  for (i=0, j=0; i<oh.count; i++) {
    // Don't copy if this is the entry to delete
    if (i != delete_slot) {
      nh.entry[j++]   = oh.entry[i];
    }
  }

  if (delete_slot < 0) { // for a register add the hook to the tail and set the all bits
    nh.entry[j].bits  = bits;
    nh.entry[j].func  = hook;
    nh.all_bits = oh.all_bits | bits;
  } else {    // for an unregister clear the matching all bits
    nh.all_bits = oh.all_bits & (~oh.entry[delete_slot].bits);
  }

  ETS_GPIO_INTR_DISABLE();
  // This is a structure copy, so interrupts need to be disabled
  platform_gpio_hook = nh;
  ETS_GPIO_INTR_ENABLE();

  c_free(oh.entry);
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

  i2c_master_gpio_init(sda, scl);
  return PLATFORM_I2C_SPEED_SLOW;
}

void platform_i2c_send_start( unsigned id ){
  i2c_master_start();
}

void platform_i2c_send_stop( unsigned id ){
  i2c_master_stop();
}

int platform_i2c_send_address( unsigned id, uint16_t address, int direction ){
  // Convert enum codes to R/w bit value.
  // If TX == 0 and RX == 1, this test will be removed by the compiler
  if ( ! ( PLATFORM_I2C_DIRECTION_TRANSMITTER == 0 &&
           PLATFORM_I2C_DIRECTION_RECEIVER == 1 ) ) {
    direction = ( direction == PLATFORM_I2C_DIRECTION_TRANSMITTER ) ? 0 : 1;
  }

  i2c_master_writeByte( (uint8_t) ((address << 1) | direction ));
  // Low-level returns nack (0=acked); we return ack (1=acked).
  return ! i2c_master_getAck();
}

int platform_i2c_send_byte( unsigned id, uint8_t data ){
  i2c_master_writeByte(data);
  // Low-level returns nack (0=acked); we return ack (1=acked).
  return ! i2c_master_getAck();
}

int platform_i2c_recv_byte( unsigned id, int ack ){
  uint8_t r = i2c_master_readByte();
  i2c_master_setAck( !ack );
  return r;
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
    apbuf = (uint32_t *)c_malloc(size);
    if(!apbuf)
      return 0;
    c_memcpy(apbuf, from, size);
  }
  system_soft_wdt_feed ();
  r = flash_write(toaddr, apbuf?(uint32 *)apbuf:(uint32 *)from, size);
  if(apbuf)
    c_free(apbuf);
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
    uint32* to2=(uint32*)((((uint32_t)to)&(~blkmask))+INTERNAL_FLASH_READ_UNIT_SIZE);
    r = flash_read(fromaddr, to2, size2);
    if(SPI_FLASH_RESULT_OK == r)
    {
      os_memmove(to,to2,size2);
      char back[ INTERNAL_FLASH_READ_UNIT_SIZE ] __attribute__ ((aligned(INTERNAL_FLASH_READ_UNIT_SIZE)));
      r=flash_read(fromaddr+size2,(uint32*)back,INTERNAL_FLASH_READ_UNIT_SIZE);
      os_memcpy((uint8_t*)to+size2,back,INTERNAL_FLASH_READ_UNIT_SIZE);
    }
  }
  else
    r = flash_read(fromaddr, (uint32 *)to, size);

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
  system_soft_wdt_feed ();
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

uint32_t platform_flash_mapped2phys (uint32_t mapped_addr)  {
  uint32_t meg = flash_map_meg_offset();
  return (meg&1) ? -1 : mapped_addr - INTERNAL_FLASH_MAPPED_ADDRESS + meg ;
}

uint32_t platform_flash_phys2mapped (uint32_t phys_addr) {
  uint32_t meg = flash_map_meg_offset();
  return (meg&1) ? -1 : phys_addr + INTERNAL_FLASH_MAPPED_ADDRESS - meg;
}

void* platform_print_deprecation_note( const char *msg, const char *time_frame)
{
  c_printf( "Warning, deprecated API! %s. It will be removed %s. See documentation for details.\n", msg, time_frame );
}
