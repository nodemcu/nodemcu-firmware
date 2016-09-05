
#include "platform.h"
#include "driver/spi.h"
#include "c_types.h"

#include "sdcard.h"


#define CHECK_SSPIN(pin) \
  if (pin < 1 || pin > NUM_GPIO) return FALSE; \
  m_ss_pin = pin;


//==============================================================================
// SD card commands
/** GO_IDLE_STATE - init card in spi mode if CS low */
uint8_t const CMD0 = 0X00;
/** SEND_IF_COND - verify SD Memory Card interface operating condition.*/
uint8_t const CMD8 = 0X08;
/** SEND_CSD - read the Card Specific Data (CSD register) */
uint8_t const CMD9 = 0X09;
/** SEND_CID - read the card identification information (CID register) */
uint8_t const CMD10 = 0X0A;
/** STOP_TRANSMISSION - end multiple block read sequence */
uint8_t const CMD12 = 0X0C;
/** SEND_STATUS - read the card status register */
uint8_t const CMD13 = 0X0D;
/** READ_SINGLE_BLOCK - read a single data block from the card */
uint8_t const CMD17 = 0X11;
/** READ_MULTIPLE_BLOCK - read a multiple data blocks from the card */
uint8_t const CMD18 = 0X12;
/** WRITE_BLOCK - write a single data block to the card */
uint8_t const CMD24 = 0X18;
/** WRITE_MULTIPLE_BLOCK - write blocks of data until a STOP_TRANSMISSION */
uint8_t const CMD25 = 0X19;
/** ERASE_WR_BLK_START - sets the address of the first block to be erased */
uint8_t const CMD32 = 0X20;
/** ERASE_WR_BLK_END - sets the address of the last block of the continuous
    range to be erased*/
uint8_t const CMD33 = 0X21;
/** ERASE - erase all previously selected blocks */
uint8_t const CMD38 = 0X26;
/** APP_CMD - escape for application specific command */
uint8_t const CMD55 = 0X37;
/** READ_OCR - read the OCR register of a card */
uint8_t const CMD58 = 0X3A;
/** CRC_ON_OFF - enable or disable CRC checking */
uint8_t const CMD59 = 0X3B;
/** SET_WR_BLK_ERASE_COUNT - Set the number of write blocks to be
     pre-erased before writing */
uint8_t const ACMD23 = 0X17;
/** SD_SEND_OP_COMD - Sends host capacity support information and
    activates the card's initialization process */
uint8_t const ACMD41 = 0X29;
//==============================================================================
/** status for card in the ready state */
uint8_t const R1_READY_STATE = 0X00;
/** status for card in the idle state */
uint8_t const R1_IDLE_STATE = 0X01;
/** status bit for illegal command */
uint8_t const R1_ILLEGAL_COMMAND = 0X04;
/** start data token for read or write single block*/
uint8_t const DATA_START_BLOCK = 0XFE;
/** stop token for write multiple blocks*/
uint8_t const STOP_TRAN_TOKEN = 0XFD;
/** start data token for write multiple blocks*/
uint8_t const WRITE_MULTIPLE_TOKEN = 0XFC;
/** mask for data response tokens after a write block operation */
uint8_t const DATA_RES_MASK = 0X1F;
/** write data accepted token */
uint8_t const DATA_RES_ACCEPTED = 0X05;

