#ifndef SPI_APP_H
#define SPI_APP_H

#include "spi_register.h"
#include "rom.h"
#include "osapi.h"
#include "uart.h"
#include "os_type.h"

/*SPI number define*/
#define SPI_SPI 		0
#define SPI_HSPI		1

#define SPI_ORDER_LSB 0
#define SPI_ORDER_MSB 1



//lcd drive function
void spi_lcd_mode_init(uint8 spi_no);
void spi_lcd_9bit_write(uint8 spi_no,uint8 high_bit,uint8 low_8bit);

//spi master init funtion
uint32_t spi_set_clkdiv(uint8 spi_no, uint32_t clock_div);
void spi_master_init(uint8 spi_no, unsigned cpol, unsigned cpha, uint32_t clock_div);
void spi_mast_byte_order(uint8 spi_no, uint8 order);
// blocked buffer write
void spi_mast_blkset(uint8 spi_no, size_t bitlen, const uint8 *data);
// blocked buffer read
void spi_mast_blkget(uint8 spi_no, size_t bitlen, uint8 *data);
// fill MOSI buffer
void spi_mast_set_mosi(uint8 spi_no, uint16 offset, uint8 bitlen, uint32 data);
// retrieve data from MISO buffer
uint32 spi_mast_get_miso(uint8 spi_no, uint16 offset, uint8 bitlen);
// initiate SPI transaction
void spi_mast_transaction(uint8 spi_no, uint8 cmd_bitlen, uint16 cmd_data, uint8 addr_bitlen, uint32 addr_data,
                          uint16 mosi_bitlen, uint8 dummy_bitlen, sint16 miso_bitlen);

//transmit data to esp8266 slave buffer,which needs 16bit transmission ,
//first byte is master command 0x04, second byte is master data
void spi_byte_write_espslave(uint8 spi_no,uint8 data);
//read data from esp8266 slave buffer,which needs 16bit transmission ,
//first byte is master command 0x06, second byte is to read slave data
void spi_byte_read_espslave(uint8 spi_no,uint8 *data);

 //esp8266 slave mode initial
void spi_slave_init(uint8 spi_no);
  //esp8266 slave isr handle funtion,tiggered when any transmission is finished.
  //the function is registered in spi_slave_init.
void spi_slave_isr_handler(void *para); 


//hspi test function, used to test esp8266 spi slave
void hspi_master_readwrite_repeat(void);


void ICACHE_FLASH_ATTR
    spi_test_init(void);


#endif

