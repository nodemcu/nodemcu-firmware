// Platform-dependent functions

#include "platform.h"
#include "c_stdio.h"
#include "c_string.h"
#include "c_stdlib.h"
#include "gpio.h"
#include "user_interface.h"
#include "driver/uart.h"
// Platform specific includes

static void pwms_init();

int platform_init()
{
  // Setup PWMs
  pwms_init();

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
#ifdef GPIO_INTERRUPT_ENABLE
extern void lua_gpio_unref(unsigned pin);
#endif
int platform_gpio_mode( unsigned pin, unsigned mode, unsigned pull )
{
  // NODE_DBG("Function platform_gpio_mode() is called. pin_mux:%d, func:%d\n",pin_mux[pin],pin_func[pin]);
  if (pin >= NUM_GPIO)
    return -1;
  if(pin == 0){
    if(mode==PLATFORM_GPIO_INPUT)
      gpio16_input_conf();
    else
      gpio16_output_conf();
    return 1;
  }

  platform_pwm_close(pin);    // closed from pwm module, if it is used in pwm

  switch(pull){
    case PLATFORM_GPIO_PULLUP:
      PIN_PULLDWN_DIS(pin_mux[pin]);
      PIN_PULLUP_EN(pin_mux[pin]);
      break;
    case PLATFORM_GPIO_PULLDOWN:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_EN(pin_mux[pin]);
      break;
    case PLATFORM_GPIO_FLOAT:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_DIS(pin_mux[pin]);
      break;
    default:
      PIN_PULLUP_DIS(pin_mux[pin]);
      PIN_PULLDWN_DIS(pin_mux[pin]);
      break;
  }

  switch(mode){
    case PLATFORM_GPIO_INPUT:
#ifdef GPIO_INTERRUPT_ENABLE
      lua_gpio_unref(pin);    // unref the lua ref call back.
#endif
      GPIO_DIS_OUTPUT(pin_num[pin]);
    case PLATFORM_GPIO_OUTPUT:
      ETS_GPIO_INTR_DISABLE();
#ifdef GPIO_INTERRUPT_ENABLE
      pin_int_type[pin] = GPIO_PIN_INTR_DISABLE;
#endif
      PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
      //disable interrupt
      gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), GPIO_PIN_INTR_DISABLE);
      //clear interrupt status
      GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin_num[pin]));
      GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin]))) & (~ GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE))); //disable open drain; 
      ETS_GPIO_INTR_ENABLE();
      break;
#ifdef GPIO_INTERRUPT_ENABLE
    case PLATFORM_GPIO_INT:
      ETS_GPIO_INTR_DISABLE();
      PIN_FUNC_SELECT(pin_mux[pin], pin_func[pin]);
      GPIO_DIS_OUTPUT(pin_num[pin]);
      gpio_register_set(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_num[pin])), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                        | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                        | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
      ETS_GPIO_INTR_ENABLE();
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
static void platform_gpio_intr_dispatcher( platform_gpio_intr_handler_fn_t cb){
  uint8 i, level;
  uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  for (i = 0; i < GPIO_PIN_NUM; i++) {
    if (pin_int_type[i] && (gpio_status & BIT(pin_num[i])) ) {
      //disable interrupt
      gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[i]), GPIO_PIN_INTR_DISABLE);
      //clear interrupt status
      GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(pin_num[i]));
      level = 0x1 & GPIO_INPUT_GET(GPIO_ID_PIN(pin_num[i]));
      if(cb){
        cb(i, level);
      }
      gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[i]), pin_int_type[i]);
    }
  }
}

void platform_gpio_init( platform_gpio_intr_handler_fn_t cb )
{
  ETS_GPIO_INTR_ATTACH(platform_gpio_intr_dispatcher, cb);
}

int platform_gpio_intr_init( unsigned pin, GPIO_INT_TYPE type )
{
  if (pin >= NUM_GPIO)
    return -1;
  ETS_GPIO_INTR_DISABLE();
  //clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pin_num[pin]));
  pin_int_type[pin] = type;
  //enable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(pin_num[pin]), type);
  ETS_GPIO_INTR_ENABLE();
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
    case PLATFORM_UART_STOPBITS_1:
      UartDev.stop_bits = ONE_STOP_BIT;
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
      break;
    case PLATFORM_UART_PARITY_ODD:
      UartDev.parity = ODD_BITS;
      break;
    default:
      UartDev.parity = NONE_BITS;
      break;
  }

  uart_setup(id);

  return baud;
}

// Send: version with and without mux
void platform_uart_send( unsigned id, u8 data ) 
{
  uart_tx_one_char(id, data);
}

// ****************************************************************************
// PWMs

static uint16_t pwms_duty[NUM_PWM] = {0};

static void pwms_init()
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
  pwm_start();
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

void platform_pwm_start( unsigned pin )
{
  // NODE_DBG("Function platform_pwm_start() is called.\n");
  if ( pin < NUM_PWM)
  {
    if(!pwm_exist(pin))
      return;
    pwm_set_duty(DUTY(pwms_duty[pin]), pin);
    pwm_start();
  }
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
uint32_t platform_spi_setup( unsigned id, int mode, unsigned cpol, unsigned cpha, unsigned databits, uint32_t clock)
{
  spi_master_init(id, cpol, cpha, databits, clock);
  return 1;
}

spi_data_type platform_spi_send_recv( unsigned id, spi_data_type data )
{
  spi_mast_byte_write(id, &data);
  return data;
}

// ****************************************************************************
// Flash access functions

uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
  toaddr -= INTERNAL_FLASH_START_ADDRESS;
  SpiFlashOpResult r;
  const uint32_t blkmask = INTERNAL_FLASH_WRITE_UNIT_SIZE - 1;
  uint32_t *apbuf = NULL;
  if( ((uint32_t)from) & blkmask ){
    apbuf = (uint32_t *)c_malloc(size);
    if(!apbuf)
      return 0;
    c_memcpy(apbuf, from, size);
  }
  WRITE_PERI_REG(0x60000914, 0x73);
  r = flash_write(toaddr, apbuf?(uint32 *)apbuf:(uint32 *)from, size);
  if(apbuf)
    c_free(apbuf);
  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_write: r=%d at %08X\n", ( int )r, ( unsigned )toaddr+INTERNAL_FLASH_START_ADDRESS );
    return 0;
  }
}

uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
  fromaddr -= INTERNAL_FLASH_START_ADDRESS;
  SpiFlashOpResult r;
  WRITE_PERI_REG(0x60000914, 0x73);
  r = flash_read(fromaddr, (uint32 *)to, size);
  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_read: r=%d at %08X\n", ( int )r, ( unsigned )fromaddr+INTERNAL_FLASH_START_ADDRESS );
    return 0;
  }
}

int platform_flash_erase_sector( uint32_t sector_id )
{
  WRITE_PERI_REG(0x60000914, 0x73);
  return flash_erase( sector_id ) == SPI_FLASH_RESULT_OK ? PLATFORM_OK : PLATFORM_ERR;
}