//------------------------------------------------------------------------------
// SD card errors
/** timeout error for command CMD0 (initialize card in SPI mode) */
uint8_t const SD_CARD_ERROR_CMD0 = 0X1;
/** CMD8 was not accepted - not a valid SD card*/
uint8_t const SD_CARD_ERROR_CMD8 = 0X2;
/** card returned an error response for CMD12 (stop multiblock read) */
uint8_t const SD_CARD_ERROR_CMD12 = 0X3;
/** card returned an error response for CMD17 (read block) */
uint8_t const SD_CARD_ERROR_CMD17 = 0X4;
/** card returned an error response for CMD18 (read multiple block) */
uint8_t const SD_CARD_ERROR_CMD18 = 0X5;
/** card returned an error response for CMD24 (write block) */
uint8_t const SD_CARD_ERROR_CMD24 = 0X6;
/**  WRITE_MULTIPLE_BLOCKS command failed */
uint8_t const SD_CARD_ERROR_CMD25 = 0X7;
/** card returned an error response for CMD58 (read OCR) */
uint8_t const SD_CARD_ERROR_CMD58 = 0X8;
/** SET_WR_BLK_ERASE_COUNT failed */
uint8_t const SD_CARD_ERROR_ACMD23 = 0X9;
/** ACMD41 initialization process timeout */
uint8_t const SD_CARD_ERROR_ACMD41 = 0XA;
/** card returned a bad CSR version field */
uint8_t const SD_CARD_ERROR_BAD_CSD = 0XB;
/** erase block group command failed */
uint8_t const SD_CARD_ERROR_ERASE = 0XC;
/** card not capable of single block erase */
uint8_t const SD_CARD_ERROR_ERASE_SINGLE_BLOCK = 0XD;
/** Erase sequence timed out */
uint8_t const SD_CARD_ERROR_ERASE_TIMEOUT = 0XE;
/** card returned an error token instead of read data */
uint8_t const SD_CARD_ERROR_READ = 0XF;
/** read CID or CSD failed */
uint8_t const SD_CARD_ERROR_READ_REG = 0X10;
/** timeout while waiting for start of read data */
uint8_t const SD_CARD_ERROR_READ_TIMEOUT = 0X11;
/** card did not accept STOP_TRAN_TOKEN */
uint8_t const SD_CARD_ERROR_STOP_TRAN = 0X12;
/** card returned an error token as a response to a write operation */
uint8_t const SD_CARD_ERROR_WRITE = 0X13;
/** attempt to write protected block zero */
uint8_t const SD_CARD_ERROR_WRITE_BLOCK_ZERO = 0X14;  // REMOVE - not used
/** card did not go ready for a multiple block write */
uint8_t const SD_CARD_ERROR_WRITE_MULTIPLE = 0X15;
/** card returned an error to a CMD13 status check after a write */
uint8_t const SD_CARD_ERROR_WRITE_PROGRAMMING = 0X16;
/** timeout occurred during write programming */
uint8_t const SD_CARD_ERROR_WRITE_TIMEOUT = 0X17;
/** incorrect rate selected */
uint8_t const SD_CARD_ERROR_SCK_RATE = 0X18;
/** init() not called */
uint8_t const SD_CARD_ERROR_INIT_NOT_CALLED = 0X19;
/** card returned an error for CMD59 (CRC_ON_OFF) */
uint8_t const SD_CARD_ERROR_CMD59 = 0X1A;
/** invalid read CRC */
uint8_t const SD_CARD_ERROR_READ_CRC = 0X1B;
/** SPI DMA error */
uint8_t const SD_CARD_ERROR_SPI_DMA = 0X1C;
//------------------------------------------------------------------------------
// card types
uint8_t const SD_CARD_TYPE_INVALID = 0;
/** Standard capacity V1 SD card */
uint8_t const SD_CARD_TYPE_SD1  = 1;
/** Standard capacity V2 SD card */
uint8_t const SD_CARD_TYPE_SD2  = 2;
/** High Capacity SD card */
uint8_t const SD_CARD_TYPE_SDHC = 3;


typedef struct {
  uint32_t start, target;
} to_t;

static uint8_t m_spi_no, m_ss_pin, m_status, m_type, m_error;

static void sdcard_chipselect_low( void ) {
  platform_gpio_write( m_ss_pin, PLATFORM_GPIO_LOW );
}

static void sdcard_chipselect_high( void ) {
  platform_gpio_write( m_ss_pin, PLATFORM_GPIO_HIGH );
  // send some cc to ensure that MISO returns to high
  platform_spi_send_recv( m_spi_no, 8, 0xff );
}

static void set_timeout( to_t *to, uint32_t us )
{
  uint32_t offset;

  to->start = system_get_time();

  offset = 0xffffffff - to->start;
  if (offset > us) {
    to->target = us - offset;
  } else {
    to->target = to->start + us;
  }
}

static uint8_t timed_out( to_t *to )
{
  uint32_t now = system_get_time();

  if (to->start < to->target) {
    if ((now >= to->start) && (now <= to->target)) {
      return FALSE;
    } else {
      return TRUE;
    }
  } else {
    if ((now >= to->start) || (now <= to->target)) {
      return FALSE;
    } else {
      return TRUE;
    }
  }
}

static int sdcard_wait_not_busy( uint32_t us )
{
  to_t to;

  set_timeout( &to, us );
  while (platform_spi_send_recv( m_spi_no, 8, 0xff ) != 0xff) {
    if (timed_out( &to )) {
      goto fail;
    }
  }
  return TRUE;

  fail:
  return FALSE;
}

static uint8_t sdcard_command( uint8_t cmd, uint32_t arg )
{
  sdcard_chipselect_low();

  // wait until card is busy
  sdcard_wait_not_busy( 100 * 1000 );

  // send command
  // with precalculated CRC - correct for CMD0 with arg zero or CMD8 with arg 0x1AA
  const uint8_t crc = cmd == CMD0 ? 0x95 : 0x87;
  platform_spi_transaction( m_spi_no, 16, (cmd | 0x40) << 8 | arg >> 24, 32, arg << 8 | crc, 0, 0, 0 );

  // skip dangling byte of data transfer
  if (cmd == CMD12) {
    platform_spi_transaction( m_spi_no, 8, 0xff, 0, 0, 0, 0, 0 );
  }

  // wait for response
  for (uint8_t i = 0; ((m_status = platform_spi_send_recv( m_spi_no, 8, 0xff )) & 0x80) && i != 0xFF; i++) ;

  return m_status;
}

static uint8_t sdcard_acmd( uint8_t cmd, uint32_t arg ) {
  sdcard_command( CMD55, 0 );
  return sdcard_command( cmd, arg );
}

static int sdcard_write_data( uint8_t token, const uint8_t *src)
{
  uint16_t crc = 0xffff;

  platform_spi_transaction( m_spi_no, 8, token, 0, 0, 0, 0, 0 );
  platform_spi_blkwrite( m_spi_no, 512, src );
  platform_spi_transaction( m_spi_no, 16, crc, 0, 0, 0, 0, 0 );

  m_status = platform_spi_send_recv( m_spi_no, 8, 0xff );
  if ((m_status & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
    m_error = SD_CARD_ERROR_WRITE;
    goto fail;
  }
  return TRUE;

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

static int sdcard_read_data( uint8_t *dst, size_t count )
{
  to_t to;

  // wait for start block token
  set_timeout( &to, 100 * 1000 );
  while ((m_status = platform_spi_send_recv( m_spi_no, 8, 0xff)) == 0xff) {
    if (timed_out( &to )) {
      goto fail;
    }
  }

  if (m_status != DATA_START_BLOCK) {
    m_error = SD_CARD_ERROR_READ;
    goto fail;
  }
  // transfer data
  platform_spi_blkread( m_spi_no, count, (void *)dst );

  // discard crc
  platform_spi_transaction( m_spi_no, 16, 0xffff, 0, 0, 0, 0, 0 );

  sdcard_chipselect_high();
  return TRUE;

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

static int sdcard_read_register( uint8_t cmd, uint8_t *buf )
{
  if (sdcard_command( cmd, 0 )) {
    m_error = SD_CARD_ERROR_READ_REG;
    goto fail;
  }
  return sdcard_read_data( buf, 16 );

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

int platform_sdcard_init( uint8_t spi_no, uint8_t ss_pin )
{
  uint32_t arg, user_spi_clkdiv;
  to_t to;

  m_type = SD_CARD_TYPE_INVALID;
  m_error = 0;

  if (spi_no > 1) {
    return FALSE;
  }
  m_spi_no = spi_no;
  CHECK_SSPIN(ss_pin);

  platform_gpio_write( m_ss_pin, PLATFORM_GPIO_HIGH );
  platform_gpio_mode( m_ss_pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT );

  // set SPI clock to 400 kHz for init phase
  user_spi_clkdiv = spi_set_clkdiv( m_spi_no, 200 );

  // apply initialization sequence:
  // keep ss and io high, apply clock for max(1ms; 74cc)
  // 1ms requires 400cc @ 400kHz
  for (int i = 0; i < 2; i++) {
    platform_spi_transaction( m_spi_no, 0, 0, 0, 0, 0, 200, 0 );
  }

  // command to go idle in SPI mode
  set_timeout( &to, 500 * 1000 );
  while (sdcard_command( CMD0, 0 ) != R1_IDLE_STATE) {
    if (timed_out( &to )) {
      goto fail;
    }
  }

  set_timeout( &to, 500 * 1000 );
  while (1) {
    if (sdcard_command( CMD8, 0x1aa) == (R1_ILLEGAL_COMMAND | R1_IDLE_STATE)) {
      m_type = SD_CARD_TYPE_SD1;
      break;
    }
    for (uint8_t i = 0; i < 4; i++) {
      m_status = platform_spi_send_recv( m_spi_no, 8, 0xff );
    }
    if (m_status == 0xaa) {
      m_type = SD_CARD_TYPE_SD2;
      break;
    }
    if (timed_out( &to )) {
      goto fail;
    }
  }
  // initialize card and send host supports SDHC if SD2
  arg = m_type == SD_CARD_TYPE_SD2 ? 0x40000000 : 0;

  set_timeout( &to, 500 * 1000 );
  while (sdcard_acmd( ACMD41, arg ) != R1_READY_STATE) {
    if (timed_out( &to )) {
      goto fail;
    }
  }
  // if SD2 read OCR register to check for SDHC card
  if (m_type == SD_CARD_TYPE_SD2) {
    if (sdcard_command( CMD58, 0 )) {
      m_error = SD_CARD_ERROR_CMD58;
      goto fail;
    }
    if ((platform_spi_send_recv( m_spi_no, 8, 0xff ) & 0xC0) == 0xC0) {
      m_type = SD_CARD_TYPE_SDHC;
    }
    // Discard rest of ocr - contains allowed voltage range.
    for (uint8_t i = 0; i < 3; i++) {
      platform_spi_send_recv( m_spi_no, 8, 0xff);
    }
  }
  sdcard_chipselect_high();

  // re-apply user's spi clock divider
  spi_set_clkdiv( m_spi_no, user_spi_clkdiv );

  return TRUE;

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

int platform_sdcard_status( void )
{
  return m_status;
}

int platform_sdcard_error( void )
{
  return m_error;
}

int platform_sdcard_type( void )
{
  return m_type;
}

int platform_sdcard_read_block( uint8_t ss_pin, uint32_t block, uint8_t *dst )
{
  CHECK_SSPIN(ss_pin);

  // generate byte address for pre-SDHC types
  if (m_type != SD_CARD_TYPE_SDHC) {
    block <<= 9;
  }
  if (sdcard_command( CMD17, block )) {
    m_error = SD_CARD_ERROR_CMD17;
    goto fail;
  }
  return sdcard_read_data( dst, 512 );

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

int platform_sdcard_read_blocks( uint8_t ss_pin, uint32_t block, size_t num, uint8_t *dst )
{
  CHECK_SSPIN(ss_pin);

  if (num == 0) {
    return TRUE;
  }
  if (num == 1) {
    return platform_sdcard_read_block( ss_pin, block, dst );
  }

  // generate byte address for pre-SDHC types
  if (m_type != SD_CARD_TYPE_SDHC) {
    block <<= 9;
  }

  // command READ_MULTIPLE_BLOCKS
  if (sdcard_command( CMD18, block )) {
    m_error = SD_CARD_ERROR_CMD18;
    goto fail;
  }

  // read required blocks
  while (num > 0) {
    sdcard_chipselect_low();
    if (sdcard_read_data( dst, 512 )) {
      num--;
      dst = &(dst[512]);
    } else {
      break;
    }
  }

  // issue command STOP_TRANSMISSION
  if (sdcard_command( CMD12, 0 )) {
    m_error = SD_CARD_ERROR_CMD12;
    goto fail;
  }
  sdcard_chipselect_high();
  return TRUE;

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

int platform_sdcard_read_csd( uint8_t ss_pin, uint8_t *csd )
{
  CHECK_SSPIN(ss_pin);

  return sdcard_read_register( CMD9, csd );
}

int platform_sdcard_read_cid( uint8_t ss_pin, uint8_t *cid )
{
  CHECK_SSPIN(ss_pin);

  return sdcard_read_register( CMD10, cid );
}

int platform_sdcard_write_block( uint8_t ss_pin, uint32_t block, const uint8_t *src )
{
  CHECK_SSPIN(ss_pin);

  // generate byte address for pre-SDHC types
  if (m_type != SD_CARD_TYPE_SDHC) {
    block <<= 9;
  }
  if (sdcard_command( CMD24, block )) {
    m_error = SD_CARD_ERROR_CMD24;
    goto fail;
  }
  if (! sdcard_write_data( DATA_START_BLOCK, src )) {
    goto fail;
  }

  sdcard_chipselect_high();
  return TRUE;

  fail:
  sdcard_chipselect_high();
  return FALSE;
}

static int sdcard_write_stop( void )
{
  sdcard_chipselect_low();

  if (! sdcard_wait_not_busy( 100 * 1000 )) {
    goto fail;
  }
  platform_spi_transaction( m_spi_no, 8, STOP_TRAN_TOKEN, 0, 0, 0, 0, 0 );
  if (! sdcard_wait_not_busy( 100 * 1000 )) {
    goto fail;
  }

  sdcard_chipselect_high();
  return TRUE;

  fail:
  m_error = SD_CARD_ERROR_STOP_TRAN;
  sdcard_chipselect_high();
  return FALSE;
}

int platform_sdcard_write_blocks( uint8_t ss_pin, uint32_t block, size_t num, const uint8_t *src )
{
  CHECK_SSPIN(ss_pin);

  if (sdcard_acmd( ACMD23, num )) {
    m_error = SD_CARD_ERROR_ACMD23;
    goto fail;
  }
  // generate byte address for pre-SDHC types
  if (m_type != SD_CARD_TYPE_SDHC) {
    block <<= 9;
  }
  if (sdcard_command( CMD25, block )) {
    m_error = SD_CARD_ERROR_CMD25;
    goto fail;
  }
  sdcard_chipselect_high();

  for (size_t b = 0; b < num; b++, src += 512) {
    sdcard_chipselect_low();

    // wait for previous write to finish
    if (! sdcard_wait_not_busy( 100 * 1000 )) {
      goto fail_write;
    }
    if (! sdcard_write_data( WRITE_MULTIPLE_TOKEN, src )) {
      goto fail_write;
    }

    sdcard_chipselect_high();
  }

  return sdcard_write_stop();

  fail_write:
  m_error = SD_CARD_ERROR_WRITE_MULTIPLE;
  fail:
  sdcard_chipselect_high();
  return FALSE;
}
